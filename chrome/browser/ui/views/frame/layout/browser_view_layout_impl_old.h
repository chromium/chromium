// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_IMPL_OLD_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_IMPL_OLD_H_

#include "chrome/browser/ui/views/frame/layout/browser_view_layout.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

class Browser;

namespace views {
class View;
}

// Original browser layout implementation.
// TODO(http://crbug.com/453717426): Move this to impl file that is only used by
// .cc file.
class BrowserViewLayoutImplOld : public BrowserViewLayout {
 public:
  BrowserViewLayoutImplOld(std::unique_ptr<BrowserViewLayoutDelegate> delegate,
                           Browser* browser,
                           BrowserViewLayoutViews views);
  ~BrowserViewLayoutImplOld() override;

  // BrowserViewLayout overrides:
  void Layout(views::View* host) override;
  gfx::Size GetMinimumSize(const views::View* host) const override;
  int GetMinWebContentsWidthForTesting() const override;

 protected:
  gfx::Point GetDialogPosition(const gfx::Size& dialog_size) const override;
  gfx::Size GetMaximumDialogSize() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserViewLayoutTest, BrowserViewLayout);
  FRIEND_TEST_ALL_PREFIXES(BrowserViewLayoutTest, Layout);

  // Helper struct and function for LayoutContentsContainerView that calculates
  // bounds for `contents_container_` and `contents_height_side_panel_`.
  struct ContentsContainerLayoutResult;
  ContentsContainerLayoutResult CalculateContentsContainerLayout(
      const gfx::Rect& available_bounds) const;

  // Layout the following controls, updating `available_bounds` to leave the
  // remaining space available for future controls.
  void LayoutTitleBarForWebApp(gfx::Rect& available_bounds);
  void LayoutVerticalTabStrip(gfx::Rect& available_bounds);
  void LayoutTabStripRegion(gfx::Rect& available_bounds);
  void LayoutWebUITabStrip(gfx::Rect& available_bounds);
  void LayoutToolbar(gfx::Rect& available_bounds);
  void LayoutBookmarkAndInfoBars(gfx::Rect& available_bounds);
  void LayoutBookmarkBar(gfx::Rect& available_bounds);
  void LayoutInfoBar(gfx::Rect& available_bounds);

  // Returns the minimum acceptable width for the browser web contents. If split
  // view is active, this includes the full split view.
  int GetMinWebContentsWidth() const;

  // Layout the `main_container_` within the available bounds.
  // See browser_view.h for details of the relationship between
  // `main_container_` and other views.
  void LayoutContentsContainerView(const gfx::Rect& available_bounds);

  // Updates `top_container_`'s bounds. The new bounds depend on the size of
  // the bookmark bar and the toolbar.
  void UpdateTopContainerBounds(const gfx::Rect& available_bounds);

  bool IsImmersiveModeEnabledWithoutToolbar() const;

  // Whether or not to use the browser based content minimum size.
  const bool use_browser_content_minimum_size_ = false;

  // The distance the web contents modal dialog is from the top of the dialog
  // host widget.
  int dialog_top_y_ = -1;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_LAYOUT_IMPL_OLD_H_
