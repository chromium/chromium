// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_button_view.h"

#include "chrome/common/accessibility/read_anything_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"

ReadAnythingButtonView::ReadAnythingButtonView(
    views::ImageButton::PressedCallback callback,
    const gfx::VectorIcon& icon,
    int icon_size,
    SkColor icon_color,
    const std::u16string& tooltip) {
  // Create and set a BoxLayout with insets to hold the button.
  auto button_layout_manager = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  button_layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  button_layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);

  SetLayoutManager(std::move(button_layout_manager));

  // Create the image button.
  auto button = views::CreateVectorImageButton(std::move(callback));
  views::SetImageFromVectorIconWithColor(button.get(), icon, icon_size,
                                         icon_color, icon_color);
  views::InstallCircleHighlightPathGenerator(button.get());
  button->SetTooltipText(tooltip);

  // Add the button to the view.
  button_ = AddChildView(std::move(button));
}

void ReadAnythingButtonView::UpdateIcon(const gfx::VectorIcon& icon,
                                        int icon_size,
                                        SkColor icon_color) {
  views::SetImageFromVectorIconWithColor(button_, icon, icon_size, icon_color,
                                         icon_color);
}

BEGIN_METADATA(ReadAnythingButtonView, views::View)
END_METADATA

ReadAnythingButtonView::~ReadAnythingButtonView() = default;
