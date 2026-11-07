// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_NOTIFICATION_RECOVERY_CONTROLLER_MAC_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_NOTIFICATION_RECOVERY_CONTROLLER_MAC_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/widget/widget_observer.h"

class BrowserWindowInterface;

namespace views {
class Widget;
}  // namespace views

namespace web_app {

// Observes navigation, active tab changes, and window visibility/activation on
// macOS to trigger the system notification permission bubble for installed web
// apps when notifications are allowed in Chrome, but not yet granted at the OS
// level without an explicit user denial (i.e. `kNotDetermined`,
// `kPromptPending`, or `kImplicitlyDenied`).
class WebAppNotificationRecoveryController
    : public content::WebContentsObserver,
      public views::WidgetObserver {
 public:
  static std::unique_ptr<WebAppNotificationRecoveryController> MaybeCreate(
      BrowserWindowInterface& browser,
      views::Widget* widget);

  // Returns whether `web_contents`'s primary main frame has a non-error,
  // non-opaque committed origin with `NOTIFICATIONS` permission granted in
  // Chrome, queried without recording a synthetic `NotificationsAccessed()` on
  // `PageSpecificContentSettings`.
  static bool HasChromeNotificationPermissionForOrigin(
      content::WebContents* web_contents);

  WebAppNotificationRecoveryController(BrowserWindowInterface& browser,
                                       views::Widget& widget);
  WebAppNotificationRecoveryController(
      const WebAppNotificationRecoveryController&) = delete;
  WebAppNotificationRecoveryController& operator=(
      const WebAppNotificationRecoveryController&) = delete;
  ~WebAppNotificationRecoveryController() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  void OnActiveTabChanged(BrowserWindowInterface* browser);
  void OnPaintAsActiveChanged();
  void MaybeTriggerRecoveryBubble();

  raw_ptr<views::Widget> widget_;
  base::CallbackListSubscription active_tab_subscription_;
  base::CallbackListSubscription paint_as_active_subscription_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_NOTIFICATION_RECOVERY_CONTROLLER_MAC_H_
