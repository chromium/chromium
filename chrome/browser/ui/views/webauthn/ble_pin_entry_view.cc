// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/ble_pin_entry_view.h"

#include <memory>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
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

constexpr int kExpectedPincodeCharLength = 6;
constexpr int kPreferredTextfieldCharLength = 20;
constexpr int kTextfieldBottomBorderThickness = 2;

}  // namespace

BlePinEntryView::BlePinEntryView(Delegate* delegate) : delegate_(delegate) {
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  SetLayoutManager(std::move(layout));

  auto textfield_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_PIN_ENTRY_PIN_LABEL),
      views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY);
  textfield_label->SetMultiLine(true);
  textfield_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  textfield_label->SetEnabledColor(gfx::kGoogleBlue500);
  auto* textfield_label_ptr = textfield_label.release();
  AddChildView(textfield_label_ptr);

  pin_text_field_ = new views::Textfield();
  pin_text_field_->SetMinimumWidthInChars(kExpectedPincodeCharLength);
  pin_text_field_->SetDefaultWidthInChars(kPreferredTextfieldCharLength);
  pin_text_field_->SetBorder(views::CreateSolidSidedBorder(
      0, 0, kTextfieldBottomBorderThickness, 0, gfx::kGoogleBlue500));
  pin_text_field_->set_controller(this);
  pin_text_field_->SetAssociatedLabel(textfield_label_ptr);
  AddChildView(pin_text_field_);
}

BlePinEntryView::~BlePinEntryView() = default;

void BlePinEntryView::RequestFocus() {
  pin_text_field_->RequestFocus();
}

void BlePinEntryView::ContentsChanged(views::Textfield* sender,
                                      const base::string16& new_contents) {
  DCHECK_EQ(pin_text_field_, sender);

  int64_t received_pin;
  if (new_contents.size() > kExpectedPincodeCharLength ||
      (!new_contents.empty() &&
       !base::StringToInt64(new_contents, &received_pin))) {
    sender->SetInvalid(true);
  } else {
    sender->SetInvalid(false);
    delegate_->OnPincodeChanged(new_contents);
  }
}

bool BlePinEntryView::HandleKeyEvent(views::Textfield* sender,
                                     const ui::KeyEvent& key_event) {
  // As WebAuthN UI views do not intercept any key events, the key event must
  // be further processed.
  return false;
}
