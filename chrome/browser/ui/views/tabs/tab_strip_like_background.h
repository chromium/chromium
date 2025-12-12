// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LIKE_BACKGROUND_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LIKE_BACKGROUND_H_

#include "ui/views/background.h"

class BrowserView;

// Draws a background the same as the tabstrip
class TabStripLikeBackground : public views::Background {
 public:
  explicit TabStripLikeBackground(BrowserView* browser_view);

 private:
  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override;

  const raw_ptr<BrowserView> browser_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LIKE_BACKGROUND_H_
