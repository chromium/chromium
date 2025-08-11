// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/upgrade_notification_controller.h"

#include "base/check_deref.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/dialogs/outdated_upgrade_bubble.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/interaction/element_tracker_views.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/browser_element_identifiers.h"
#endif

UpgradeNotificationController::~UpgradeNotificationController() = default;

void UpgradeNotificationController::OnOutdatedInstall() {
  Browser* const browser = browser_->GetBrowserForMigrationOnly();
  ShowOutdatedUpgradeBubble(GetBrowserElementContext(), browser, true);
}

void UpgradeNotificationController::OnOutdatedInstallNoAutoUpdate() {
  Browser* const browser = browser_->GetBrowserForMigrationOnly();
  ShowOutdatedUpgradeBubble(GetBrowserElementContext(), browser, false);
}

void UpgradeNotificationController::OnCriticalUpgradeInstalled() {
#if BUILDFLAG(IS_WIN)
  views::View* anchor_view =
      views::ElementTrackerViews::GetInstance()->GetUniqueView(
          kToolbarAppMenuButtonElementId, GetBrowserElementContext());
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
  views::View* anchor_view =
      views::ElementTrackerViews::GetInstance()->GetUniqueView(
          kToolbarAppMenuButtonElementId, GetBrowserElementContext());
  return std::make_unique<CriticalNotificationBubbleView>(anchor_view);
}
#endif

UpgradeNotificationController::UpgradeNotificationController(
    BrowserWindowInterface* browser)
    : browser_(CHECK_DEREF(browser)) {
  upgrade_detector_observation_.Observe(UpgradeDetector::GetInstance());
}

ui::ElementContext UpgradeNotificationController::GetBrowserElementContext() {
  return browser_->GetBrowserForMigrationOnly()->window()->GetElementContext();
}
