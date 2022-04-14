// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_toolbar_view.h"

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_constants.h"
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
    ReadAnythingToolbarView::Delegate* delegate) {
  delegate_ = delegate;

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

  // Add all views as children.
  font_combobox_ = AddChildView(std::move(combobox));
}

void ReadAnythingToolbarView::SetFontModel(ui::ComboboxModel* model) {
  font_combobox_->SetModel(model);
}

void ReadAnythingToolbarView::FontNameChangedCallback() {
  delegate_->OnFontChoiceChanged(font_combobox_->GetSelectedIndex());
}

ReadAnythingToolbarView::~ReadAnythingToolbarView() = default;
