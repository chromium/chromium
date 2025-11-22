// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_EXCLUSIVE_ACCESS_CONTEXT_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_EXCLUSIVE_ACCESS_CONTEXT_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views_context.h"

class BrowserWindowInterface;
class ExclusiveAccessBubbleViews;
class Profile;
class TabStripModel;

namespace views {
class Widget;
}

// Implements the exclusive access context and bubble context for
// WebUIBrowserWindow.
class WebUIBrowserExclusiveAccessContext
    : public ExclusiveAccessContext,
      public ExclusiveAccessBubbleViewsContext {
 public:
  WebUIBrowserExclusiveAccessContext(
      Profile* profile,
      BrowserWindowInterface* browser,
      TabStripModel* tab_strip_model,
      views::Widget* widget,
      ui::AcceleratorProvider* accelerator_provider);
  WebUIBrowserExclusiveAccessContext(
      const WebUIBrowserExclusiveAccessContext&) = delete;
  WebUIBrowserExclusiveAccessContext& operator=(
      const WebUIBrowserExclusiveAccessContext&) = delete;
  ~WebUIBrowserExclusiveAccessContext() override;

  // Called when the widget's show state changes (e.g., entering/exiting
  // fullscreen).
  void OnWidgetShowStateChanged();

  // ExclusiveAccessContext:
  Profile* GetProfile() override;
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
  bool IsFullscreen() const override;

  // ExclusiveAccessBubbleViewsContext:
  ExclusiveAccessManager* GetExclusiveAccessManager() override;
  ui::AcceleratorProvider* GetAcceleratorProvider() override;
  gfx::NativeView GetBubbleParentView() const override;
  gfx::Rect GetClientAreaBoundsInScreen() const override;
  bool IsImmersiveModeEnabled() const override;
  gfx::Rect GetTopContainerBoundsInScreen() override;
  void DestroyAnyExclusiveAccessBubble() override;

 private:
  const raw_ptr<Profile> profile_;
  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<TabStripModel> tab_strip_model_;
  const raw_ptr<views::Widget> widget_;
  const raw_ptr<ui::AcceleratorProvider> accelerator_provider_;

  std::unique_ptr<ExclusiveAccessBubbleViews> exclusive_access_bubble_;

  // Tracks the task to asynchronously destroy the exclusive access bubble.
  base::CancelableTaskTracker exclusive_access_bubble_cancelable_task_tracker_;
  std::optional<base::CancelableTaskTracker::TaskId>
      exclusive_access_bubble_destruction_task_id_;

  base::WeakPtrFactory<WebUIBrowserExclusiveAccessContext> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_EXCLUSIVE_ACCESS_CONTEXT_H_
