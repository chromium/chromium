// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_DELEGATE_H_

#include "chrome/browser/ui/views/frame/layout/browser_view_layout_params.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/layout/layout_types.h"

class ExclusiveAccessBubbleViews;
class ImmersiveModeController;

namespace gfx {
class Rect;
}

// Delegate class to allow BrowserViewLayout to be decoupled from BrowserView
// for testing.
class BrowserViewLayoutDelegate {
 public:
  virtual ~BrowserViewLayoutDelegate() = default;

  // The state the window is in. We do not lay out when minimized/hidden, so
  // that isn't included here.
  enum class WindowState { kNormal, kMaximized, kFullscreen };

  virtual bool ShouldDrawTabStrip() const = 0;
  virtual bool ShouldUseTouchableTabstrip() const = 0;
  virtual bool ShouldDrawVerticalTabStrip() const = 0;
  virtual bool IsVerticalTabStripCollapsed() const = 0;
  virtual bool ShouldDrawWebAppFrameToolbar() const = 0;
  virtual bool GetBorderlessModeEnabled() const = 0;
  virtual BrowserLayoutParams GetBrowserLayoutParams(
      bool use_browser_bounds) const = 0;
  virtual WindowState GetBrowserWindowState() const = 0;
  virtual views::LayoutAlignment GetWindowTitleAlignment() const = 0;
  virtual bool IsToolbarVisible() const = 0;
  virtual bool IsBookmarkBarVisible() const = 0;
  virtual bool IsInfobarVisible() const = 0;
  virtual bool IsContentsSeparatorEnabled() const = 0;
  virtual bool IsActiveTabSplit() const = 0;
  virtual bool IsActiveTabAtLeadingWindowEdge() const = 0;
  virtual const ImmersiveModeController* GetImmersiveModeController() const = 0;
  virtual ExclusiveAccessBubbleViews* GetExclusiveAccessBubble() const = 0;
  virtual bool IsTopControlsSlideBehaviorEnabled() const = 0;
  virtual float GetTopControlsSlideBehaviorShownRatio() const = 0;
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
