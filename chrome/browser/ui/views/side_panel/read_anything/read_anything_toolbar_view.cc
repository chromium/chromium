// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_toolbar_view.h"

#include <memory>
#include <utility>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_menu_button.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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
#include "ui/views/layout/flex_layout.h"

ReadAnythingToolbarView::ReadAnythingToolbarView(
    ReadAnythingCoordinator* coordinator,
    ReadAnythingToolbarView::Delegate* toolbar_delegate,
    ReadAnythingFontCombobox::Delegate* font_combobox_delegate)
    : delegate_(toolbar_delegate), coordinator_(std::move(coordinator)) {
  coordinator_->AddObserver(this);

  // Set a FlexLayout LayoutManager for this view.
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(views::kMarginsKey, gfx::Insets::VH(0, kButtonPadding))
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetInteriorMargin(gfx::Insets(kInternalInsets));

  // Create a font selection combobox for the toolbar. The font combobox uses
  // a custom MenuModel, so we have a separate View for it for convenience.
  auto combobox =
      std::make_unique<ReadAnythingFontCombobox>(font_combobox_delegate);

  // Font combobox should shrink as panel gets smaller.
  combobox->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum));

  // Create the decrease/increase text size buttons.
  auto decrease_size_button = std::make_unique<ReadAnythingButtonView>(
      base::BindRepeating(&ReadAnythingToolbarView::DecreaseFontSizeCallback,
                          weak_pointer_factory_.GetWeakPtr()),
      kTextDecreaseIcon, kIconSize, gfx::kPlaceholderColor,
      l10n_util::GetStringUTF16(
          IDS_READING_MODE_DECREASE_FONT_SIZE_BUTTON_LABEL));

  auto increase_size_button = std::make_unique<ReadAnythingButtonView>(
      base::BindRepeating(&ReadAnythingToolbarView::IncreaseFontSizeCallback,
                          weak_pointer_factory_.GetWeakPtr()),
      kTextIncreaseIcon, kIconSize, gfx::kPlaceholderColor,
      l10n_util::GetStringUTF16(
          IDS_READING_MODE_INCREASE_FONT_SIZE_BUTTON_LABEL));

  // Create theme selection menubutton.
  auto colors_button = std::make_unique<ReadAnythingMenuButton>(
      base::BindRepeating(&ReadAnythingToolbarView::ChangeColorsCallback,
                          weak_pointer_factory_.GetWeakPtr()),
      kPaletteIcon,
      l10n_util::GetStringUTF16(IDS_READING_MODE_COLORS_COMBOBOX_LABEL),
      delegate_->GetColorsModel());

  // Create line spacing menubutton.
  auto line_spacing_button = std::make_unique<ReadAnythingMenuButton>(
      base::BindRepeating(&ReadAnythingToolbarView::ChangeLineSpacingCallback,
                          weak_pointer_factory_.GetWeakPtr()),
      kLineSpacingIcon,
      l10n_util::GetStringUTF16(IDS_READING_MODE_LINE_SPACING_COMBOBOX_LABEL),
      delegate_->GetLineSpacingModel());

  // Create letter spacing menubutton.
  auto letter_spacing_button = std::make_unique<ReadAnythingMenuButton>(
      base::BindRepeating(&ReadAnythingToolbarView::ChangeLetterSpacingCallback,
                          weak_pointer_factory_.GetWeakPtr()),
      kLetterSpacingIcon,
      l10n_util::GetStringUTF16(IDS_READING_MODE_LETTER_SPACING_COMBOBOX_LABEL),
      delegate_->GetLetterSpacingModel());

  // Add all views as children.
  font_combobox_ = AddChildView(std::move(combobox));
  AddChildView(Separator());
  decrease_text_size_button_ = AddChildView(std::move(decrease_size_button));
  increase_text_size_button_ = AddChildView(std::move(increase_size_button));
  AddChildView(Separator());
  colors_button_ = AddChildView(std::move(colors_button));
  line_spacing_button_ = AddChildView(std::move(line_spacing_button));
  letter_spacing_button_ = AddChildView(std::move(letter_spacing_button));

  // Start observing model after views creation so initial theme is applied.
  coordinator_->AddModelObserver(this);
}

void ReadAnythingToolbarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  if (delegate_) {
    delegate_->OnSystemThemeChanged();
  }
}

