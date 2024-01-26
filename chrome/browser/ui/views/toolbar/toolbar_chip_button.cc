// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_chip_button.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/base/metadata/base_type_conversion.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view_class_properties.h"

ToolbarChipButton::ToolbarChipButton(PressedCallback callback,
                                     std::optional<Edge> flat_edge)
    : ToolbarButton(std::move(callback)), flat_edge_(flat_edge) {}

ToolbarChipButton::~ToolbarChipButton() = default;

std::optional<ToolbarButton::Edge> ToolbarChipButton::GetFlatEdge() const {
  return flat_edge_;
}

void ToolbarChipButton::SetFlatEdge(
    std::optional<ToolbarButton::Edge> flat_edge) {
  flat_edge_ = flat_edge;
  UpdateColorsAndInsets();
}

float ToolbarChipButton::GetCornerRadiusFor(Edge edge) const {
  return flat_edge_.has_value() && flat_edge_.value() == edge
             ? 0
             : GetRoundedCornerRadius();
}

void ToolbarChipButton::UpdateColorsAndInsets() {
  const gfx::Insets target_insets = GetTargetInsets();
  const int rounded_corner_radius = GetRoundedCornerRadius();

  const auto* color_provider = GetColorProvider();
  if (flat_edge_.has_value() && color_provider) {
    const gfx::Size target_size = GetTargetSize();
    const int extra_height = std::max(
        0, target_size.height() - GetLayoutConstant(LOCATION_BAR_HEIGHT));
    const gfx::Insets paint_insets = gfx::Insets(extra_height / 2) +
                                     *GetProperty(views::kInternalPaddingKey);
    SkColor background_color =
        color_provider->GetColor(kColorToolbarBackgroundSubtleEmphasis);

    // Set background without taking into account flat edges, since border
    // will take care of that.
    SetBackground(views::CreateBackgroundFromPainter(
        views::Painter::CreateSolidRoundRectPainter(
            background_color, rounded_corner_radius, paint_insets)));
    label()->SetBackgroundColor(background_color);

    // Add border with radius according to the button's flat edge.
    const int left_corner_radius =
        GetCornerRadiusFor(ToolbarChipButton::Edge::kLeft);
    const int right_corner_radius =
        GetCornerRadiusFor(ToolbarChipButton::Edge::kRight);

    std::unique_ptr<views::Border> internal_border = views::CreateBorderPainter(
        views::Painter::CreateSolidRoundRectPainterWithVariableRadius(
            background_color,
            gfx::RoundedCornersF(left_corner_radius, right_corner_radius,
                                 right_corner_radius, left_corner_radius),
            paint_insets),
        paint_insets);
    const gfx::Insets extra_insets =
        target_insets - internal_border->GetInsets();
    SetBorder(
        views::CreatePaddedBorder(std::move(internal_border), extra_insets));
  } else {
    // Button's with no flat edge are standalone buttons and don't have a
    // background or border.
    SetBackground(nullptr);
    SetBorder(views::CreateEmptyBorder(target_insets));
  }

  // Update spacing on the outer-side of the label to match the current
  // corner radius.
  SetLabelSideSpacing(rounded_corner_radius / 2);
}

BEGIN_METADATA(ToolbarChipButton)
ADD_PROPERTY_METADATA(std::optional<ToolbarButton::Edge>, FlatEdge)
END_METADATA

DEFINE_ENUM_CONVERTERS(ToolbarButton::Edge,
                       {ToolbarButton::Edge::kLeft, u"kLeft"},
                       {ToolbarButton::Edge::kRight, u"kRight"})
