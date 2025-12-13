// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/upgrade_notification_controller.h"

#include "base/check_deref.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/dialogs/outdated_upgrade_bubble.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#endif

UpgradeNotificationController::~UpgradeNotificationController() = default;

void UpgradeNotificationController::OnOutdatedInstall() {
  Browser* const browser = browser_->GetBrowserForMigrationOnly();
  ShowOutdatedUpgradeBubble(browser, browser, true);
}

void UpgradeNotificationController::OnOutdatedInstallNoAutoUpdate() {
  Browser* const browser = browser_->GetBrowserForMigrationOnly();
  ShowOutdatedUpgradeBubble(browser, browser, false);
}

void UpgradeNotificationController::OnCriticalUpgradeInstalled() {
#if BUILDFLAG(IS_WIN)
  auto* const anchor_view = BrowserElementsViews::From(&*browser_)
                                ->GetView(kToolbarAppMenuButtonElementId);
  if (!anchor_view) {
    return;
  }

  views::BubbleDialogDelegateView::CreateBubble(
      std::make_unique<CriticalNotificationBubbleView>(anchor_view))
      ->Show();
#endif
}

#if BUILDFLAG(IS_WIN)
std::unique_ptr<CriticalNotificationBubbleView>
UpgradeNotificationController::GetCriticalNotificationBubbleViewForTest() {
  views::View* const anchor_view = BrowserElementsViews::From(&*browser_)
                                       ->GetView(kToolbarActionViewElementId);
  return std::make_unique<CriticalNotificationBubbleView>(anchor_view);
}
#endif

UpgradeNotificationController::UpgradeNotificationController(
    BrowserWindowInterface* browser)
    : browser_(CHECK_DEREF(browser)) {
  upgrade_detector_observation_.Observe(UpgradeDetector::GetInstance());
}
