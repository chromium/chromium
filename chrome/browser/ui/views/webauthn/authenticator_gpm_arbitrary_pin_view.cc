// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_gpm_arbitrary_pin_view.h"

#include <string>

#include "chrome/browser/ui/views/webauthn/reveal_button_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"

namespace {
constexpr int kBetweenChildSpacing = 8;
constexpr int kPinTextfieldWidthInChars = 25;
}  // namespace

AuthenticatorGPMArbitraryPinView::AuthenticatorGPMArbitraryPinView(
    bool ui_disabled,
    const std::u16string& pin,
    const std::u16string& pin_accessible_name,
    const std::u16string& pin_accessible_description,
    Delegate* delegate)
    : delegate_(delegate) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_between_child_spacing(kBetweenChildSpacing);

  auto pin_textfield = std::make_unique<views::Textfield>();
  pin_textfield->SetController(this);
  pin_textfield->GetViewAccessibility().SetName(pin_accessible_name);
  if (!pin_accessible_description.empty()) {
    pin_textfield->GetViewAccessibility().SetDescription(
        pin_accessible_description);
  }
  pin_textfield->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  pin_textfield->SetDefaultWidthInChars(kPinTextfieldWidthInChars);
  pin_textfield->SetReadOnly(ui_disabled);
  pin_textfield->SetText(pin);
  pin_textfield->SetEnabled(!ui_disabled);
  pin_textfield_ = AddChildView(std::move(pin_textfield));

  reveal_button_ = AddChildView(CreateRevealButton(base::BindRepeating(
      &AuthenticatorGPMArbitraryPinView::OnRevealButtonClicked,
      base::Unretained(this))));
  reveal_button_->SetEnabled(!ui_disabled);
}

AuthenticatorGPMArbitraryPinView::~AuthenticatorGPMArbitraryPinView() = default;

void AuthenticatorGPMArbitraryPinView::OnRevealButtonClicked() {
  pin_revealed_ = !pin_revealed_;
  reveal_button_->SetToggled(pin_revealed_);
  pin_textfield_->SetTextInputType(
      pin_revealed_ ? ui::TEXT_INPUT_TYPE_TEXT : ui::TEXT_INPUT_TYPE_PASSWORD);
}

void AuthenticatorGPMArbitraryPinView::RequestFocus() {
  pin_textfield_->RequestFocus();
}

void AuthenticatorGPMArbitraryPinView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  delegate_->OnPinChanged(new_contents);
}

BEGIN_METADATA(AuthenticatorGPMArbitraryPinView)
END_METADATA
