// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTAINER_BACKGROUND_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTAINER_BACKGROUND_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/background.h"

class BrowserView;

// Background which renders the appropriate toolbar/bookmarks/etc. background
// on a view which must be a descendant of the browser view in the hierarchy. If
// there is a background image, it will be painted or tiled appropriately.
class TopContainerBackground : public views::Background {
 public:
  // Construct a themed background for the specified browser.
  explicit TopContainerBackground(BrowserView* browser_view);

  TopContainerBackground(const TopContainerBackground& other) = delete;
  TopContainerBackground& operator=(const TopContainerBackground& other) =
      delete;

  // We need a mechanism to consistently paint theme custom images across
  // multiple views. Specifically, IDR_THEME_TOOLBAR and IDR_THEME_FRAME*, and
  // IDR_TAB* are
  // expected to be aligned. To do this we need:
  //   (1) A fixed reference point + coordinate system that all views can align
  //   on.
  //   (2) An agreement on the position of the theme custom image in this
  //   coordinate system.
  // This is complicated by the fact that for most platforms, Chrome also draws
  // the frame/border/shadow of the window itself, which we do not want themed.
  //
  // (1) The fixed reference point that we will use for all calculations is the
  // origin of the BrowserView in the coordinate system of the root view
  // (BrowserRootView). To provide an example: at the time of this writing, on a
  // standard tabbed window on Linux, this point is (16, 13).
  // (2) The origin of the theme custom image is set to 16 DIPs above the fixed
  // reference point. See kFrameHeightAboveTabs. This is an implementation
  // detail that theme authors have been relying on for many years. Continuing
  // our example, this point is (16, -3).
  //
  // Most views will be a child of BrowserView. This method handles painting the
  // theme custom image for these views. The math is straight forward. However,
  // the portion of the tab strip that is behind the tabs is painted by
  // views::FrameBackground. The logic there must be kept in sync with the logic
  // here. Continuing our example, the origin that FrameBackground starts
  // drawing at is (16, 10). Notice that this is 3 DIPS above the BrowserView!
  //
  // This method paints IDR_THEME_TOOLBAR if its exists. Returns whether or not
  // any painting occurred.
  static bool PaintThemeCustomImage(gfx::Canvas* canvas,
                                    const views::View* view,
                                    const BrowserView* browser_view);

  // Similar to PaintThemeCustomImage but the image is supplied.
  static void PaintThemeAlignedImage(gfx::Canvas* canvas,
                                     const views::View* view,
                                     const BrowserView* browser_view,
                                     gfx::ImageSkia* image);

  // Static version for painting this background, used by the SidePanel
  // background to paint this background as a part of its background.
  // TODO(pbos): Figure out if tab painting could reuse this logic.
  static void PaintBackground(gfx::Canvas* canvas,
                              const views::View* view,
                              const BrowserView* browser_view);

 private:
  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override;

  const raw_ptr<BrowserView> browser_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTAINER_BACKGROUND_H_
