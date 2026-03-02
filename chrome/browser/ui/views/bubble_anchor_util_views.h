// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_ANCHOR_UTIL_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_ANCHOR_UTIL_VIEWS_H_

#include <optional>

#include "chrome/browser/ui/bubble_anchor_util.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Browser;

namespace bubble_anchor_util {

struct AnchorConfiguration {
  // The bubble anchor.
  views::BubbleAnchor anchor = nullptr;

  // The element to be highlighted, or nullopt if it should not be used.
  std::optional<ui::ElementIdentifier> highlighted_element;

  // The arrow position for the bubble.
  views::BubbleBorder::Arrow bubble_arrow = views::BubbleBorder::TOP_LEFT;
};

// Returns the anchor configuration for bubbles that are aligned to the page
// info bubble.
AnchorConfiguration GetPageInfoAnchorConfiguration(
    Browser* browser,
    Anchor = Anchor::kLocationBar);

// Returns the anchor configuration for the permission bubble.
AnchorConfiguration GetPermissionPromptBubbleAnchorConfiguration(
    Browser* browser);

// Returns the anchor configuration for bubbles that are aligned to the app menu
// button.
AnchorConfiguration GetAppMenuAnchorConfiguration(Browser* browser);

// Returns true if the given anchor can be used as a highlight.
bool IsHighlightable(views::BubbleAnchor anchor);

}  // namespace bubble_anchor_util

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_ANCHOR_UTIL_VIEWS_H_
