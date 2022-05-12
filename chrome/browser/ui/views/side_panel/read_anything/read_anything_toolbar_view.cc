// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_toolbar_view.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_constants.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/geometry_export.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_types.h"

ReadAnythingToolbarView::ReadAnythingToolbarView(
    ReadAnythingCoordinator* coordinator)
    : coordinator_(std::move(coordinator)) {
  coordinator_->AddObserver(this);
  delegate_ = static_cast<ReadAnythingToolbarView::Delegate*>(
      coordinator_->GetController());
  auto* font_model = coordinator_->GetModel()->GetFontModel();

  // Create and set a BoxLayout LayoutManager for this view.
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  layout->set_inside_border_insets(gfx::Insets(kInternalInsets));

  SetLayoutManager(std::move(layout));

  // Create a font selection combobox for the toolbar.
  auto combobox = std::make_unique<views::Combobox>();
  combobox->SetCallback(
      base::BindRepeating(&ReadAnythingToolbarView::FontNameChangedCallback,
                          weak_pointer_factory_.GetWeakPtr()));
  combobox->SetSizeToLargestLabel(true);
  // TODO(1266555): This is placeholder text, remove for final UI.
  combobox->SetTooltipTextAndAccessibleName(u"Font Choice");
  combobox->SetModel(font_model);

  // Add all views as children.
  font_combobox_ = AddChildView(std::move(combobox));
}

void ReadAnythingToolbarView::FontNameChangedCallback() {
  if (delegate_)
    delegate_->OnFontChoiceChanged(font_combobox_->GetSelectedIndex());
}

void ReadAnythingToolbarView::OnCoordinatorDestroyed() {
  // When the coordinator that created |this| is destroyed, clean up pointers.
  coordinator_ = nullptr;
  delegate_ = nullptr;
  font_combobox_->SetModel(nullptr);
}

ReadAnythingToolbarView::~ReadAnythingToolbarView() {
  // If |this| is being destroyed before the associated coordinator, then
  // remove |this| as an observer.
  if (coordinator_) {
    coordinator_->RemoveObserver(this);
  }
}
