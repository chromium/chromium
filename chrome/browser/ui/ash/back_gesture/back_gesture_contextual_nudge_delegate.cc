// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/back_gesture/back_gesture_contextual_nudge_delegate.h"

#include "ash/public/cpp/back_gesture_contextual_nudge_controller.h"
#include "chromeos/ash/components/browser_delegate/browser_controller.h"
#include "chromeos/ash/components/browser_delegate/browser_delegate.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "ui/aura/window.h"

BackGestureContextualNudgeDelegate::BackGestureContextualNudgeDelegate(
    ash::BackGestureContextualNudgeController* controller)
    : controller_(controller) {
  tab_observation_.Observe(ash::BrowserController::GetInstance());
}

BackGestureContextualNudgeDelegate::~BackGestureContextualNudgeDelegate() {
  StopTrackingNavigation();
}

void BackGestureContextualNudgeDelegate::MaybeStartTrackingNavigation(
    aura::Window* window) {
  if (window == window_) {
    return;
  }

  // Stop tracking the previous window before tracking a new window.
  StopTrackingNavigation();

  ash::BrowserDelegate* browser =
      ash::BrowserController::GetInstance()->GetBrowserForWindow(window);
  if (!browser) {
    return;
  }

  window_ = window;
  window_->AddObserver(this);

  Observe(browser->GetActiveWebContents());
}

void BackGestureContextualNudgeDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(window_);
  // Make sure for one valid navigation, we only fire one status change
  // notification.
  if (navigation_handle->HasCommitted() &&
      ((navigation_handle->IsInPrimaryMainFrame() &&
        (navigation_handle->GetURL() !=
         navigation_handle->GetPreviousPrimaryMainFrameURL())) ||
       (navigation_handle->GetParentFrame() &&
        navigation_handle->GetParentFrame()->GetPage().IsPrimary() &&
        navigation_handle->HasSubframeNavigationEntryCommitted()))) {
    controller_->NavigationEntryChanged(window_);
  }
}

void BackGestureContextualNudgeDelegate::OnActiveWebContentsChanged(
    ash::BrowserDelegate* browser,
    content::WebContents* /*old_contents*/,
    content::WebContents* new_contents) {
  if (window_ && browser->GetNativeWindow() == window_) {
    controller_->NavigationEntryChanged(window_);
    Observe(new_contents);
  }
}

void BackGestureContextualNudgeDelegate::OnWindowDestroying(
    aura::Window* window) {
  DCHECK_EQ(window_, window);
  StopTrackingNavigation();
}

void BackGestureContextualNudgeDelegate::StopTrackingNavigation() {
  if (window_) {
    window_->RemoveObserver(this);
    window_ = nullptr;
  }
  Observe(nullptr);
}
