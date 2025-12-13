// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_header.h"

#include <memory>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/side_panel/side_panel_header_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {
// The minimum cross axis size should the expected height of the header.
constexpr int kDefaultSidePanelHeaderHeight = 40;
}  // namespace

SidePanelHeader::SidePanelHeader(
    std::unique_ptr<SidePanelHeader::Delegate> side_panel_header_delegate)
    : side_panel_header_delegate_(std::move(side_panel_header_delegate)) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  auto* const layout = SetLayoutManager(std::make_unique<views::FlexLayout>());

  const int header_interior_margin =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::
              DISTANCE_SIDE_PANEL_HEADER_INTERIOR_MARGIN_HORIZONTAL);
  layout->SetDefault(views::kMarginsKey,
                     gfx::Insets().set_left(header_interior_margin));

  // Set alignments for horizontal (main) and vertical (cross) axes.
  layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  layout->SetMinimumCrossAxisSize(kDefaultSidePanelHeaderHeight);

  auto panel_icon = side_panel_header_delegate_->CreatePanelIcon();
  panel_icon->SetProperty(views::kMarginsKey,
                          gfx::Insets().set_left(header_interior_margin * 2));
  panel_icon_ = AddChildView(std::move(panel_icon));

  auto panel_title = side_panel_header_delegate_->CreatePanelTitle();
  panel_title->SetProperty(views::kMarginsKey,
                           gfx::Insets().set_left(header_interior_margin * 2));
  panel_title->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithAlignment(views::LayoutAlignment::kStart));
  panel_title_ = AddChildView(std::move(panel_title));

  auto pin_button = side_panel_header_delegate_->CreatePinButton();
  pin_button->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification().WithAlignment(views::LayoutAlignment::kEnd));
  header_pin_button_ = AddChildView(std::move(pin_button));

  auto create_open_new_tab_button =
      side_panel_header_delegate_->CreateOpenNewTabButton();
  create_open_new_tab_button->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification().WithAlignment(views::LayoutAlignment::kEnd));
  header_open_in_new_tab_button_ =
      AddChildView(std::move(create_open_new_tab_button));

  auto more_info_button = side_panel_header_delegate_->CreateMoreInfoButton();
  more_info_button->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification().WithAlignment(views::LayoutAlignment::kEnd));
  header_more_info_button_ = AddChildView(std::move(more_info_button));

  auto close_button = side_panel_header_delegate_->CreateCloseButton();
  close_button->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification().WithAlignment(views::LayoutAlignment::kEnd));
  AddChildView(std::move(close_button));
}

SidePanelHeader::~SidePanelHeader() = default;

void SidePanelHeader::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  // The side panel header should draw on top of its parent's border.
  gfx::Rect contents_bounds = parent()->GetContentsBounds();

  gfx::Rect header_bounds = gfx::Rect(
      contents_bounds.x(), contents_bounds.y() - GetPreferredSize().height(),
      contents_bounds.width(), GetPreferredSize().height());

  SetBoundsRect(header_bounds);
}

BEGIN_METADATA(SidePanelHeader)
END_METADATA
