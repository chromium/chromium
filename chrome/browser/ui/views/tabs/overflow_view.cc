// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/overflow_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/normalized_geometry.h"
#include "ui/views/view_class_properties.h"

namespace {

absl::optional<gfx::Size> GetSizeFromFlexRule(const views::View* view,
                                              const views::SizeBounds& bounds) {
  const views::FlexSpecification* const spec =
      view->GetProperty(views::kFlexBehaviorKey);
  return spec ? absl::make_optional(spec->rule().Run(view, bounds))
              : absl::nullopt;
}

gfx::Size GetSizeFromFlexRuleOrDefault(const views::View* view,
                                       const gfx::Size& bounds) {
  const views::FlexSpecification* const property_spec =
      view->GetProperty(views::kFlexBehaviorKey);
  views::FlexSpecification spec =
      property_spec ? *property_spec
                    : views::FlexSpecification(
                          views::MinimumFlexSizeRule::kScaleToMinimum);
  return spec.rule().Run(view, views::SizeBounds(bounds));
}

}  // namespace

OverflowView::OverflowView(std::unique_ptr<views::View> primary_view,
                           std::unique_ptr<views::View> indicator_view) {
  primary_view_ = AddChildView(std::move(primary_view));
  indicator_view_ = AddChildView(std::move(indicator_view));
}

OverflowView::~OverflowView() = default;

void OverflowView::SetOrientation(views::LayoutOrientation orientation) {
  if (orientation == orientation_)
    return;
  orientation_ = orientation;
  InvalidateLayout();
}

void OverflowView::SetCrossAxisAlignment(
    views::LayoutAlignment cross_axis_alignment) {
  if (cross_axis_alignment == cross_axis_alignment_)
    return;
  cross_axis_alignment_ = cross_axis_alignment;
  InvalidateLayout();
}

void OverflowView::Layout() {
  const gfx::Size available_size = size();
  const gfx::Size primary_size =
      GetSizeFromFlexRuleOrDefault(primary_view_, available_size);
  views::NormalizedRect primary_bounds =
      Normalize(orientation_, gfx::Rect(primary_size));
  const views::NormalizedSize normalized_size =
      Normalize(orientation_, available_size);

  // Determine the size that the overflow indicator will take up if it is
  // needed.
  views::NormalizedRect indicator_bounds;
  const auto rule_size =
      GetSizeFromFlexRule(indicator_view_, views::SizeBounds(available_size));
  if (rule_size.has_value()) {
    indicator_bounds = Normalize(orientation_, gfx::Rect(rule_size.value()));
  } else {
    indicator_bounds.set_size(Normalize(
        orientation_,
        GetSizeFromFlexRuleOrDefault(indicator_view_, available_size)));
  }

  // Determine if overflow is occurring and show/size and position the
  // overflow indicator if it is.
  if (primary_bounds.size_main() <= normalized_size.main()) {
    indicator_view_->SetVisible(false);
  } else {
    indicator_view_->SetVisible(true);
    indicator_bounds.set_origin_main(normalized_size.main() -
                                     indicator_bounds.size_main());
    indicator_bounds.AlignCross(views::Span(0, normalized_size.cross()),
                                cross_axis_alignment_);
    indicator_view_->SetBoundsRect(Denormalize(orientation_, indicator_bounds));

    // Also shrink the primary view by the size of the indicator.
    primary_bounds.set_size_main(std::max(0, indicator_bounds.origin_main()));
  }

  // Hide/show and size the primary view.
  if (primary_bounds.is_empty()) {
    primary_view_->SetVisible(false);
  } else {
    primary_view_->SetVisible(true);
    primary_bounds.AlignCross(views::Span(0, normalized_size.cross()),
                              cross_axis_alignment_);
    primary_view_->SetBoundsRect(Denormalize(orientation_, primary_bounds));
  }
}

views::SizeBounds OverflowView::GetAvailableSize(
    const views::View* child) const {
  DCHECK_EQ(this, child->parent());

  if (!parent())
    return views::SizeBounds();

  const views::SizeBounds available = parent()->GetAvailableSize(this);
  if (child != primary_view_ || !indicator_view_) {
    // Give the overflow view as much space as it needs; all other views are
    // unmanaged and have no additional space constraints. The primary view is
    // given all available space when there is no overflow view.
    return available;
  }

  // The primary view may need to be limited by the size of the overflow view,
  // but only if the overflow view would be shown.
  const gfx::Size required_size =
      GetSizeFromFlexRule(child, available).value_or(child->GetMinimumSize());
  const gfx::Size indicator_size =
      GetSizeFromFlexRule(indicator_view_, views::SizeBounds())
          .value_or(indicator_view_->GetPreferredSize());
  switch (orientation_) {
    case views::LayoutOrientation::kHorizontal:
      if (!available.width().is_bounded() ||
          available.width().value() >= required_size.width()) {
        return available;
      }
      return views::SizeBounds(
          std::max(0, available.width().value() - indicator_size.width()),
          available.height());
    case views::LayoutOrientation::kVertical:
      if (!available.height().is_bounded() ||
          available.height().value() >= required_size.height()) {
        return available;
      }
      return views::SizeBounds(
          available.width(),
          std::max(0, available.height().value() - indicator_size.height()));
  }
}

gfx::Size OverflowView::GetMinimumSize() const {
  const gfx::Size primary_minimum =
      GetSizeFromFlexRule(primary_view_, views::SizeBounds(0, 0))
          .value_or(primary_view_->GetMinimumSize());
  const gfx::Size indicator_minimum =
      GetSizeFromFlexRule(indicator_view_, views::SizeBounds(0, 0))
          .value_or(indicator_view_->GetMinimumSize());

  // Minimum size on the main axis is the indicator's minimum size. Minimum size
  // on the cross axis is the larger of the two minimum sizes.
  switch (orientation_) {
    case views::LayoutOrientation::kHorizontal:
      return gfx::Size(
          indicator_minimum.width(),
          std::max(indicator_minimum.height(), primary_minimum.height()));
    case views::LayoutOrientation::kVertical:
      return gfx::Size(
          std::max(indicator_minimum.width(), primary_minimum.width()),
          indicator_minimum.height());
  }
}

gfx::Size OverflowView::CalculatePreferredSize() const {
  // Preferred size is the max of the preferred sizes of the primary and
  // indicator views.
  gfx::Size result = GetSizeFromFlexRule(primary_view_, views::SizeBounds())
                         .value_or(primary_view_->GetPreferredSize());
  result.SetToMax(GetSizeFromFlexRule(indicator_view_, views::SizeBounds())
                      .value_or(indicator_view_->GetPreferredSize()));
  return result;
}

int OverflowView::GetHeightForWidth(int width) const {
  const auto primary_size = GetSizeFromFlexRule(
      primary_view_, views::SizeBounds(width, views::SizeBound()));
  int primary_height = primary_size ? primary_size->height()
                                    : primary_view_->GetHeightForWidth(width);
  const auto indicator_size = GetSizeFromFlexRule(
      indicator_view_, views::SizeBounds(width, views::SizeBound()));
  const int indicator_height = indicator_size
                                   ? indicator_size->height()
                                   : indicator_view_->GetHeightForWidth(width);
  return std::max(primary_height, indicator_height);
}
