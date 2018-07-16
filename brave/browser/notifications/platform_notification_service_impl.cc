// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE-CHROMIUM file.

#include "brave/browser/notifications/platform_notification_service_impl.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "brave/browser/brave_content_browser_client.h"
#include "brave/browser/brave_permission_manager.h"
#include "browser/notification.h"
#include "browser/notification_delegate.h"
#include "browser/notification_presenter.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/platform_notification_data.h"
#include "content/public/common/notification_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"

#include "atom/common/api/event_emitter_caller.h"
#include "atom/common/native_mate_converters/callback.h"
#include "atom/common/native_mate_converters/content_converter.h"
#include "atom/common/native_mate_converters/string16_converter.h"
#include "atom/common/node_includes.h"
#include "native_mate/converter.h"
#include "native_mate/dictionary.h"

namespace brave {

namespace {

class NotificationDelegate : public brightray::NotificationDelegate {
 public:
  NotificationDelegate()
    : brightray::NotificationDelegate()
    , render_process_id_(-1) {}
  explicit NotificationDelegate(const std::string& notification_id,
   int render_process_id)
    : brightray::NotificationDelegate(notification_id)
    , render_process_id_(render_process_id) {}
  virtual ~NotificationDelegate() {}

  void EmitEvent(const std::string& event) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (!isolate) {
      return;
    }

    node::Environment* env = node::Environment::GetCurrent(isolate);
    if (!env) {
      return;
    }

    mate::EmitEvent(isolate, env->process_object(), event, notificationId(),
      render_process_id_);
  }

  void NotificationDestroyed() override {
    EmitEvent("notification-destroyed");
  }

  void NotificationFailed() override {
    EmitEvent("notification-failed");
  }

  void NotificationClick() override {
    EmitEvent("notification-clicked");
  }

  void NotificationClosed() override {
    EmitEvent("notification-closed");
  }

  void NotificationDisplayed() override {
    EmitEvent("notification-displayed");
  }

 private:
  int render_process_id_;

  DISALLOW_COPY_AND_ASSIGN(NotificationDelegate);
};

void OnPermissionResponse(const base::Callback<void(bool)>& callback,
                          blink::mojom::PermissionStatus status) {
  if (status == blink::mojom::PermissionStatus::GRANTED)
    callback.Run(true);
  else
    callback.Run(false);
}

void OnWebNotificationAllowed(
    brightray::BrowserClient* browser_client,
    const SkBitmap& icon,
    const content::PlatformNotificationData& data,
    brightray::NotificationDelegate* delegate,
    bool allowed) {
  if (!allowed)
    return;
  auto presenter = browser_client->GetNotificationPresenter();
  if (!presenter)
    return;
  auto notification = presenter->CreateNotification(delegate);
  if (notification) {
    notification->Show(data.title, data.body, data.tag,
        data.icon, icon, data.silent);
  }
}

}  // namespace

// static
PlatformNotificationServiceImpl*
PlatformNotificationServiceImpl::GetInstance() {
  return base::Singleton<PlatformNotificationServiceImpl>::get();
}

PlatformNotificationServiceImpl::PlatformNotificationServiceImpl() {}

PlatformNotificationServiceImpl::~PlatformNotificationServiceImpl() {}

blink::mojom::PermissionStatus
  PlatformNotificationServiceImpl::CheckPermissionOnUIThread(
    content::BrowserContext* browser_context,
    const GURL& origin,
    int render_process_id) {
  return blink::mojom::PermissionStatus::GRANTED;
}

blink::mojom::PermissionStatus
  PlatformNotificationServiceImpl::CheckPermissionOnIOThread(
    content::ResourceContext* resource_context,
    const GURL& origin,
    int render_process_id) {
  return blink::mojom::PermissionStatus::GRANTED;
}

void PlatformNotificationServiceImpl::DisplayNotification(
    content::BrowserContext* browser_context,
    const std::string& notification_id,
    const GURL& origin,
    const content::PlatformNotificationData& notification_data,
    const content::NotificationResources& notification_resources,
    int render_process_id) {
  brightray::NotificationDelegate* delegate =
      new NotificationDelegate(notification_id, render_process_id);
  auto callback = base::Bind(&OnWebNotificationAllowed,
             BraveContentBrowserClient::Get(),
             notification_resources.notification_icon,
             notification_data,
             base::Passed(&delegate));

  auto permission_manager = browser_context->GetPermissionManager();
  // TODO(bridiver) user gesture
  permission_manager->RequestPermission(
      content::PermissionType::NOTIFICATIONS, NULL, origin, false,
        base::Bind(&OnPermissionResponse, callback));
}

void PlatformNotificationServiceImpl::DisplayPersistentNotification(
    content::BrowserContext* browser_context,
    const std::string& notification_id,
    const GURL& service_worker_origin,
    const GURL& origin,
    const content::PlatformNotificationData& notification_data,
    const content::NotificationResources& notification_resources) {
}

void PlatformNotificationServiceImpl::CloseNotification(
    content::BrowserContext* browser_context,
    const std::string& notification_id) {
  auto presenter =
    BraveContentBrowserClient::Get()->GetNotificationPresenter();
  if (!presenter)
    return;
  auto notification = presenter->lookupNotification(notification_id);
  if (!notification)
    return;
  notification->Dismiss();
}

void PlatformNotificationServiceImpl::ClosePersistentNotification(
    content::BrowserContext* browser_context,
    const std::string& notification_id) {
}

void PlatformNotificationServiceImpl::GetDisplayedNotifications(
    content::BrowserContext* browser_context,
    const DisplayedNotificationsCallback& callback) {
}

}  // namespace brave
