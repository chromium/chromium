// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_notification_recovery_controller_mac.h"

#include <optional>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model_states.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "ui/views/widget/widget.h"
#include "url/origin.h"

namespace web_app {

// static
std::unique_ptr<WebAppNotificationRecoveryController>
WebAppNotificationRecoveryController::MaybeCreate(
    BrowserWindowInterface& browser,
    views::Widget* widget) {
  if (!widget || !AreWebAppsEnabled(browser.GetProfile()) ||
      !UseNotificationAttributionForWebAppShims() ||
      (browser.GetType() != BrowserWindowInterface::Type::TYPE_NORMAL &&
       browser.GetType() != BrowserWindowInterface::Type::TYPE_APP)) {
    return nullptr;
  }
  return std::make_unique<WebAppNotificationRecoveryController>(browser,
                                                                *widget);
}

// static
bool WebAppNotificationRecoveryController::
    HasChromeNotificationPermissionForOrigin(
        content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }
  content::RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();
  if (!main_rfh || main_rfh->IsErrorDocument()) {
    return false;
  }
  const url::Origin& origin = main_rfh->GetLastCommittedOrigin();
  if (origin.opaque()) {
    return false;
  }
  // Query by origin rather than `GetPermissionStatusForCurrentDocument()` so
  // checking permission status does not pass a `RenderFrameHost*` into
  // `NotificationPermissionContext::GetContentSettingStatusInternal()`, which
  // would record a synthetic `NotificationsAccessed()` on pages that have not
  // accessed notifications.
  return web_contents->GetBrowserContext()
             ->GetPermissionController()
             ->GetPermissionResultForOriginWithoutContext(
                 content::PermissionDescriptorUtil::
                     CreatePermissionDescriptorForPermissionType(
                         blink::PermissionType::NOTIFICATIONS),
                 origin, origin)
             .status == blink::mojom::PermissionStatus::GRANTED;
}

WebAppNotificationRecoveryController::WebAppNotificationRecoveryController(
    BrowserWindowInterface& browser,
    views::Widget& widget)
    : widget_(&widget),
      active_tab_subscription_(
          browser.RegisterActiveTabDidChange(base::BindRepeating(
              &WebAppNotificationRecoveryController::OnActiveTabChanged,
              base::Unretained(this)))),
      paint_as_active_subscription_(
          widget.RegisterPaintAsActiveChangedCallback(base::BindRepeating(
              &WebAppNotificationRecoveryController::OnPaintAsActiveChanged,
              base::Unretained(this)))) {
  widget_observation_.Observe(&widget);
}

WebAppNotificationRecoveryController::~WebAppNotificationRecoveryController() =
    default;

void WebAppNotificationRecoveryController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  // `WebAppTabHelper::ReadyToCommitNavigation` and
  // `PageSpecificContentSettings::WebContentsHandler::DidFinishNavigation`
  // (created early during tab initialization) have already updated the app ID
  // and created `PageSpecificContentSettings` for the new page by the time this
  // window-level observer runs.
  MaybeTriggerRecoveryBubble();
}

void WebAppNotificationRecoveryController::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visible) {
  // In RemoteCocoa App Shim windows, `OnPaintAsActiveChanged()` can fire before
  // `OnVisibilityChanged(true)` during `-[NSWindow makeKeyAndOrderFront:]`.
  if (!visible) {
    return;
  }
  MaybeTriggerRecoveryBubble();
}

void WebAppNotificationRecoveryController::OnWidgetDestroying(
    views::Widget* widget) {
  widget_observation_.Reset();
  paint_as_active_subscription_ = {};
  widget_ = nullptr;
}

void WebAppNotificationRecoveryController::OnActiveTabChanged(
    BrowserWindowInterface* browser) {
  // `Browser::OnActiveTabChanged` refreshes the location bar for the newly
  // active tab before `RegisterActiveTabDidChange` subscribers run, so calling
  // `SetNotificationsWasDeniedBecauseOfSystemPermission()` here triggers an
  // immediate follow-up `LocationBar::Update` on the newly active tab.
  Observe(browser->GetTabStripModel()->GetActiveWebContents());
  MaybeTriggerRecoveryBubble();
}

void WebAppNotificationRecoveryController::OnPaintAsActiveChanged() {
  // Closing a child permission prompt bubble retains its parent's
  // `PaintAsActiveLock` until destruction (https://crbug.com/1303549), so
  // accepting Chrome's prompt does not cause a `true -> false -> true` bounce.
  if (!widget_ || !widget_->ShouldPaintAsActive()) {
    return;
  }
  MaybeTriggerRecoveryBubble();
}

void WebAppNotificationRecoveryController::MaybeTriggerRecoveryBubble() {
  // Check `widget_` directly rather than `BrowserWindowInterface::IsActive()`,
  // which `BrowserView::Show()` / `Activate()` set optimistically before the
  // window actually becomes key on macOS.
  if (!widget_ || !widget_->IsVisible() || !widget_->ShouldPaintAsActive() ||
      !web_contents()) {
    return;
  }

  std::optional<webapps::AppId> app_id =
      WebAppTabHelper::GetAppIdForNotificationAttribution(web_contents());
  if (!app_id) {
    return;
  }

  using SystemPermissionStatus = AppShimRegistry::NotificationPermissionStatus;
  switch (
      AppShimRegistry::Get()->GetNotificationPermissionStatusForApp(*app_id)) {
    case SystemPermissionStatus::kNotDetermined:
    case SystemPermissionStatus::kPromptPending:
    case SystemPermissionStatus::kImplicitlyDenied:
      break;
    case SystemPermissionStatus::kGranted:
    case SystemPermissionStatus::kExplicitlyDenied:
      return;
  }

  if (!HasChromeNotificationPermissionForOrigin(web_contents())) {
    return;
  }

  auto* pscs = content_settings::PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  if (!pscs || pscs->notifications_was_denied_because_of_system_permission()) {
    return;
  }

  if (ContentSettingImageModelStates::Get(web_contents())
          ->BubbleWasAutoOpened(
              ContentSettingImageModel::ImageType::kNotifications)) {
    return;
  }

  // TODO(crbug.com/581969756): Also consider stopping keeping the
  // "Notifications off" label expanded once the auto-open limit is reached.
  if (AppShimRegistry::Get()->GetNotificationBubbleAutoOpenCountForApp(
          *app_id) >=
      features::kAppShimNotificationAttributionMaxAutoOpenBubbleCount.Get()) {
    return;
  }

  pscs->SetNotificationsWasDeniedBecauseOfSystemPermission();
  if (ContentSettingImageModelStates::Get(web_contents())
          ->BubbleWasAutoOpened(
              ContentSettingImageModel::ImageType::kNotifications)) {
    AppShimRegistry::Get()->IncrementNotificationBubbleAutoOpenCountForApp(
        *app_id);
  }
}

}  // namespace web_app
