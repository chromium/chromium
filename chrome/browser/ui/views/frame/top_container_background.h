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

  // Static version for painting this background, used by the SidePanel
  // background to paint this background as a part of its background.
  // TODO(pbos): See if we can get rid of `translate_view_coordinates` by
  // figuring out a way to translate the offset correctly regardless of `view`.
  // Also figure out if tab painting could reuse this logic.
  static void PaintBackground(gfx::Canvas* canvas,
                              const views::View* view,
                              const BrowserView* browser_view,
                              bool translate_view_coordinates);

 private:
  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override;

  const raw_ptr<BrowserView> browser_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTAINER_BACKGROUND_H_
