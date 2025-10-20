// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/android_bnpl_ui_delegate.h"

#include <vector>

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/autofill_progress_dialog_type.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/bnpl_tos_controller.h"

namespace autofill::payments {

AndroidBnplUiDelegate::AndroidBnplUiDelegate(PaymentsAutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

AndroidBnplUiDelegate::~AndroidBnplUiDelegate() = default;

void AndroidBnplUiDelegate::ShowSelectBnplIssuerUi(
    std::vector<BnplIssuerContext> bnpl_issuer_context,
    std::string app_locale,
    base::OnceCallback<void(autofill::BnplIssuer)> selected_issuer_callback,
    base::OnceClosure cancel_callback) {
  client_->ShowTouchToFillBnplIssuers(bnpl_issuer_context, app_locale,
                                      std::move(selected_issuer_callback),
                                      std::move(cancel_callback));
}

void AndroidBnplUiDelegate::DismissSelectBnplIssuerUi() {
  // TODO(crbug.com/438783909): Add JNI call to dismiss the TouchToFill bottom
  // sheet with the BNPL issuer selection screen.
}

void AndroidBnplUiDelegate::ShowBnplTosUi(BnplTosModel bnpl_tos_model,
                                          base::OnceClosure accept_callback,
                                          base::OnceClosure cancel_callback) {
  // TODO(crbug.com/438783909): Add JNI call to show the TouchToFill bottom
  // sheet with the ToS screen.
}

void AndroidBnplUiDelegate::CloseBnplTosUi() {
  // TODO(crbug.com/438783909): Add JNI call to close the ToS screen.
}

void AndroidBnplUiDelegate::ShowProgressUi(
    AutofillProgressDialogType autofill_progress_dialog_type,
    base::OnceClosure cancel_callback) {
  client_->ShowTouchToFillProgress(std::move(cancel_callback));
}

void AndroidBnplUiDelegate::CloseProgressUi(
    bool show_confirmation_before_closing) {
  // TODO(crbug.com/438783909): Add JNI call to hide the progress spinner.
}

void AndroidBnplUiDelegate::ShowAutofillErrorUi(
    AutofillErrorDialogContext context) {
  client_->ShowTouchToFillError(context);
}

}  // namespace autofill::payments
