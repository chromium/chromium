// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_client_pin_entry_view.h"

#include <memory>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/style/typography.h"

namespace {

std::unique_ptr<views::Textfield> MakePinTextField(
    views::TextfieldController* controller,
    views::View* label) {
  constexpr int kMinWidthInChars = 6;
  constexpr int kDefaultWidthInChars = 20;
  constexpr int kBottomBorderThickness = 2;

  auto field = std::make_unique<views::Textfield>();
  field->SetTextInputType(ui::TextInputType::TEXT_INPUT_TYPE_PASSWORD);
  field->SetMinimumWidthInChars(kMinWidthInChars);
  field->SetDefaultWidthInChars(kDefaultWidthInChars);
  field->SetBorder(views::CreateSolidSidedBorder(0, 0, kBottomBorderThickness,
                                                 0, gfx::kGoogleBlue500));
  field->set_controller(controller);
  field->SetAssociatedLabel(label);
  return field;
}

}  // namespace

AuthenticatorClientPinEntryView::AuthenticatorClientPinEntryView(
    Delegate* delegate,
    bool show_confirmation_text_field)
    : delegate_(delegate),
      show_confirmation_text_field_(show_confirmation_text_field) {
  auto* layout = SetLayoutManager(std::make_unique<views::TableLayout>());

  layout
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kStart,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::GridLayout::kFixedSize, 10)
      .AddColumn(views::LayoutAlignment::kStart, views::LayoutAlignment::kStart,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(2, views::TableLayout::kFixedSize);

  auto* pin_label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_ENTRY_PIN_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY));
  pin_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  pin_label->SetEnabledColor(gfx::kGoogleBlue500);

  views::View* confirmation_label_ptr = nullptr;
  if (show_confirmation_text_field_) {
    auto confirmation_label = std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_SETUP_CONFIRMATION_LABEL),
        views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY);
    confirmation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    confirmation_label->SetEnabledColor(gfx::kGoogleBlue500);
    confirmation_label_ptr = AddChildView(std::move(confirmation_label));
  } else {
    // For TableLayout, we must add a filler view to the empty cell.
    AddChildView(std::make_unique<views::View>());
  }

  pin_text_field_ = AddChildView(MakePinTextField(this, pin_label));

  if (show_confirmation_text_field_) {
    DCHECK(confirmation_label_ptr);
    auto confirmation_text_field =
        MakePinTextField(this, confirmation_label_ptr);
    confirmation_text_field_ = AddChildView(std::move(confirmation_text_field));
  } else {
    AddChildView(std::make_unique<views::View>());
  }
}

AuthenticatorClientPinEntryView::~AuthenticatorClientPinEntryView() = default;

void AuthenticatorClientPinEntryView::RequestFocus() {
  pin_text_field_->RequestFocus();
}

void AuthenticatorClientPinEntryView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  DCHECK(sender == pin_text_field_ || sender == confirmation_text_field_);

  if (sender == pin_text_field_) {
    delegate_->OnPincodeChanged(new_contents);
  } else {
    delegate_->OnConfirmationChanged(new_contents);
  }
}

bool AuthenticatorClientPinEntryView::HandleKeyEvent(
    views::Textfield* sender,
    const ui::KeyEvent& key_event) {
  // As WebAuthN UI views do not intercept any key events, the key event must
  // be further processed.
  return false;
}

BEGIN_METADATA(AuthenticatorClientPinEntryView, views::View)
END_METADATA
