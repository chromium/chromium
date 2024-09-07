// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/back_gesture/back_gesture_contextual_nudge_delegate.h"

#include "ash/public/cpp/back_gesture_contextual_nudge_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/aura/window.h"

BackGestureContextualNudgeDelegate::BackGestureContextualNudgeDelegate(
    ash::BackGestureContextualNudgeController* controller)
    : controller_(controller) {}

BackGestureContextualNudgeDelegate::~BackGestureContextualNudgeDelegate() {
  StopTrackingNavigation();
}

void BackGestureContextualNudgeDelegate::MaybeStartTrackingNavigation(
    aura::Window* window) {
  if (window == window_)
    return;

  // Stop tracking the previous window before tracking a new window.
  StopTrackingNavigation();

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(window);
  if (!browser_view)
    return;

  window_ = window;
  window_->AddObserver(this);

  TabStripModel* tab_strip_model = browser_view->browser()->tab_strip_model();
  tab_strip_model->AddObserver(this);

  content::WebContents* contents = tab_strip_model->GetActiveWebContents();
  Observe(contents);
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

void BackGestureContextualNudgeDelegate::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  const bool active_tab_changed = selection.active_tab_changed();
  if (active_tab_changed) {
    DCHECK(window_);
    controller_->NavigationEntryChanged(window_);
    content::WebContents* contents = tab_strip_model->GetActiveWebContents();
    Observe(contents);
  }
}

void BackGestureContextualNudgeDelegate::OnWindowDestroying(
    aura::Window* window) {
  DCHECK_EQ(window_, window);
  StopTrackingNavigation();
}

void BackGestureContextualNudgeDelegate::StopTrackingNavigation() {
  if (window_) {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForNativeWindow(window_);
    DCHECK(browser_view);
    browser_view->browser()->tab_strip_model()->RemoveObserver(this);

    window_->RemoveObserver(this);
    window_ = nullptr;
  }
  Observe(nullptr);
}
