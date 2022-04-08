// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/read_later/side_panel/read_anything/read_anything_toolbar_view.h"

#include "chrome/browser/ui/webui/read_later/side_panel/read_anything/read_anything_constants.h"
#include "chrome/browser/ui/webui/read_later/side_panel/read_anything/read_anything_font_model.h"
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
    base::RepeatingCallback<void(const std::string&)> callback) {
  // Create and set a BoxLayout LayoutManager for this view.
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  layout->set_inside_border_insets(gfx::Insets(kInternalInsets));

  SetLayoutManager(std::move(layout));

  // Create a font selection combobox for the toolbar.
  auto combobox = std::make_unique<views::Combobox>(
      std::make_unique<ReadAnythingFontModel>());
  base::RepeatingClosure combobox_callback =
      base::BindRepeating(&ReadAnythingToolbarView::OnFontChanged,
                          weak_pointer_factory_.GetWeakPtr());
  combobox->SetSizeToLargestLabel(true);
  // TODO(1266555): This is placeholder text, remove for final UI.
  combobox->SetTooltipTextAndAccessibleName(u"Font Choice");
  combobox->SetCallback(combobox_callback);
  font_passthrough_ = std::move(callback);

  // Create a Settings button with a callback.
  views::Button::PressedCallback settings_callback =
      base::BindRepeating(&ReadAnythingToolbarView::OnSettingsClicked,
                          weak_pointer_factory_.GetWeakPtr());
  auto settings_button =
      views::CreateVectorImageButton(std::move(settings_callback));
  // TODO(1266555): This is placeholder text, remove for final UI.
  settings_button->SetTooltipText(u"Settings");
  views::InstallCircleHighlightPathGenerator(settings_button.get(),
                                             gfx::Insets(kIconCornerRadius));

  // Add all components to view.
  settings_button_ = AddChildView(std::move(settings_button));
  font_combobox_ = AddChildView(std::move(combobox));
}

void ReadAnythingToolbarView::OnThemeChanged() {
  views::View::OnThemeChanged();

  const auto* const cp = GetColorProvider();
  views::SetImageFromVectorIconWithColor(
      settings_button_, vector_icons::kSettingsIcon, kIconSize,
      cp->GetColor(ui::kColorIcon), cp->GetColor(ui::kColorIconDisabled));
}

void ReadAnythingToolbarView::OnSettingsClicked() {
  // TODO(1266555): Stubbed method, to be implemented.
}

// TODO(1266555): Move this logic into a controller that handles the component.
void ReadAnythingToolbarView::OnFontChanged() {
  auto* mymodel =
      static_cast<ReadAnythingFontModel*>(font_combobox_->GetModel());
  font_passthrough_.Run(
      mymodel->GetCurrentFontName(font_combobox_->GetSelectedIndex()));
}

ReadAnythingToolbarView::~ReadAnythingToolbarView() = default;
