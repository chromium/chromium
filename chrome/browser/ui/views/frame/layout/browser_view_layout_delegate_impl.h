// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_DELEGATE_IMPL_H_

#include "base/auto_reset.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate.h"

class BrowserFrameView;

// Base class for concrete implementations of layout delegate used in live
// browsers. Use `CreateDelegate()` to generate an appropriate delegate.
class BrowserViewLayoutDelegateImpl : public BrowserViewLayoutDelegate {
 public:
  explicit BrowserViewLayoutDelegateImpl(BrowserView& browser_view);
  BrowserViewLayoutDelegateImpl(const BrowserViewLayoutDelegateImpl&) = delete;
  void operator=(const BrowserViewLayoutDelegateImpl&) = delete;
  ~BrowserViewLayoutDelegateImpl() override;

  bool ShouldDrawTabStrip() const override;
  bool ShouldUseTouchableTabstrip() const override;
  bool ShouldDrawVerticalTabStrip() const override;
  bool ShouldDrawWebAppFrameToolbar() const override;
  bool GetBorderlessModeEnabled() const override;
  gfx::Rect GetBoundsForTabStripRegionInBrowserView() const override;
  gfx::Rect GetBoundsForToolbarInVerticalTabBrowserView() const override;
  gfx::Rect GetBoundsForWebAppFrameToolbarInBrowserView() const override;
  BrowserLayoutParams GetBrowserLayoutParams(
      bool use_browser_bounds) const override;
  int GetTopInsetInBrowserView() const override;
  void LayoutWebAppWindowTitle(const gfx::Rect& available_space,
                               views::Label& window_title_label) const override;
  bool IsToolbarVisible() const override;
  bool IsBookmarkBarVisible() const override;
  bool IsInfobarVisible() const override;
  bool IsContentsSeparatorEnabled() const override;
  bool IsActiveTabSplit() const override;
  const ImmersiveModeController* GetImmersiveModeController() const override;
  ExclusiveAccessBubbleViews* GetExclusiveAccessBubble() const override;
  bool IsTopControlsSlideBehaviorEnabled() const override;
  float GetTopControlsSlideBehaviorShownRatio() const override;
  bool SupportsWindowFeature(Browser::WindowFeature feature) const override;
  gfx::NativeView GetHostViewForAnchoring() const override;
  bool HasFindBarController() const override;
  void MoveWindowForFindBarIfNecessary() const override;
  bool IsWindowControlsOverlayEnabled() const override;
  void UpdateWindowControlsOverlay(
      const gfx::Rect& available_titlebar_area) override;
  bool ShouldLayoutTabStrip() const override;
  int GetExtraInfobarOffset() const override;

 protected:
  BrowserView& browser_view() { return browser_view_.get(); }
  const BrowserView& browser_view() const { return browser_view_.get(); }

  const BrowserFrameView* GetFrameView() const;

 private:
  const raw_ref<BrowserView> browser_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_DELEGATE_IMPL_H_
