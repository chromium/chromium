// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTAINER_BACKGROUND_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTAINER_BACKGROUND_H_

#include "ui/views/background.h"

class BrowserView;

// Background which renders the appropriate toolbar/bookmarks/etc. background
// on a view which must be a descendant of the browser view in the hierarchy. If
// there is a background image, it will be painted or tiled appropriately.
class TopContainerBackground : public views::Background {
 public:
  // Construct a themed background for the specified browser.
  explicit TopContainerBackground(BrowserView* browser_view);

 private:
  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override;

  BrowserView* const browser_view_;

  // Disallow copy and assign.
  TopContainerBackground(const TopContainerBackground& other) = delete;
  TopContainerBackground& operator=(const TopContainerBackground& other) =
      delete;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTAINER_BACKGROUND_H_
