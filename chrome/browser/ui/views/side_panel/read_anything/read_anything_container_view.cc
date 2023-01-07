// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_container_view.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_constants.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_toolbar_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/gfx/geometry/geometry_export.h"
#include "ui/views/background.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout.h"

ReadAnythingContainerView::ReadAnythingContainerView(
    std::unique_ptr<ReadAnythingToolbarView> toolbar,
    std::unique_ptr<SidePanelWebUIViewT<ReadAnythingUI>> content) {
  // Create and set a FlexLayout LayoutManager for this view, set background.
  auto layout = std::make_unique<views::FlexLayout>();
  layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetInteriorMargin(gfx::Insets(kInternalInsets));

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
  AddChildView(std::move(separator));
  AddChildView(std::move(content));
}

ReadAnythingContainerView::~ReadAnythingContainerView() = default;
