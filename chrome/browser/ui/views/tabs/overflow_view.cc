// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/overflow_view.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/normalized_geometry.h"
#include "ui/views/view_class_properties.h"

namespace {

std::optional<gfx::Size> GetSizeFromFlexRule(const views::View* view,
                                             const views::SizeBounds& bounds) {
  const views::FlexSpecification* const spec =
      view->GetProperty(views::kFlexBehaviorKey);
  return spec ? std::make_optional(spec->rule().Run(view, bounds))
              : std::nullopt;
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
                           std::unique_ptr<views::View> postfix_indicator_view)
    : OverflowView(std::move(primary_view),
                   nullptr,
                   std::move(postfix_indicator_view)) {}

OverflowView::OverflowView(
    std::unique_ptr<views::View> primary_view,
    std::unique_ptr<views::View> prefix_indicator_view,
    std::unique_ptr<views::View> postfix_indicator_view) {
  if (prefix_indicator_view) {
    prefix_indicator_view_ = AddChildView(std::move(prefix_indicator_view));
  }
  primary_view_ = AddChildView(std::move(primary_view));
  if (postfix_indicator_view) {
    postfix_indicator_view_ = AddChildView(std::move(postfix_indicator_view));
  }
}

OverflowView::~OverflowView() = default;

void OverflowView::SetOrientation(views::LayoutOrientation orientation) {
  if (orientation == orientation_) {
    return;
  }
  orientation_ = orientation;
  InvalidateLayout();
}

void OverflowView::SetCrossAxisAlignment(
    views::LayoutAlignment cross_axis_alignment) {
  if (cross_axis_alignment == cross_axis_alignment_) {
    return;
  }
  cross_axis_alignment_ = cross_axis_alignment;
  InvalidateLayout();
}

void OverflowView::Layout(PassKey) {
  const gfx::Size available_size = size();
  const gfx::Size primary_size =
      GetSizeFromFlexRuleOrDefault(primary_view_, available_size);
  views::NormalizedRect primary_bounds =
      Normalize(orientation_, gfx::Rect(primary_size));
  const views::NormalizedSize normalized_size =
      Normalize(orientation_, available_size);

  // Determine the size that the overflow indicator will take up if it is
  // needed.
  views::NormalizedRect prefix_indicator_bounds;
  if (prefix_indicator_view_) {
    const auto prefix_rule_size = GetSizeFromFlexRule(
        prefix_indicator_view_, views::SizeBounds(available_size));
    if (prefix_rule_size.has_value()) {
      prefix_indicator_bounds =
          Normalize(orientation_, gfx::Rect(prefix_rule_size.value()));
    } else {
      prefix_indicator_bounds.set_size(
          Normalize(orientation_, GetSizeFromFlexRuleOrDefault(
                                      prefix_indicator_view_, available_size)));
    }
  }

  views::NormalizedRect postfix_indicator_bounds;
  if (postfix_indicator_view_) {
    const auto postfix_rule_size = GetSizeFromFlexRule(
        postfix_indicator_view_, views::SizeBounds(available_size));
    if (postfix_rule_size.has_value()) {
      postfix_indicator_bounds =
          Normalize(orientation_, gfx::Rect(postfix_rule_size.value()));
    } else {
      postfix_indicator_bounds.set_size(Normalize(
          orientation_, GetSizeFromFlexRuleOrDefault(postfix_indicator_view_,
                                                     available_size)));
    }
  }

  // Determine if overflow is occurring and show/size and position the
  // overflow indicator if it is.
  if (primary_bounds.size_main() <= normalized_size.main()) {
    if (prefix_indicator_view_) {
      prefix_indicator_view_->SetVisible(false);
    }
    if (postfix_indicator_view_) {
      postfix_indicator_view_->SetVisible(false);
    }
  } else {
    if (prefix_indicator_view_) {
      prefix_indicator_view_->SetVisible(true);
      prefix_indicator_bounds.set_origin_main(0);
      prefix_indicator_bounds.AlignCross(
          views::Span(0, normalized_size.cross()), cross_axis_alignment_);
      prefix_indicator_view_->SetBoundsRect(
          Denormalize(orientation_, prefix_indicator_bounds));
    }
    if (postfix_indicator_view_) {
      postfix_indicator_view_->SetVisible(true);
      postfix_indicator_bounds.set_origin_main(
          normalized_size.main() - postfix_indicator_bounds.size_main());
      postfix_indicator_bounds.AlignCross(
          views::Span(0, normalized_size.cross()), cross_axis_alignment_);
      postfix_indicator_view_->SetBoundsRect(
          Denormalize(orientation_, postfix_indicator_bounds));
    }

    // Also shrink the primary view by the size of the indicator.
    primary_bounds.set_origin_main(prefix_indicator_bounds.size_main());
    primary_bounds.set_size_main(std::max(
        0, normalized_size.main() - prefix_indicator_bounds.size_main() -
               postfix_indicator_bounds.size_main()));
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

  if (!parent()) {
    return views::SizeBounds();
  }

  const views::SizeBounds available = parent()->GetAvailableSize(this);
  if (child != primary_view_ ||
      (!prefix_indicator_view_ && !postfix_indicator_view_)) {
    // Give the overflow view as much space as it needs; all other views are
    // unmanaged and have no additional space constraints. The primary view is
    // given all available space when there is no overflow view.
    return available;
  }

  // The primary view may need to be limited by the size of the overflow view,
  // but only if the overflow view would be shown.
  const gfx::Size required_size =
      GetSizeFromFlexRule(child, available).value_or(child->GetMinimumSize());

  gfx::Size prefix_indicator_size;
  if (prefix_indicator_view_) {
    prefix_indicator_size =
        GetSizeFromFlexRule(prefix_indicator_view_, views::SizeBounds())
            .value_or(prefix_indicator_view_->GetPreferredSize());
  }

  gfx::Size postfix_indicator_size;
  if (postfix_indicator_view_) {
    postfix_indicator_size =
        GetSizeFromFlexRule(postfix_indicator_view_, views::SizeBounds())
            .value_or(postfix_indicator_view_->GetPreferredSize());
  }

  switch (orientation_) {
    case views::LayoutOrientation::kHorizontal:
      if (!available.width().is_bounded() ||
          available.width().value() >= required_size.width()) {
        return available;
      }
      return views::SizeBounds(std::max(0, available.width().value() -
                                               prefix_indicator_size.width() -
                                               postfix_indicator_size.width()),
                               available.height());

    case views::LayoutOrientation::kVertical:
      if (!available.height().is_bounded() ||
          available.height().value() >= required_size.height()) {
        return available;
      }
      return views::SizeBounds(
          available.width(), std::max(0, available.height().value() -
                                             prefix_indicator_size.height() -
                                             postfix_indicator_size.height()));
  }
}

gfx::Size OverflowView::GetMinimumSize() const {
  const gfx::Size primary_minimum =
      GetSizeFromFlexRule(primary_view_, views::SizeBounds(0, 0))
          .value_or(primary_view_->GetMinimumSize());
  gfx::Size prefix_indicator_minimum;
  if (prefix_indicator_view_) {
    prefix_indicator_minimum =
        GetSizeFromFlexRule(prefix_indicator_view_, views::SizeBounds(0, 0))
            .value_or(prefix_indicator_view_->GetMinimumSize());
  }
  gfx::Size postfix_indicator_minimum;
  if (postfix_indicator_view_) {
    postfix_indicator_minimum =
        GetSizeFromFlexRule(postfix_indicator_view_, views::SizeBounds(0, 0))
            .value_or(postfix_indicator_view_->GetMinimumSize());
  }

  // Minimum width on the main axis and the Minimum height on the cross axis
  // is the minimum of the indicator's minimum size and primary's minimum size.
  // When the primary view's minimum size is less than the indicator's minimum
  // size, the overflow minimum size can be shrinked down to the primary.
  switch (orientation_) {
    case views::LayoutOrientation::kHorizontal:
      return gfx::Size(std::min(prefix_indicator_minimum.width() +
                                    postfix_indicator_minimum.width(),
                                primary_minimum.width()),
                       std::max({prefix_indicator_minimum.height(),
                                 postfix_indicator_minimum.height(),
                                 primary_minimum.height()}));
    case views::LayoutOrientation::kVertical:
      return gfx::Size(std::max({prefix_indicator_minimum.width(),
                                 postfix_indicator_minimum.width(),
                                 primary_minimum.width()}),
                       std::min(prefix_indicator_minimum.height() +
                                    postfix_indicator_minimum.height(),
                                primary_minimum.height()));
  }
}

gfx::Size OverflowView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // Preferred size is the preferred size of the primary as the overflow
  // view wants to show the primary by itself if it can.
  gfx::Size result = GetSizeFromFlexRule(primary_view_, views::SizeBounds())
                         .value_or(primary_view_->GetPreferredSize());
  return result;
}

BEGIN_METADATA(OverflowView)
END_METADATA
