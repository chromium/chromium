// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_DELEGATE_H_

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_params.h"
#include "ui/gfx/native_ui_types.h"

class ImmersiveModeController;
class ExclusiveAccessBubbleViews;

namespace gfx {
class Rect;
}

namespace views {
class Label;
}

// Delegate class to allow BrowserViewLayout to be decoupled from BrowserView
// for testing.
class BrowserViewLayoutDelegate {
 public:
  virtual ~BrowserViewLayoutDelegate() = default;

  virtual bool ShouldDrawTabStrip() const = 0;
  virtual bool ShouldUseTouchableTabstrip() const = 0;
  virtual bool ShouldDrawVerticalTabStrip() const = 0;
  virtual bool ShouldDrawWebAppFrameToolbar() const = 0;
  virtual bool GetBorderlessModeEnabled() const = 0;
  virtual gfx::Rect GetBoundsForTabStripRegionInBrowserView() const = 0;
  virtual gfx::Rect GetBoundsForToolbarInVerticalTabBrowserView() const = 0;
  virtual gfx::Rect GetBoundsForWebAppFrameToolbarInBrowserView() const = 0;
  virtual BrowserLayoutParams GetBrowserLayoutParams(
      bool use_browser_bounds) const = 0;
  virtual void LayoutWebAppWindowTitle(
      const gfx::Rect& available_space,
      views::Label& window_title_label) const = 0;
  virtual int GetTopInsetInBrowserView() const = 0;
  virtual bool IsToolbarVisible() const = 0;
  virtual bool IsBookmarkBarVisible() const = 0;
  virtual bool IsInfobarVisible() const = 0;
  virtual bool IsContentsSeparatorEnabled() const = 0;
  virtual bool IsActiveTabSplit() const = 0;
  virtual const ImmersiveModeController* GetImmersiveModeController() const = 0;
  virtual ExclusiveAccessBubbleViews* GetExclusiveAccessBubble() const = 0;
  virtual bool IsTopControlsSlideBehaviorEnabled() const = 0;
  virtual float GetTopControlsSlideBehaviorShownRatio() const = 0;
  virtual bool SupportsWindowFeature(Browser::WindowFeature feature) const = 0;
  virtual gfx::NativeView GetHostViewForAnchoring() const = 0;
  virtual bool HasFindBarController() const = 0;
  virtual void MoveWindowForFindBarIfNecessary() const = 0;
  virtual bool IsWindowControlsOverlayEnabled() const = 0;
  virtual void UpdateWindowControlsOverlay(
      const gfx::Rect& available_titlebar_area) = 0;
  virtual bool ShouldLayoutTabStrip() const = 0;
  virtual int GetExtraInfobarOffset() const = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_DELEGATE_H_
