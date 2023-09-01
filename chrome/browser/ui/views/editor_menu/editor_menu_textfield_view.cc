// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_textfield_view.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_view_delegate.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_view.h"
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
constexpr int kBorderThicknessDip = 1;

}  // namespace

EditorMenuTextfieldView::EditorMenuTextfieldView(
    EditorMenuViewDelegate* delegate)
    : delegate_(delegate) {
  CHECK(delegate_);
}

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

bool EditorMenuTextfieldView::HandleKeyEvent(views::Textfield* sender,
                                             const ui::KeyEvent& key_event) {
  if (key_event.key_code() != ui::VKEY_RETURN ||
      key_event.type() != ui::ET_KEY_PRESSED) {
    return false;
  }

  OnTextfieldArrowButtonPressed();
  return true;
}

void EditorMenuTextfieldView::InitLayout() {
  SetBackground(views::CreateThemedRoundedRectBackground(
      static_cast<ui::ColorId>(cros_tokens::kCrosSysSystemBaseElevated),
      kBackgroundRadiusDip));
  SetBorder(views::CreateThemedRoundedRectBorder(
      kBorderThicknessDip, kBackgroundRadiusDip, ui::kColorSysNeutralOutline));

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

  arrow_button_ =
      AddChildView(std::make_unique<views::ImageButton>(base::BindRepeating(
          &EditorMenuTextfieldView::OnTextfieldArrowButtonPressed,
          weak_factory_.GetWeakPtr())));
  arrow_button_->SetAccessibleName(kContainerTitle);
  arrow_button_->SetTooltipText(kContainerTitle);
  arrow_button_->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kForwardArrowIcon));
  arrow_button_->SetImageHorizontalAlignment(
      views::ImageButton::HorizontalAlignment::ALIGN_CENTER);
  arrow_button_->SetImageVerticalAlignment(
      views::ImageButton::VerticalAlignment::ALIGN_MIDDLE);
  arrow_button_->SetPreferredSize(gfx::Size(kButtonSizeDip, kButtonSizeDip));
  arrow_button_->SetVisible(false);
  views::InkDrop::Get(arrow_button_)
      ->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(arrow_button_)->SetBaseColorId(ui::kColorIcon);
  arrow_button_->SetHasInkDropActionOnClick(true);
}

void EditorMenuTextfieldView::OnTextfieldArrowButtonPressed() {
  CHECK(delegate_);
  delegate_->OnTextfieldArrowButtonPressed(textfield_->GetText());
}

BEGIN_METADATA(EditorMenuTextfieldView, views::View)
END_METADATA

}  // namespace chromeos::editor_menu
