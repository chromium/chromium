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
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
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
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* columns = layout->AddColumnSet(0);

  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                     views::GridLayout::kFixedSize, views::GridLayout::USE_PREF,
                     0, 0);
  columns->AddPaddingColumn(views::GridLayout::kFixedSize, 10);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                     views::GridLayout::kFixedSize, views::GridLayout::USE_PREF,
                     0, 0);

  layout->StartRow(views::GridLayout::kFixedSize, 0);

  auto pin_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_ENTRY_PIN_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY);
  pin_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  pin_label->SetEnabledColor(gfx::kGoogleBlue500);
  auto* pin_label_ptr = layout->AddView(std::move(pin_label));

  views::View* confirmation_label_ptr = nullptr;
  if (show_confirmation_text_field_) {
    auto confirmation_label = std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_SETUP_CONFIRMATION_LABEL),
        views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY);
    confirmation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    confirmation_label->SetEnabledColor(gfx::kGoogleBlue500);
    confirmation_label_ptr = confirmation_label.get();
    layout->AddView(std::move(confirmation_label));
  }

  layout->StartRow(views::GridLayout::kFixedSize, 0);

  pin_text_field_ = layout->AddView(MakePinTextField(this, pin_label_ptr));

  if (show_confirmation_text_field_) {
    DCHECK(confirmation_label_ptr);
    auto confirmation_text_field =
        MakePinTextField(this, confirmation_label_ptr);
    confirmation_text_field_ = confirmation_text_field.get();
    layout->AddView(std::move(confirmation_text_field));
  }

  layout->StartRow(views::GridLayout::kFixedSize, 0);

  auto error_label = std::make_unique<views::Label>(
      base::string16(), views::style::CONTEXT_LABEL, STYLE_RED);
  error_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  error_label_ = layout->AddView(std::move(error_label), 3 /* col_span */,
                                 1 /* row_span */);
}

AuthenticatorClientPinEntryView::~AuthenticatorClientPinEntryView() = default;

void AuthenticatorClientPinEntryView::UpdateError(const base::string16& text) {
  error_label_->SetVisible(true);
  error_label_->SetText(text);
  error_label_->SizeToPreferredSize();
  Layout();
}

void AuthenticatorClientPinEntryView::RequestFocus() {
  pin_text_field_->RequestFocus();
}

void AuthenticatorClientPinEntryView::ContentsChanged(
    views::Textfield* sender,
    const base::string16& new_contents) {
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
