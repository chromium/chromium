// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_OVERFLOW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_OVERFLOW_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

// Encloses a primary view and shows an overflow indicator (which is a second
// child view) if the overflow view is given less than the other view's minimum
// space.
//
// Provided because FlexLayout is not designed to show additional views as the
// parent view shrinks, but rather to shrink/hide them. It might still be
// possible to approximate this behavior with FlexLayout and creative use of
// FlexRules.
//
// Size of both views is by default interpolated between preferred and minimum
// size, with the indicator taking precedence if present. However you may set
// kFlexBehaviorKey on either or both views to provide different behavior.
//
// Note: currently only FlexSpecification::rule() will currently be respected as
// the other behaviors are implicit in how OverflowView functions. In the future
// we may add support for alignment() on the primary view as well as supporting
// kMarginsKey and kInternalPaddingKey. In the meantime you can simulate these
// by nesting views using the layout of your choice, or adding a border.
//
// TODO(dfried): If this turns out to be usable in multiple places, move to
// ui/views/layout.
// If there are 2 indicator views defined, then the first goes on the left and
// the the second goes on the right, else only the right gets filled.
class OverflowView : public views::View {
  METADATA_HEADER(OverflowView, views::View)

 public:
  OverflowView(std::unique_ptr<views::View> primary_view,
               std::unique_ptr<views::View> prefix_indicator_view,
               std::unique_ptr<views::View> postfix_indicator_view);
  OverflowView(std::unique_ptr<views::View> primary_view,
               std::unique_ptr<views::View> postfix_indicator_view);
  ~OverflowView() override;

  void SetOrientation(views::LayoutOrientation orientation);
  views::LayoutOrientation orientation() const { return orientation_; }

  void SetCrossAxisAlignment(views::LayoutAlignment cross_axis_alignment);
  views::LayoutAlignment cross_axis_alignment() const {
    return cross_axis_alignment_;
  }

  // View:
  void Layout(PassKey) override;
  views::SizeBounds GetAvailableSize(const View* child) const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  // Whether the primary and overflow views are arranged horizontally or
  // vertically. The overflow view is always placed in the trailing position.
  views::LayoutOrientation orientation_ = views::LayoutOrientation::kHorizontal;

  // By default stretch the main and overflow indicator views to the cross axis
  // size of this view.
  views::LayoutAlignment cross_axis_alignment_ =
      views::LayoutAlignment::kStretch;

  // The primary view to be displayed.
  raw_ptr<views::View> primary_view_ = nullptr;

  // The overflow indicator view to be displayed if the primary view will
  // receive less than its minimum size along the main axis.
  raw_ptr<views::View> prefix_indicator_view_ = nullptr;

  // The overflow indicator view to be displayed if the primary view will
  // receive less than its minimum size along the main axis.
  raw_ptr<views::View> postfix_indicator_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_OVERFLOW_VIEW_H_
