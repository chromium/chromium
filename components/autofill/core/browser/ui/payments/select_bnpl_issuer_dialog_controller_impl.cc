// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_dialog_controller_impl.h"

#include "components/autofill/core/browser/ui/payments/select_bnpl_issuer_view.h"

namespace autofill::payments {

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

}  // namespace autofill::payments
