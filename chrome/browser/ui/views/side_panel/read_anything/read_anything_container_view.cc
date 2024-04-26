// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_container_view.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_web_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_toolbar_view.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout.h"

// TODO(crbug.com/40909106): Remove unused constructor when the
// ReadAnythingLocalSidePanel flag is removed.
ReadAnythingContainerView::ReadAnythingContainerView(
    ReadAnythingCoordinator* coordinator,
    std::unique_ptr<ReadAnythingToolbarView> toolbar,
    std::unique_ptr<ReadAnythingSidePanelWebView> content)
    : coordinator_(std::move(coordinator)) {
  Init(std::move(toolbar), std::move(content));
  coordinator_->AddObserver(this);
  coordinator_->AddModelObserver(this);
}

ReadAnythingContainerView::ReadAnythingContainerView(
    ReadAnythingSidePanelController* controller,
    std::unique_ptr<ReadAnythingToolbarView> toolbar,
    std::unique_ptr<ReadAnythingSidePanelWebView> content)
    : controller_(std::move(controller)) {
  Init(std::move(toolbar), std::move(content));
  controller_->AddObserver(this);
  controller_->AddModelObserver(this);
}

void ReadAnythingContainerView::Init(
    std::unique_ptr<ReadAnythingToolbarView> toolbar,
    std::unique_ptr<ReadAnythingSidePanelWebView> content) {
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
}

void ReadAnythingContainerView::OnReadAnythingThemeChanged(
    const std::string& font_name,
    double font_scale,
    bool links_enabled,
    ui::ColorId foreground_color_id,
    ui::ColorId background_color_id,
    ui::ColorId separator_color_id,
    ui::ColorId dropdown_color_id,
    ui::ColorId selection_color_id,
    ui::ColorId focus_ring_color_id,
    read_anything::mojom::LineSpacing line_spacing,
    read_anything::mojom::LetterSpacing letter_spacing) {
  separator_->SetColorId(separator_color_id);
}

void ReadAnythingContainerView::OnCoordinatorDestroyed() {
  // When the coordinator that created |this| is destroyed, clean up pointers.
  coordinator_ = nullptr;
}

void ReadAnythingContainerView::OnSidePanelControllerDestroyed() {
  // When the side panel controller that created |this| is destroyed, clean up
  // pointers.
  controller_ = nullptr;
}

ReadAnythingContainerView::~ReadAnythingContainerView() {
  // If |this| is being destroyed before the associated coordinator or side
  // panel controller, then remove |this| as an observer.
  if (features::IsReadAnythingLocalSidePanelEnabled() && controller_) {
    controller_->RemoveObserver(this);
    controller_->RemoveModelObserver(this);
  } else if (coordinator_) {
    coordinator_->RemoveObserver(this);
    coordinator_->RemoveModelObserver(this);
  }
}

BEGIN_METADATA(ReadAnythingContainerView)
END_METADATA
