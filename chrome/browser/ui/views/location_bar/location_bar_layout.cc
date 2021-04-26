// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_layout.h"

#include "chrome/browser/themes/theme_properties.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

// Description of a decoration to be added inside the location bar, either to
// the left or to the right.
struct DecorationInfo {
  DecorationInfo(int y,
                 int height,
                 bool auto_collapse,
                 double max_fraction,
                 int edge_item_padding,
                 views::View* view);

  // The y position of the view inside its parent.
  int y;

  // The height of the view.
  int height;

  // True means that, if there is not enough available space in the location
  // bar, the view will reduce its width either to its minimal width or to zero
  // (making it invisible), whichever fits. If true, |max_fraction| must be 0.
  bool auto_collapse;

  // Used for resizeable decorations, indicates the maximum fraction of the
  // location bar that can be taken by this decoration, 0 for non-resizable
  // decorations. If non-zero, |auto_collapse| must be false.
  double max_fraction;

  // Padding to use if the decoration is the first element next to the edge.
  int edge_item_padding;

  views::View* view;

  // The width computed by the layout process.
  double computed_width;
};

DecorationInfo::DecorationInfo(int y,
                               int height,
                               bool auto_collapse,
                               double max_fraction,
                               int edge_item_padding,
                               views::View* view)
    : y(y),
      height(height),
      auto_collapse(auto_collapse),
      max_fraction(max_fraction),
      edge_item_padding(edge_item_padding),
      view(view),
      computed_width(0) {
  DCHECK((max_fraction == 0.0) || (!auto_collapse && (max_fraction > 0.0)));
}

// LocationBarLayout ---------------------------------------------------------

LocationBarLayout::LocationBarLayout(Position position, int item_edit_padding)
    : position_(position), item_edit_padding_(item_edit_padding) {}

LocationBarLayout::~LocationBarLayout() {}

void LocationBarLayout::AddDecoration(int y,
                                      int height,
                                      bool auto_collapse,
                                      double max_fraction,
                                      int edge_item_padding,
                                      views::View* view) {
  decorations_.push_back(std::make_unique<DecorationInfo>(
      y, height, auto_collapse, max_fraction, edge_item_padding, view));
}

void LocationBarLayout::LayoutPass1(int* entry_width) {
  bool first_item = true;
  for (const auto& decoration : decorations_) {
    // Autocollapsing decorations are ignored in this pass.
    if (first_item && !decoration->auto_collapse)
      *entry_width -= decoration->edge_item_padding;
    first_item = false;
    // Resizing decorations are ignored in this pass.
    if (!decoration->auto_collapse && (decoration->max_fraction == 0.0)) {
      decoration->computed_width = decoration->view->GetPreferredSize().width();
      *entry_width -= decoration->computed_width;
    }
  }
  *entry_width -= item_edit_padding_;
}

void LocationBarLayout::LayoutPass2(int* entry_width) {
  for (const auto& decoration : decorations_) {
    if (decoration->max_fraction > 0.0) {
      int max_width = static_cast<int>(*entry_width * decoration->max_fraction);
      decoration->computed_width = std::min(
          decoration->view->GetPreferredSize().width(),
          std::max(decoration->view->GetMinimumSize().width(), max_width));
      *entry_width -= decoration->computed_width;
    }
  }
}

void LocationBarLayout::LayoutPass3(gfx::Rect* bounds, int* available_width) {
  bool first_visible = true;
  for (const auto& decoration : decorations_) {
    int padding = first_visible ? decoration->edge_item_padding : 0;

    // Collapse decorations if needed.
    if (decoration->auto_collapse) {
      // Try preferred size, if it fails try minimum size, if it fails collapse.
      decoration->computed_width = decoration->view->GetPreferredSize().width();
      if (decoration->computed_width + padding > *available_width)
        decoration->computed_width = decoration->view->GetMinimumSize().width();
      if (decoration->computed_width + padding > *available_width) {
        decoration->computed_width = 0;
        decoration->view->SetVisible(false);
        continue;
      }
      (*available_width) -= decoration->computed_width + padding;
    }
    decoration->view->SetVisible(true);
    first_visible = false;

    // Layout visible decorations.
    int x = (position_ == Position::kLeftEdge)
                ? (bounds->x() + padding)
                : (bounds->right() - padding - decoration->computed_width);
    decoration->view->SetBounds(x, decoration->y, decoration->computed_width,
                                decoration->height);
    bounds->set_width(bounds->width() - padding - decoration->computed_width);
    if (position_ == Position::kLeftEdge)
      bounds->set_x(bounds->x() + padding + decoration->computed_width);
  }
  bounds->set_width(bounds->width() - item_edit_padding_);
  if (position_ == Position::kLeftEdge)
    bounds->set_x(bounds->x() + item_edit_padding_);
}
