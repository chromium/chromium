// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_chromeos_test_utils.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_chromeos.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/shelf_test_api.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Toggles fullscreen mode and waits for the notification.
void ToggleFullscreenModeAndWait(Browser* browser) {
  FullscreenNotificationObserver waiter(browser);
  chrome::ToggleFullscreenMode(browser);
  waiter.Wait();
}

// Enters fullscreen mode for tab and waits for the notification.
void EnterFullscreenModeForTabAndWait(Browser* browser,
                                      content::WebContents* web_contents) {
  FullscreenNotificationObserver waiter(browser);
  static_cast<content::WebContentsDelegate*>(browser)
      ->EnterFullscreenModeForTab(web_contents->GetPrimaryMainFrame(), {});
  waiter.Wait();
}

// Exits fullscreen mode for tab and waits for the notification.
void ExitFullscreenModeForTabAndWait(Browser* browser,
                                     content::WebContents* web_contents) {
  FullscreenNotificationObserver waiter(browser);
  browser->exclusive_access_manager()
      ->fullscreen_controller()
      ->ExitFullscreenModeForTab(web_contents);
  waiter.Wait();
}

BrowserNonClientFrameViewChromeOS* GetFrameViewChromeOS(
    BrowserView* browser_view) {
  // We know we're using ChromeOS, so static cast.
  auto* frame_view = static_cast<BrowserNonClientFrameViewChromeOS*>(
      browser_view->GetWidget()->non_client_view()->frame_view());
  DCHECK(frame_view);
  return frame_view;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void StartOverview() {
  ash::Shell::Get()->overview_controller()->StartOverview(
      ash::OverviewStartAction::kTests);
}

void EndOverview() {
  ash::Shell::Get()->overview_controller()->EndOverview(
      ash::OverviewEndAction::kTests);
}

bool IsShelfVisible() {
  return ash::ShelfTestApi().IsVisible();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
