// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_client_pin_entry_view.h"

#include <memory>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/style/typography.h"

namespace {

class PinTextfield : public views::Textfield {
  METADATA_HEADER(PinTextfield, views::Textfield)

 public:
  PinTextfield(views::TextfieldController* controller, views::View* label) {
    SetTextInputType(ui::TextInputType::TEXT_INPUT_TYPE_PASSWORD);
    SetMinimumWidthInChars(6);
    SetDefaultWidthInChars(20);

    set_controller(controller);
    GetViewAccessibility().SetName(*label);
  }
  PinTextfield(const PinTextfield&) = delete;
  PinTextfield& operator=(const PinTextfield&) = delete;
  ~PinTextfield() override = default;

  void OnThemeChanged() override {
    views::Textfield::OnThemeChanged();
    constexpr int kBottomBorderThickness = 2;
    SetBorder(views::CreateSolidSidedBorder(
        gfx::Insets::TLBR(0, 0, kBottomBorderThickness, 0),
        GetColorProvider()->GetColor(kColorWebAuthnPinTextfieldBottomBorder)));
  }
};

BEGIN_METADATA(PinTextfield)
END_METADATA

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
      .AddPaddingColumn(views::TableLayout::kFixedSize, 10)
      .AddColumn(views::LayoutAlignment::kStart, views::LayoutAlignment::kStart,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(2, views::TableLayout::kFixedSize);

  pin_label_ = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_ENTRY_PIN_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY));
  pin_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  if (show_confirmation_text_field_) {
    confirmation_label_ = AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_PIN_SETUP_CONFIRMATION_LABEL),
        views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY));
    confirmation_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  } else {
    // For TableLayout, we must add a filler view to the empty cell.
    AddChildView(std::make_unique<views::View>());
  }

  pin_text_field_ =
      AddChildView(std::make_unique<PinTextfield>(this, pin_label_));

  if (show_confirmation_text_field_) {
    DCHECK(confirmation_label_);
    confirmation_text_field_ =
        AddChildView(std::make_unique<PinTextfield>(this, confirmation_label_));
  } else {
    AddChildView(std::make_unique<views::View>());
  }
}

AuthenticatorClientPinEntryView::~AuthenticatorClientPinEntryView() = default;

void AuthenticatorClientPinEntryView::RequestFocus() {
  pin_text_field_->RequestFocus();
}

void AuthenticatorClientPinEntryView::OnThemeChanged() {
  views::View::OnThemeChanged();
  const auto* const color_provider = GetColorProvider();
  const SkColor label_color = color_provider->GetColor(ui::kColorAccent);
  pin_label_->SetEnabledColor(label_color);
  if (confirmation_label_) {
    confirmation_label_->SetEnabledColor(label_color);
  }
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

BEGIN_METADATA(AuthenticatorClientPinEntryView)
END_METADATA
