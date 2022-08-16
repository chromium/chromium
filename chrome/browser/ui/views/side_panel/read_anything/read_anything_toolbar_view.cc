// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_toolbar_view.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/cc_macros.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/geometry_export.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_types.h"

ReadAnythingToolbarView::ReadAnythingToolbarView(
    ReadAnythingCoordinator* coordinator,
    ReadAnythingToolbarView::Delegate* toolbar_delegate,
    ReadAnythingFontCombobox::Delegate* font_combobox_delegate)
    : delegate_(toolbar_delegate), coordinator_(std::move(coordinator)) {
  coordinator_->AddObserver(this);

  // Create and set a BoxLayout LayoutManager for this view.
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  layout->set_inside_border_insets(gfx::Insets(kInternalInsets));

  SetLayoutManager(std::move(layout));

  // Create a font selection combobox for the toolbar. The font combobox uses
  // a custom MenuModel, so we have a separate View for it for convenience.
  auto combobox =
      std::make_unique<ReadAnythingFontCombobox>(font_combobox_delegate);

  // Create the decrease/increase text size buttons.
  auto decrease_size_button = std::make_unique<ReadAnythingButtonView>(
      base::BindRepeating(&ReadAnythingToolbarView::DecreaseFontSizeCallback,
                          weak_pointer_factory_.GetWeakPtr()),
      gfx::CreateVectorIcon(vector_icons::kTextDecreaseIcon, kSmallIconSize,
                            gfx::kPlaceholderColor),
      l10n_util::GetStringUTF16(
          IDS_READ_ANYTHING_DECREASE_FONT_SIZE_BUTTON_LABEL));

  auto increase_size_button = std::make_unique<ReadAnythingButtonView>(
      base::BindRepeating(&ReadAnythingToolbarView::IncreaseFontSizeCallback,
                          weak_pointer_factory_.GetWeakPtr()),
      gfx::CreateVectorIcon(vector_icons::kTextIncreaseIcon, kLargeIconSize,
                            gfx::kPlaceholderColor),
      l10n_util::GetStringUTF16(
          IDS_READ_ANYTHING_INCREASE_FONT_SIZE_BUTTON_LABEL));

  // Create theme selection combobox.
  auto colors_combobox = std::make_unique<views::Combobox>();
  colors_combobox->SetModel(delegate_->GetColorsModel());
  colors_combobox->SetTooltipTextAndAccessibleName(
      l10n_util::GetStringUTF16(IDS_READ_ANYTHING_COLORS_COMBOBOX_LABEL));
  colors_combobox->SetSizeToLargestLabel(true);
  colors_combobox->SetCallback(
      base::BindRepeating(&ReadAnythingToolbarView::ChangeColorsCallback,
                          weak_pointer_factory_.GetWeakPtr()));
  colors_combobox->SetShouldShowArrow(false);
  colors_combobox->SetBorderColorId(ui::kColorSidePanelComboboxBorder);

  // Add all views as children.
  font_combobox_ = AddChildView(std::move(combobox));
  AddChildView(Separator());
  decrease_text_size_button_ = AddChildView(std::move(decrease_size_button));
  increase_text_size_button_ = AddChildView(std::move(increase_size_button));
  AddChildView(Separator());
  colors_combobox_ = AddChildView(std::move(colors_combobox));

  // Start observing model after views creation so initial theme is applied.
  coordinator_->AddModelObserver(this);
}

void ReadAnythingToolbarView::DecreaseFontSizeCallback() {
  if (delegate_)
    delegate_->OnFontSizeChanged(/* increase = */ false);
}

void ReadAnythingToolbarView::IncreaseFontSizeCallback() {
  if (delegate_)
    delegate_->OnFontSizeChanged(/* increase = */ true);
}

void ReadAnythingToolbarView::ChangeColorsCallback() {
  if (delegate_)
    delegate_->OnColorsChanged(
        colors_combobox_->GetSelectedIndex().value_or(0));
}

void ReadAnythingToolbarView::OnCoordinatorDestroyed() {
  // When the coordinator that created |this| is destroyed, clean up pointers.
  coordinator_ = nullptr;
  delegate_ = nullptr;
  font_combobox_->SetModel(nullptr);
}

void ReadAnythingToolbarView::OnReadAnythingThemeChanged(
    read_anything::mojom::ReadAnythingThemePtr new_theme) {
  SetBackground(views::CreateSolidBackground(new_theme->background_color));
  font_combobox_->SetBackground(
      views::CreateSolidBackground(new_theme->background_color));
  colors_combobox_->SetBackground(
      views::CreateSolidBackground(new_theme->background_color));

  decrease_text_size_button_->UpdateIcon(
      gfx::CreateVectorIcon(vector_icons::kTextDecreaseIcon, kSmallIconSize,
                            new_theme->foreground_color));

  increase_text_size_button_->UpdateIcon(
      gfx::CreateVectorIcon(vector_icons::kTextIncreaseIcon, kLargeIconSize,
                            new_theme->foreground_color));
}

std::unique_ptr<views::View> ReadAnythingToolbarView::Separator() {
  // Create a simple separator with padding to be inserted into views.
  auto separator_container = std::make_unique<views::View>();

  auto separator_layout_manager = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  separator_layout_manager->set_inside_border_insets(
      gfx::Insets(kButtonPadding)
          .set_top(kSeparatorTopBottomPadding)
          .set_bottom(kSeparatorTopBottomPadding));

  separator_container->SetLayoutManager(std::move(separator_layout_manager));

  auto separator = std::make_unique<views::Separator>();
  separator->SetColorId(ui::kColorMenuSeparator);

  separator_container->AddChildView(std::move(separator));

  return separator_container;
}

void ReadAnythingToolbarView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kToolbar;
  node_data->SetDescription(
      l10n_util::GetStringUTF16(IDS_READ_ANYTHING_TOOLBAR_LABEL));
}

ReadAnythingToolbarView::~ReadAnythingToolbarView() {
  // If |this| is being destroyed before the associated coordinator, then
  // remove |this| as an observer.
  if (coordinator_) {
    coordinator_->RemoveObserver(this);
    coordinator_->RemoveModelObserver(this);
  }
}