// After this view is added to the widget, we have access to the color provider
// to apply the initial theme skcolors.
void ReadAnythingToolbarView::AddedToWidget() {
  ChangeColorsCallback();
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
    delegate_->OnColorsChanged(colors_button_->GetSelectedIndex().value_or(0));
}

void ReadAnythingToolbarView::ChangeLineSpacingCallback() {
  if (delegate_)
    delegate_->OnLineSpacingChanged(
        line_spacing_button_->GetSelectedIndex().value_or(1));
}

void ReadAnythingToolbarView::ChangeLetterSpacingCallback() {
  if (delegate_)
    delegate_->OnLetterSpacingChanged(
        letter_spacing_button_->GetSelectedIndex().value_or(0));
}

void ReadAnythingToolbarView::OnCoordinatorDestroyed() {
  // When the coordinator that created |this| is destroyed, clean up pointers.
  coordinator_ = nullptr;
  delegate_ = nullptr;
  font_combobox_->SetModel(nullptr);
  colors_button_->SetMenuModel(nullptr);
  line_spacing_button_->SetMenuModel(nullptr);
  letter_spacing_button_->SetMenuModel(nullptr);
}

void ReadAnythingToolbarView::OnReadAnythingThemeChanged(
    const std::string& font_name,
    double font_scale,
    ui::ColorId foreground_color_id,
    ui::ColorId background_color_id,
    ui::ColorId separator_color_id,
    ui::ColorId dropdown_color_id,
    read_anything::mojom::LineSpacing line_spacing,
    read_anything::mojom::LetterSpacing letter_spacing) {
  if (font_scale > kReadAnythingMinimumFontScale) {
    decrease_text_size_button_->Enable();
  } else {
    decrease_text_size_button_->Disable();
  }
  if (font_scale < kReadAnythingMaximumFontScale) {
    increase_text_size_button_->Enable();
  } else {
    increase_text_size_button_->Disable();
  }

  if (!GetColorProvider())
    return;

  SetBackground(views::CreateThemedSolidBackground(background_color_id));
  font_combobox_->SetBackgroundColorId(background_color_id);
  colors_button_->SetBackground(
      views::CreateThemedSolidBackground(background_color_id));
  line_spacing_button_->SetBackground(
      views::CreateThemedSolidBackground(background_color_id));
  letter_spacing_button_->SetBackground(
      views::CreateThemedSolidBackground(background_color_id));

  decrease_text_size_button_->UpdateIcon(kTextDecreaseIcon, kIconSize,
                                         foreground_color_id);

  increase_text_size_button_->UpdateIcon(kTextIncreaseIcon, kIconSize,
                                         foreground_color_id);

  colors_button_->SetIcon(kPaletteIcon, kIconSize, foreground_color_id);

  line_spacing_button_->SetIcon(kLineSpacingIcon, kIconSize,
                                foreground_color_id);
  letter_spacing_button_->SetIcon(kLetterSpacingIcon, kIconSize,
                                  foreground_color_id);

  // Update the background colors for the dropdowns.
  colors_button_->SetDropdownColors(dropdown_color_id, foreground_color_id);
  letter_spacing_button_->SetDropdownColors(dropdown_color_id,
                                            foreground_color_id);
  line_spacing_button_->SetDropdownColors(dropdown_color_id,
                                          foreground_color_id);
  font_combobox_->SetDropdownColors(dropdown_color_id, foreground_color_id);

  for (views::Separator* separator : separators_) {
    separator->SetColorId(separator_color_id);
  }

  font_combobox_->SetForegroundColorId(foreground_color_id);
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
  separators_.push_back(
      separator_container->AddChildView(std::move(separator)));

  return separator_container;
}

void ReadAnythingToolbarView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kToolbar;
  node_data->SetNameChecked(
      l10n_util::GetStringUTF16(IDS_READING_MODE_TOOLBAR_LABEL));
}

BEGIN_METADATA(ReadAnythingToolbarView, views::View)
END_METADATA

ReadAnythingToolbarView::~ReadAnythingToolbarView() {
  // If |this| is being destroyed before the associated coordinator, then
  // remove |this| as an observer.
  if (coordinator_) {
    coordinator_->RemoveObserver(this);
    coordinator_->RemoveModelObserver(this);
  }
}
