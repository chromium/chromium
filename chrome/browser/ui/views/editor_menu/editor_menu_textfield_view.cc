// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_textfield_view.h"

#include <algorithm>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace chromeos::editor_menu {

namespace {

constexpr char16_t kContainerTitle[] = u"Editor Menu Textfield";

constexpr int kConatinerHeightDip = 30;
constexpr int kBackgroundRadiusDip = 8;
constexpr gfx::Insets kContainerInsets = gfx::Insets::TLBR(0, 16, 0, 6);
constexpr int kTextIconSpacingDip = 8;
constexpr int kButtonSizeDip = 32;

}  // namespace

EditorMenuTextfieldView::EditorMenuTextfieldView() = default;
EditorMenuTextfieldView::~EditorMenuTextfieldView() = default;

void EditorMenuTextfieldView::AddedToWidget() {
  // Only initialize the view after it is added to a widget.
  InitLayout();
}

int EditorMenuTextfieldView::GetHeightForWidth(int width) const {
  return kConatinerHeightDip;
}

void EditorMenuTextfieldView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kDialog;
  node_data->SetName(kContainerTitle);
}

void EditorMenuTextfieldView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  arrow_button_->SetVisible(!new_contents.empty());
}

void EditorMenuTextfieldView::InitLayout() {
  SetBackground(views::CreateThemedRoundedRectBackground(
      static_cast<ui::ColorId>(cros_tokens::kCrosSysSystemBaseElevated),
      kBackgroundRadiusDip));
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kContainerInsets,
      kTextIconSpacingDip));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  textfield_ = AddChildView(std::make_unique<views::Textfield>());
  textfield_->SetAccessibleName(kContainerTitle);
  textfield_->set_controller(this);
  textfield_->SetBorder(views::NullBorder());
  textfield_->SetBackgroundColor(SK_ColorTRANSPARENT);
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT);
  textfield_->SetPlaceholderText(kContainerTitle);
  layout->SetFlexForView(textfield_, 1, /*use_min_size=*/true);

  auto* button_container = AddChildView(std::make_unique<views::View>());
  button_container->SetLayoutManager(std::make_unique<views::FillLayout>());
  button_container->SetPreferredSize(gfx::Size(kButtonSizeDip, kButtonSizeDip));

  arrow_button_ =
      button_container->AddChildView(std::make_unique<views::ImageButton>());
  arrow_button_->SetAccessibleName(kContainerTitle);
  arrow_button_->SetTooltipText(kContainerTitle);
  arrow_button_->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kProductIcon));
  arrow_button_->SetVisible(false);
}

BEGIN_METADATA(EditorMenuTextfieldView, views::View)
END_METADATA

}  // namespace chromeos::editor_menu
