// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_DELEGATE_IMPL_H_

#include <optional>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/frame/browser_view_layout_delegate.h"

class BrowserFrameView;

// Base class for concrete implementations of layout delegate used in live
// browsers. Use `CreateDelegate()` to generate an appropriate delegate.
class BrowserViewLayoutDelegateImplBase : public BrowserViewLayoutDelegate {
 public:
  BrowserViewLayoutDelegateImplBase(const BrowserViewLayoutDelegateImplBase&) =
      delete;
  void operator=(const BrowserViewLayoutDelegateImplBase&) = delete;
  ~BrowserViewLayoutDelegateImplBase() override;

  bool ShouldDrawTabStrip() const override;
  bool GetBorderlessModeEnabled() const override;
  int GetTopInsetInBrowserView() const override;
  void LayoutWebAppWindowTitle(const gfx::Rect& available_space,
                               views::Label& window_title_label) const override;
  bool IsToolbarVisible() const override;
  bool IsBookmarkBarVisible() const override;
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

  // Creates the appropriate delegate for the browser to use given flags, etc.
  static std::unique_ptr<BrowserViewLayoutDelegate> CreateDelegate(
      BrowserView& browser_view);

 protected:
  explicit BrowserViewLayoutDelegateImplBase(BrowserView& browser_view);

  BrowserView& browser_view() { return browser_view_.get(); }
  const BrowserView& browser_view() const { return browser_view_.get(); }

  const BrowserFrameView* GetFrameView() const;

 private:
  const raw_ref<BrowserView> browser_view_;
};

// The original implementation of the layout delegate; uses obsolete
// BrowserFrameView APIs.
class BrowserViewLayoutDelegateImplOld
    : public BrowserViewLayoutDelegateImplBase {
 public:
  explicit BrowserViewLayoutDelegateImplOld(BrowserView& browser_view);
  ~BrowserViewLayoutDelegateImplOld() override;

  gfx::Rect GetBoundsForTabStripRegionInBrowserView() const override;
  gfx::Rect GetBoundsForToolbarInVerticalTabBrowserView() const override;
  gfx::Rect GetBoundsForWebAppFrameToolbarInBrowserView() const override;
};

// The new implementation of the layout delegate; uses new BrowserLayoutParams
// API.
class BrowserViewLayoutDelegateImpl : public BrowserViewLayoutDelegateImplBase {
 public:
  explicit BrowserViewLayoutDelegateImpl(BrowserView& browser_view);
  ~BrowserViewLayoutDelegateImpl() override;

  gfx::Rect GetBoundsForTabStripRegionInBrowserView() const override;
  gfx::Rect GetBoundsForToolbarInVerticalTabBrowserView() const override;
  gfx::Rect GetBoundsForWebAppFrameToolbarInBrowserView() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_DELEGATE_IMPL_H_
