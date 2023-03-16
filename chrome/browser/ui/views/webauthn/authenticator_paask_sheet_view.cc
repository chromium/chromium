// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/webauthn/authenticator_paask_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"

AuthenticatorPaaskSheetView::AuthenticatorPaaskSheetView(
    std::unique_ptr<AuthenticatorPaaskSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {}

AuthenticatorPaaskSheetView::~AuthenticatorPaaskSheetView() = default;

// LinkLabelButton is a LabelButton where the text is styled like a link.
class LinkLabelButton : public views::LabelButton {
 public:
  LinkLabelButton(PressedCallback callback, const std::u16string& text)
      : LabelButton(std::move(callback), text, views::style::CONTEXT_BUTTON) {
    SetBorder(views::CreateEmptyBorder(0));
    label()->SetTextStyle(views::style::STYLE_LINK);
    SetEnabledTextColorIds(views::style::GetColorId(label()->GetTextContext(),
                                                    views::style::STYLE_LINK));
  }
};

std::pair<std::unique_ptr<views::View>,
          AuthenticatorRequestSheetView::AutoFocus>
AuthenticatorPaaskSheetView::BuildStepSpecificContent() {
  AuthenticatorRequestDialogModel* const dialog_model =
      reinterpret_cast<AuthenticatorPaaskSheetModel*>(model())->dialog_model();
  // This context is only shown when USB fallback is an option.
  if (!dialog_model->cable_should_suggest_usb()) {
    return std::make_pair(nullptr, AutoFocus::kNo);
  }

  return std::make_pair(
      std::make_unique<LinkLabelButton>(
          base::BindRepeating(&AuthenticatorPaaskSheetView::OnLinkClicked,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLEV2_SERVERLINK_TROUBLE)),
      AutoFocus::kNo);
}

void AuthenticatorPaaskSheetView::OnLinkClicked(const ui::Event&) {
  reinterpret_cast<AuthenticatorPaaskSheetModel*>(model())
      ->dialog_model()
      ->ShowCableUsbFallback();
}
