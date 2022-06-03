// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_ANCHOR_UTIL_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_ANCHOR_UTIL_VIEWS_H_

#include "chrome/browser/ui/bubble_anchor_util.h"
#include "ui/views/bubble/bubble_border.h"

namespace views {
class Button;
class View;
}

class Browser;

namespace bubble_anchor_util {

struct AnchorConfiguration {
  // The bubble anchor view.
  views::View* anchor_view = nullptr;

  // The view to be highlighted, or null if it should not be used.
  views::Button* highlighted_button = nullptr;

  // The arrow position for the bubble.
  views::BubbleBorder::Arrow bubble_arrow = views::BubbleBorder::TOP_LEFT;
};

// Returns the anchor configuration for bubbles that are aligned to the page
// info bubble.
AnchorConfiguration GetPageInfoAnchorConfiguration(Browser* browser,
                                                   Anchor = kLocationBar);

// Returns the anchor configuration for the permission bubble.
AnchorConfiguration GetPermissionPromptBubbleAnchorConfiguration(
    Browser* browser);

}  // namespace bubble_anchor_util

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_ANCHOR_UTIL_VIEWS_H_
