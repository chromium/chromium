// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/ui/views/webauthn/authenticator_paask_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "chrome/grit/generated_resources.h"
#include "device/fido/features.h"
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
  }

  // views::LabelButton:
  void OnThemeChanged() override {
    LabelButton::OnThemeChanged();
    // LabelButton sets its own colours on the label and thus the colour from
    // STYLE_LINK must be set explicitly at the LabelButton level too.
    SetEnabledTextColors(views::style::GetColor(
        *label(), label()->GetTextContext(), views::style::STYLE_LINK));
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

  if (base::FeatureList::IsEnabled(
          device::kWebAuthnNewDiscoverableCredentialsUi)) {
    return std::make_pair(
        std::make_unique<LinkLabelButton>(
            base::BindRepeating(&AuthenticatorPaaskSheetView::OnLinkClicked,
                                base::Unretained(this)),
            l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLEV2_SERVERLINK_TROUBLE)),
        AutoFocus::kNo);
  }

  std::u16string link_text;
  switch (dialog_model->experiment_server_link_sheet_) {
    case AuthenticatorRequestDialogModel::ExperimentServerLinkSheet::CONTROL:
    case AuthenticatorRequestDialogModel::ExperimentServerLinkSheet::ARM_2:
    case AuthenticatorRequestDialogModel::ExperimentServerLinkSheet::ARM_3:
    case AuthenticatorRequestDialogModel::ExperimentServerLinkSheet::ARM_5:
    case AuthenticatorRequestDialogModel::ExperimentServerLinkSheet::ARM_6:
      link_text =
          l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLEV2_SERVERLINK_TROUBLE);
      break;
    case AuthenticatorRequestDialogModel::ExperimentServerLinkSheet::ARM_4:
      link_text = l10n_util::GetStringUTF16(
          IDS_WEBAUTHN_CABLEV2_SERVERLINK_TROUBLE_ALT);
      break;
  }

  return std::make_pair(
      std::make_unique<LinkLabelButton>(
          base::BindRepeating(&AuthenticatorPaaskSheetView::OnLinkClicked,
                              base::Unretained(this)),
          link_text),
      AutoFocus::kNo);
}

void AuthenticatorPaaskSheetView::OnLinkClicked(const ui::Event&) {
  reinterpret_cast<AuthenticatorPaaskSheetModel*>(model())
      ->dialog_model()
      ->ShowCableUsbFallback();
}
