// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/upgrade_notification_controller.h"

#include "base/check_deref.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/dialogs/outdated_upgrade_bubble.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#endif

UpgradeNotificationController::~UpgradeNotificationController() = default;

void UpgradeNotificationController::OnOutdatedInstall() {
  ShowOutdatedUpgradeBubble(&browser_.get(), &browser_.get(), true);
}

void UpgradeNotificationController::OnOutdatedInstallNoAutoUpdate() {
  ShowOutdatedUpgradeBubble(&browser_.get(), &browser_.get(), false);
}

void UpgradeNotificationController::OnCriticalUpgradeInstalled() {
#if BUILDFLAG(IS_WIN)
  auto* const browser_view = BrowserView::GetBrowserViewForBrowser(&*browser_);
  auto* const control =
      browser_view
          ? browser_view->toolbar_button_provider()->GetAppMenuControl()
          : nullptr;
  views::BubbleAnchor anchor =
      control ? control->GetAnchor() : views::BubbleAnchor();
  if (!anchor) {
    return;
  }

  auto bubble_view = std::make_unique<CriticalNotificationBubbleView>(anchor);
  bubble_view->SetHighlightedElement(kToolbarAppMenuButtonElementId);
  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view))->Show();
#endif
}

#if BUILDFLAG(IS_WIN)
std::unique_ptr<CriticalNotificationBubbleView>
UpgradeNotificationController::GetCriticalNotificationBubbleViewForTest() {
  views::View* const anchor_view = BrowserElementsViews::From(&*browser_)
                                       ->GetView(kToolbarActionViewElementId);
  return std::make_unique<CriticalNotificationBubbleView>(
      views::BubbleAnchor(anchor_view));
}
#endif

DEFINE_USER_DATA(UpgradeNotificationController);

UpgradeNotificationController::UpgradeNotificationController(
    BrowserWindowInterface* browser)
    : browser_(CHECK_DEREF(browser)),
      scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {
  upgrade_detector_observation_.Observe(UpgradeDetector::GetInstance());
}

// static
UpgradeNotificationController* UpgradeNotificationController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}
