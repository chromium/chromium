// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_gpm_arbitrary_pin_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/vector_icons.h"

namespace {

constexpr int kEyePaddingWidth = 4;

// Creates the eye icon button that toggles the pin visibility.
std::unique_ptr<views::ToggleImageButton> CreateRevealButton(
    views::ImageButton::PressedCallback callback) {
  auto button =
      views::Builder<views::ToggleImageButton>()
          .SetInstallFocusRingOnFocus(true)
          .SetRequestFocusOnPress(true)
          .SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE)
          .SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER)
          .SetCallback(std::move(callback))
          .SetBorder(views::CreateEmptyBorder(kEyePaddingWidth))
          .Build();
  SetImageFromVectorIconWithColorId(button.get(), views::kEyeIcon,
                                    ui::kColorIcon, ui::kColorIconDisabled);
  SetToggledImageFromVectorIconWithColorId(button.get(), views::kEyeCrossedIcon,
                                           ui::kColorIcon,
                                           ui::kColorIconDisabled);
  return button;
}

}  // namespace

AuthenticatorGPMArbitraryPinView::AuthenticatorGPMArbitraryPinView(
    Delegate* delegate)
    : delegate_(delegate) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto pin_textfield = std::make_unique<views::Textfield>();
  pin_textfield->SetController(this);
  pin_textfield->SetAccessibleName(u"Pin field (UNTRANSLATED)");
  pin_textfield->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  pin_textfield->SetDefaultWidthInChars(20);
  pin_textfield_ = AddChildView(std::move(pin_textfield));

  std::unique_ptr<views::ToggleImageButton> reveal_button =
      CreateRevealButton(base::BindRepeating(
          &AuthenticatorGPMArbitraryPinView::OnRevealButtonClicked,
          base::Unretained(this)));
  reveal_button->SetTooltipText(u"Tooltip (UNTRANSLATED)");
  reveal_button->SetToggledTooltipText(u"Toggled tooltip (UNTRANSLATED)");
  reveal_button_ = AddChildView(std::move(reveal_button));
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
