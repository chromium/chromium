// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_container_view.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_toolbar_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_ui.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/geometry_export.h"
#include "ui/views/background.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout.h"

namespace {

int GetNormalizedFontScale(double font_scale) {
  DCHECK(font_scale >= kReadAnythingMinimumFontScale &&
         font_scale <= kReadAnythingMaximumFontScale);
  return (font_scale - kReadAnythingMinimumFontScale) *
         (1 / kReadAnythingFontScaleIncrement);
}

}  // namespace

ReadAnythingContainerView::ReadAnythingContainerView(
    ReadAnythingCoordinator* coordinator,
    std::unique_ptr<ReadAnythingToolbarView> toolbar,
    std::unique_ptr<SidePanelWebUIViewT<ReadAnythingUI>> content)
    : coordinator_(std::move(coordinator)) {
  // Create and set a FlexLayout LayoutManager for this view, set background.
  auto layout = std::make_unique<views::FlexLayout>();
  layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  SetLayoutManager(std::move(layout));
  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorPrimaryBackground));

  // Set flex behavior on provided toolbar and content, and include a separator.
  toolbar->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kScaleToMaximum)
          .WithOrder(1));

  auto separator = std::make_unique<views::Separator>();
  separator->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kScaleToMaximum)
          .WithOrder(2));
  content->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(3));

  // Add all views as children.
  AddChildView(std::move(toolbar));
  separator_ = AddChildView(std::move(separator));
  AddChildView(std::move(content));

  coordinator_->AddObserver(this);
  coordinator_->AddModelObserver(this);
}

void ReadAnythingContainerView::OnReadAnythingThemeChanged(
    const std::string& font_name,
    double font_scale,
    ui::ColorId foreground_color_id,
    ui::ColorId background_color_id,
    ui::ColorId separator_color_id,
    ui::ColorId dropdown_color_id,
    read_anything::mojom::LineSpacing line_spacing,
    read_anything::mojom::LetterSpacing letter_spacing) {
  separator_->SetColorId(separator_color_id);
}

void ReadAnythingContainerView::OnCoordinatorDestroyed() {
  // When the coordinator that created |this| is destroyed, clean up pointers.
  LogTextStyle();
  coordinator_ = nullptr;
}

void ReadAnythingContainerView::LogTextStyle() {
  int maximum_font_scale_logging =
      GetNormalizedFontScale(kReadAnythingMaximumFontScale);
  double font_scale = coordinator_->GetModel()->GetFontScale();
  base::UmaHistogramExactLinear(string_constants::kFontScaleHistogramName,
                                GetNormalizedFontScale(font_scale),
                                maximum_font_scale_logging + 1);
  ReadAnythingColorsModel::ReadAnythingColor color =
      coordinator_->GetModel()->color_logging_value();
  base::UmaHistogramEnumeration(string_constants::kColorHistogramName, color);
  read_anything::mojom::LineSpacing line_spacing =
      coordinator_->GetModel()->line_spacing();
  base::UmaHistogramEnumeration(string_constants::kLineSpacingHistogramName,
                                line_spacing);
  read_anything::mojom::LetterSpacing letter_spacing =
      coordinator_->GetModel()->letter_spacing();
  base::UmaHistogramEnumeration(string_constants::kLetterSpacingHistogramName,
                                letter_spacing);
}

BEGIN_METADATA(ReadAnythingContainerView, views::View)
END_METADATA

ReadAnythingContainerView::~ReadAnythingContainerView() {
  // If |this| is being destroyed before the associated coordinator, then
  // remove |this| as an observer.
  if (coordinator_) {
    LogTextStyle();
    coordinator_->RemoveObserver(this);
    coordinator_->RemoveModelObserver(this);
  }
}
