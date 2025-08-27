// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/desktop_bnpl_ui_delegate.h"

#include "base/check_deref.h"
#include "chrome/browser/ui/autofill/payments/payments_view_factory.h"
#include "components/autofill/content/browser/content_autofill_client.h"
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
    base::OnceClosure cancel_callback) {
  select_bnpl_issuer_dialog_controller_ =
      std::make_unique<SelectBnplIssuerDialogControllerImpl>();
  select_bnpl_issuer_dialog_controller_->ShowDialog(
      base::BindOnce(&CreateAndShowBnplIssuerSelectionDialog,
                     select_bnpl_issuer_dialog_controller_->GetWeakPtr(),
                     base::Unretained(&client_->GetWebContents())),
      std::move(bnpl_issuer_context), std::move(app_locale),
      std::move(selected_issuer_callback), std::move(cancel_callback));
}

void DesktopBnplUiDelegate::DismissSelectBnplIssuerUi() {
  if (select_bnpl_issuer_dialog_controller_) {
    select_bnpl_issuer_dialog_controller_->Dismiss();
    select_bnpl_issuer_dialog_controller_.reset();
  }
}

}  // namespace autofill::payments
