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
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace chromeos::editor_menu {

namespace {

constexpr char16_t kContainerTitle[] = u"Editor Menu Textfield";

constexpr gfx::Size kArrowButtonSize(20, 20);
constexpr gfx::Insets kArrowButtonInsets(4);
constexpr int kPaddingBetweenArrowButtonAndTextfield = 10;

}  // namespace

EditorMenuTextfieldView::EditorMenuTextfieldView(
    EditorMenuMode editor_menu_mode,
    EditorMenuViewDelegate* delegate)
    : editor_menu_mode_(editor_menu_mode), delegate_(delegate) {
  CHECK(delegate_);
}

EditorMenuTextfieldView::~EditorMenuTextfieldView() = default;

void EditorMenuTextfieldView::AddedToWidget() {
  // Only initialize the view after it is added to a widget.
  InitLayout();
}

void EditorMenuTextfieldView::Layout() {
  View::Layout();

  // Vertically center the arrow button at the right end of the textfield.
  arrow_button_->SetBounds(
      width() - (kArrowButtonSize.width() + kArrowButtonInsets.right() +
                 kPaddingBetweenArrowButtonAndTextfield),
      height() / 2 -
          (kArrowButtonSize.height() + kArrowButtonInsets.height()) / 2,
      kArrowButtonSize.width() + kArrowButtonInsets.width(),
      kArrowButtonSize.height() + kArrowButtonInsets.height());
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
  SetLayoutManager(std::make_unique<views::FillLayout>());

  textfield_ = AddChildView(std::make_unique<views::Textfield>());
  textfield_->set_controller(this);
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT);
  // TODO:b:302404392 - Consider removing the line below after fixing the autocorrect crash
  // issue in native views
  textfield_->SetTextInputFlags(ui::TEXT_INPUT_FLAG_AUTOCORRECT_OFF);
  textfield_->SetAccessibleName(kContainerTitle);
  textfield_->SetPlaceholderText(l10n_util::GetStringUTF16(
      editor_menu_mode_ == EditorMenuMode::kWrite
          ? IDS_EDITOR_MENU_WRITE_CARD_FREEFORM_PLACEHOLDER
          : IDS_EDITOR_MENU_REWRITE_CARD_FREEFORM_PLACEHOLDER));
  textfield_->SetBackgroundColor(SK_ColorTRANSPARENT);
  textfield_->RemoveHoverEffect();
  textfield_->SetExtraInsets(gfx::Insets::TLBR(
      0, 0, 0, kArrowButtonSize.width() + kArrowButtonInsets.width()));

  arrow_button_ = AddChildView(views::ImageButton::CreateIconButton(
      base::BindRepeating(
          &EditorMenuTextfieldView::OnTextfieldArrowButtonPressed,
          weak_factory_.GetWeakPtr()),
      vector_icons::kForwardArrowIcon,
      l10n_util::GetStringUTF16(
          IDS_EDITOR_MENU_FREEFORM_TEXTFIELD_ARROW_BUTTON_TOOLTIP)));
  arrow_button_->SetImageHorizontalAlignment(
      views::ImageButton::HorizontalAlignment::ALIGN_CENTER);
  arrow_button_->SetImageVerticalAlignment(
      views::ImageButton::VerticalAlignment::ALIGN_MIDDLE);
  arrow_button_->SetVisible(false);
}

void EditorMenuTextfieldView::OnTextfieldArrowButtonPressed() {
  CHECK(delegate_);
  delegate_->OnTextfieldArrowButtonPressed(textfield_->GetText());
}

BEGIN_METADATA(EditorMenuTextfieldView, views::View)
END_METADATA

}  // namespace chromeos::editor_menu
