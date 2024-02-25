// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_coordinator.h"

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace web_app {

WebAppInstallDialogCoordinator::~WebAppInstallDialogCoordinator() {
  if (IsShowing()) {
    StopTracking();
  }
}

bool WebAppInstallDialogCoordinator::IsShowing() {
  return widget_observation_.IsObserving();
}

views::BubbleDialogDelegate* WebAppInstallDialogCoordinator::GetBubbleView() {
  return IsShowing() ? dialog_delegate_ : nullptr;
}

void WebAppInstallDialogCoordinator::StartTracking(
    views::BubbleDialogDelegate* bubble_view) {
  CHECK(!IsShowing()) << "Cannot track a new install dialog if an existing "
                         "one is already open";
  dialog_delegate_ = bubble_view;
  auto* widget = bubble_view->GetWidget();
  widget_observation_.Observe(widget);
}

void WebAppInstallDialogCoordinator::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(widget, dialog_delegate_->GetWidget());
  base::UmaHistogramEnumeration("WebApp.InstallConfirmation.CloseReason",
                                widget->closed_reason());
  StopTracking();
}

void WebAppInstallDialogCoordinator::StopTracking() {
  CHECK(IsShowing()) << "Cannot stop tracking install dialog when it was not "
                        "being tracked previously";
  widget_observation_.Reset();
  dialog_delegate_ = nullptr;
  MaybeUpdatePwaAnchorViewIfNeeded();
}

void WebAppInstallDialogCoordinator::MaybeUpdatePwaAnchorViewIfNeeded() {
  Browser* browser = &GetBrowser();
  if (!browser) {
    return;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);

  if (browser_view && browser_view->toolbar_button_provider()) {
    PageActionIconView* install_icon =
        browser_view->toolbar_button_provider()->GetPageActionIconView(
            PageActionIconType::kPwaInstall);
    if (install_icon) {
      // This ensures that quick navigations in between an installable and
      // non-installable site hides the anchor view. See the test
      // |IconVisibilityAfterTabSwitchingWhenPWAConfirmationBubbleViewShowing|
      // for more information on why this was needed.
      install_icon->Update();
    }
  }
}

WebAppInstallDialogCoordinator::WebAppInstallDialogCoordinator(Browser* browser)
    : BrowserUserData<WebAppInstallDialogCoordinator>(*browser) {}

BROWSER_USER_DATA_KEY_IMPL(WebAppInstallDialogCoordinator);

}  // namespace web_app
