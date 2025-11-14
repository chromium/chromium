// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/desktop_bnpl_ui_delegate.h"

#include "base/check_deref.h"
#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/payments_view_factory.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/autofill_progress_dialog_type.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_view.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_view.h"

namespace autofill::payments {

DesktopBnplUiDelegate::DesktopBnplUiDelegate(ContentAutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

DesktopBnplUiDelegate::~DesktopBnplUiDelegate() = default;

void DesktopBnplUiDelegate::ShowSelectBnplIssuerUi(
    std::vector<BnplIssuerContext> bnpl_issuer_context,
    std::string app_locale,
    base::OnceCallback<void(BnplIssuer)> selected_issuer_callback,
    base::OnceClosure cancel_callback,
    bool has_seen_ai_terms) {
  select_bnpl_issuer_dialog_controller_ =
      std::make_unique<SelectBnplIssuerDialogControllerImpl>(
          client_->GetPaymentsAutofillClient());
  select_bnpl_issuer_dialog_controller_->ShowDialog(
      base::BindOnce(&CreateAndShowBnplIssuerSelectionDialog,
                     select_bnpl_issuer_dialog_controller_->GetWeakPtr(),
                     base::Unretained(&client_->GetWebContents()),
                     has_seen_ai_terms),
      std::move(bnpl_issuer_context), std::move(app_locale),
      std::move(selected_issuer_callback), std::move(cancel_callback));
}

void DesktopBnplUiDelegate::UpdateBnplIssuerDialogUi(
    std::vector<BnplIssuerContext> issuer_contexts) {
  if (select_bnpl_issuer_dialog_controller_) {
    select_bnpl_issuer_dialog_controller_->UpdateDialogWithIssuers(
        std::move(issuer_contexts));
  }
}

void DesktopBnplUiDelegate::RemoveSelectBnplIssuerOrProgressUi() {
  if (select_bnpl_issuer_dialog_controller_) {
    select_bnpl_issuer_dialog_controller_->Dismiss();
    select_bnpl_issuer_dialog_controller_.reset();
  }
}

void DesktopBnplUiDelegate::ShowBnplTosUi(BnplTosModel bnpl_tos_model,
                                          base::OnceClosure accept_callback,
                                          base::OnceClosure cancel_callback) {
  if (!bnpl_tos_controller_) {
    bnpl_tos_controller_ =
        std::make_unique<BnplTosControllerImpl>(&client_.get());
  }

  bnpl_tos_controller_->Show(
      base::BindOnce(&CreateAndShowBnplTos, bnpl_tos_controller_->GetWeakPtr(),
                     base::Unretained(&client_->GetWebContents())),
      std::move(bnpl_tos_model), std::move(accept_callback),
      std::move(cancel_callback));
}

void DesktopBnplUiDelegate::RemoveBnplTosOrProgressUi() {
  if (!bnpl_tos_controller_) {
    return;
  }

  // If the BNPL issuer selected is not linked, or is linked but requires ToS
  // acceptance, then the ToS UI must be showing, so close it.
  bnpl_tos_controller_->Dismiss();
  bnpl_tos_controller_.reset();
}

void DesktopBnplUiDelegate::ShowProgressUi(
    AutofillProgressDialogType autofill_progress_dialog_type,
    base::OnceClosure cancel_callback) {
  client_->GetPaymentsAutofillClient()->ShowAutofillProgressDialog(
      autofill_progress_dialog_type, std::move(cancel_callback));
}

void DesktopBnplUiDelegate::CloseProgressUi(
    bool credit_card_fetched_successfully) {
  client_->GetPaymentsAutofillClient()->CloseAutofillProgressDialog(
      /*show_confirmation_before_closing=*/credit_card_fetched_successfully,
      /*no_interactive_authentication_callback=*/base::DoNothing());
}

void DesktopBnplUiDelegate::ShowAutofillErrorUi(
    AutofillErrorDialogContext context) {
  client_->GetPaymentsAutofillClient()->ShowAutofillErrorDialog(
      std::move(context));
}

}  // namespace autofill::payments
