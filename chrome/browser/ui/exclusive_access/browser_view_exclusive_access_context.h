// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_BROWSER_VIEW_EXCLUSIVE_ACCESS_CONTEXT_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_BROWSER_VIEW_EXCLUSIVE_ACCESS_CONTEXT_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/views/exclusive_access/exclusive_access_bubble_views_context.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserView;
class BrowserWindowInterface;
class ExclusiveAccessBubbleViews;

// Adapts BrowserView to the exclusive access and bubble contexts. Owned by
// BrowserWindowFeatures and destroyed during pre-window teardown while
// BrowserView and its widget are still available.
class BrowserViewExclusiveAccessContext
    : public ExclusiveAccessContext,
      public ExclusiveAccessBubbleViewsContext {
 public:
  DECLARE_USER_DATA(BrowserViewExclusiveAccessContext);

  // Returns null for other window implementations, before window features are
  // initialized, and after they have been torn down.
  static BrowserViewExclusiveAccessContext* From(
      BrowserWindowInterface* browser);

  BrowserViewExclusiveAccessContext(BrowserWindowInterface& browser_window,
                                    BrowserView& browser_view);
  BrowserViewExclusiveAccessContext(const BrowserViewExclusiveAccessContext&) =
      delete;
  BrowserViewExclusiveAccessContext& operator=(
      const BrowserViewExclusiveAccessContext&) = delete;
  ~BrowserViewExclusiveAccessContext() override;

  // Returns nullptr when no bubble exists, even if this context is initialized.
  ExclusiveAccessBubbleViews* exclusive_access_bubble() const {
    return exclusive_access_bubble_.get();
  }

  // ExclusiveAccessContext:
  Profile* GetProfile() override;
  bool IsFullscreen() const override;
  void UpdateUIForTabFullscreen() override;
  void EnterFullscreen(const url::Origin& origin,
                       ExclusiveAccessBubbleType bubble_type,
                       FullscreenTabParams fullscreen_tab_params) override;
  void ExitFullscreen() override;
  void UpdateExclusiveAccessBubble(
      const ExclusiveAccessBubbleParams& params,
      ExclusiveAccessBubbleHideCallback first_hide_callback) override;
  bool IsExclusiveAccessBubbleDisplayed() const override;
  void OnExclusiveAccessUserInput() override;
  content::WebContents* GetWebContentsForExclusiveAccess() override;
  bool CanUserEnterFullscreen() const override;
  bool CanUserExitFullscreen() const override;

  // ExclusiveAccessBubbleViewsContext:
  ExclusiveAccessManager* GetExclusiveAccessManager() override;
  ui::AcceleratorProvider* GetAcceleratorProvider() override;
  gfx::NativeView GetBubbleParentView() const override;
  gfx::Rect GetClientAreaBoundsInScreen() const override;
  bool IsImmersiveModeEnabled() const override;
  gfx::Rect GetTopContainerBoundsInScreen() override;
  void DestroyAnyExclusiveAccessBubble() override;

 private:
  const raw_ref<BrowserWindowInterface> browser_window_;
  const raw_ref<BrowserView> browser_view_;
  std::unique_ptr<ExclusiveAccessBubbleViews> exclusive_access_bubble_;

  // Tracks the task to asynchronously destroy the exclusive access bubble.
  base::CancelableTaskTracker exclusive_access_bubble_cancelable_task_tracker_;
  std::optional<base::CancelableTaskTracker::TaskId>
      exclusive_access_bubble_destruction_task_id_;

  ui::ScopedUnownedUserData<BrowserViewExclusiveAccessContext>
      scoped_unowned_user_data_;
  base::WeakPtrFactory<BrowserViewExclusiveAccessContext> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_BROWSER_VIEW_EXCLUSIVE_ACCESS_CONTEXT_H_
