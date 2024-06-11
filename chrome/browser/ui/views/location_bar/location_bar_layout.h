// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_LAYOUT_H_

#include <memory>
#include <vector>

namespace gfx {
class Rect;
}

namespace views {
class View;
}

struct DecorationInfo;

// Helper class used to layout a list of decorations inside the omnibox.
class LocationBarLayout {
 public:
  enum class Position {
    kLeftEdge,
    kRightEdge,
  };

  LocationBarLayout(Position position, int item_edit_padding);

  LocationBarLayout(const LocationBarLayout&) = delete;
  LocationBarLayout& operator=(const LocationBarLayout&) = delete;

  virtual ~LocationBarLayout();

  // Add a decoration, specifying:
  // - The |y| position inside its parent;
  // - The |height| in pixel, 0 meaning the preferred height of the |view|;
  // - Whether the decoration should |auto_collapse| if there is no room for it;
  // - The |max_fraction| it can use within the omnibox, or 0 for non-resizable
  //   decorations;
  // - |intra_item_padding|, the padding between the item and the previous item.
  //   Does not apply to the first item, which instead uses edge_item_padding.
  // - |edge_item_padding|, the padding between the omnibox edge and the item,
  //   if the item is the first one drawn;
  // - The |view| corresponding to this decoration, a weak pointer.
  // Note that |auto_collapse| can be true if and only if |max_fraction| is 0.
  void AddDecoration(int y,
                     int height,
                     bool auto_collapse,
                     double max_fraction,
                     int intra_item_padding,
                     int edge_item_padding,
                     views::View* view);

  // First pass of decoration layout process. Pass the full width of the
  // location bar in `entry_width`. This pass will decrease it to account for
  // non-collapsible and non-resizable decorations.
  // `reserved_width` is the minimum necessary width required by the location
  // bar excluding decorations. This used in preferred size calculations to
  // ensure the available space constraint is calculated correctly.
  void LayoutPass1(int* entry_width, int reserved_width);

  // Second pass of decoration layout process. Pass the `entry_width` computed
  // by the first pass. This pass will decrease it to account for resizable
  // decorations.
  void LayoutPass2(int* entry_width);

  // Third and final pass of decoration layout process. Pass the `bounds`
  // corresponding to the entire space available in the location bar. This pass
  // will update it as decorations are laid out. `available_width` measures the
  // empty space within the location bar, taking the decorations and text into
  // account. `decorations_` must always be ordered from the edge of the
  // location bar towards the middle.
  void LayoutPass3(gfx::Rect* bounds, int* available_width);

 private:
  // LEFT_EDGE means decorations are added from left to right and stacked on
  // the left of the omnibox, RIGHT_EDGE means the opposite.
  Position position_;

  // The padding between the last decoration and the edit box.
  int item_edit_padding_;

  // The list of decorations to layout.
  std::vector<std::unique_ptr<DecorationInfo>> decorations_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_LAYOUT_H_
