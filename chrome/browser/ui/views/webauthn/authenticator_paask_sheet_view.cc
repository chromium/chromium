// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/webauthn/authenticator_paask_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/styled_label.h"

AuthenticatorPaaskSheetView::AuthenticatorPaaskSheetView(
    std::unique_ptr<AuthenticatorPaaskSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {}

AuthenticatorPaaskSheetView::~AuthenticatorPaaskSheetView() = default;

std::unique_ptr<views::View>
AuthenticatorPaaskSheetView::BuildStepSpecificContent() {
  AuthenticatorRequestDialogModel* const dialog_model =
      reinterpret_cast<AuthenticatorPaaskSheetModel*>(model())->dialog_model();
  // This context is only shown when USB fallback is an option.
  if (!dialog_model->cable_should_suggest_usb()) {
    return nullptr;
  }

  // link_message contains the translation of the text of the link.
  const base::string16 link_message =
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLEV2_SERVERLINK_TROUBLE);

  // offsets will contain the index of the start of the substituted strings. The
  // second of these will be the position of the link text, which is used to
  // decorate it.
  std::vector<size_t> offsets;
  const base::string16 description = l10n_util::GetStringFUTF16(
      IDS_WEBAUTHN_CABLEV2_SERVERLINK_DESCRIPTION,
      {AuthenticatorPaaskSheetModel::GetRelyingPartyIdString(dialog_model),
       link_message},
      &offsets);
  DCHECK_EQ(offsets.size(), 2u);

  auto label = std::make_unique<views::StyledLabel>();
  label->SetText(description);
  label->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  label->SetDefaultTextStyle(views::style::STYLE_PRIMARY);
  auto link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &AuthenticatorPaaskSheetView::OnLinkClicked, base::Unretained(this)));
  label->AddStyleRange(gfx::Range(offsets[1], offsets[1] + link_message.size()),
                       link_style);

  return label;
}

void AuthenticatorPaaskSheetView::OnLinkClicked() {
  reinterpret_cast<AuthenticatorPaaskSheetModel*>(model())
      ->dialog_model()
      ->ShowCableUsbFallback();
}
