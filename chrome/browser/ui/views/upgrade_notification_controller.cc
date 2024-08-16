// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/upgrade_notification_controller.h"

#include "chrome/browser/buildflags.h"
#include "chrome/browser/ui/dialogs/outdated_upgrade_bubble.h"
#include "chrome/browser/ui/views/frame/browser_view.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "ui/views/interaction/element_tracker_views.h"
#endif

UpgradeNotificationController::~UpgradeNotificationController() = default;

void UpgradeNotificationController::OnOutdatedInstall() {
  ShowOutdatedUpgradeBubble(&GetBrowser(), true);
}

void UpgradeNotificationController::OnOutdatedInstallNoAutoUpdate() {
  ShowOutdatedUpgradeBubble(&GetBrowser(), false);
}

void UpgradeNotificationController::OnCriticalUpgradeInstalled() {
#if BUILDFLAG(IS_WIN)
  auto* browser_view = GetBrowserView();
  if (!browser_view) {
    return;
  }

  views::View* anchor_view =
      views::ElementTrackerViews::GetInstance()->GetUniqueView(
          kToolbarAppMenuButtonElementId,
          views::ElementTrackerViews::GetContextForView(browser_view));
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
          kToolbarAppMenuButtonElementId,
          views::ElementTrackerViews::GetContextForView(GetBrowserView()));
  return std::make_unique<CriticalNotificationBubbleView>(anchor_view);
}
#endif

UpgradeNotificationController::UpgradeNotificationController(Browser* browser)
    : BrowserUserData<UpgradeNotificationController>(*browser) {
  upgrade_detector_observation_.Observe(UpgradeDetector::GetInstance());
}

BrowserView* UpgradeNotificationController::GetBrowserView() {
  return BrowserView::GetBrowserViewForBrowser(&GetBrowser());
}

BROWSER_USER_DATA_KEY_IMPL(UpgradeNotificationController);
