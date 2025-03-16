// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_view.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill::payments {

using l10n_util::GetStringFUTF16;
using l10n_util::GetStringUTF16;
using std::u16string;

SelectBnplIssuerDialogControllerImpl::SelectBnplIssuerDialogControllerImpl(
    std::vector<BnplIssuer> issuers,
    base::OnceCallback<void(const std::string&)> selected_issuer_callback,
    base::OnceClosure cancel_callback)
    : issuers_(std::move(issuers)),
      selected_issuer_callback_(std::move(selected_issuer_callback)),
      cancel_callback_(std::move(cancel_callback)) {}

SelectBnplIssuerDialogControllerImpl::~SelectBnplIssuerDialogControllerImpl() {
  // The browser window may be closed while the dialog is shown.
  if (dialog_view_) {
    dialog_view_->Dismiss();
  }
}

void SelectBnplIssuerDialogControllerImpl::ShowDialog(
    base::OnceCallback<std::unique_ptr<SelectBnplIssuerView>()>
        create_and_show_dialog_callback) {
  dialog_view_ = std::move(create_and_show_dialog_callback).Run();
}

void SelectBnplIssuerDialogControllerImpl::OnAccepted(
    const std::string& issuer_id) {
  if (selected_issuer_callback_) {
    std::move(selected_issuer_callback_).Run(issuer_id);
  }
}

void SelectBnplIssuerDialogControllerImpl::OnCancel() {
  std::move(cancel_callback_).Run();
}

void SelectBnplIssuerDialogControllerImpl::OnDialogClosed() {
  dialog_view_.reset();
}

const std::vector<BnplIssuer>&
SelectBnplIssuerDialogControllerImpl::GetIssuers() const {
  return issuers_;
}

bool SelectBnplIssuerDialogControllerImpl::IssuerEligible(
    std::string_view issuer_id) const {
  return true;
}

u16string SelectBnplIssuerDialogControllerImpl::GetTitle() const {
  return GetStringUTF16(IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_TITLE);
}

u16string SelectBnplIssuerDialogControllerImpl::GetSelectionOptionText(
    std::string_view issuer_id) const {
  return issuer_id == kBnplZipIssuerId
             ? GetStringUTF16(
                   IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_ZIP)
             : GetStringUTF16(
                   IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_AFFIRM_AND_AFTERPAY);
}

TextWithLink SelectBnplIssuerDialogControllerImpl::GetLinkText() const {
  TextWithLink text_with_link;
  std::u16string payments_settings_link_text = GetStringUTF16(
      IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_FOOTNOTE_HIDE_OPTION_PAYMENT_SETTINGS_LINK_TEXT);
  size_t offset = 0;
  text_with_link.text = GetStringFUTF16(
      IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_FOOTNOTE_HIDE_OPTION,
      payments_settings_link_text, &offset);

  text_with_link.offset =
      gfx::Range(offset, offset + payments_settings_link_text.length());

  return text_with_link;
}

}  // namespace autofill::payments
