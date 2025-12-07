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
#include "components/grit/components_scaled_resources.h"

namespace autofill::payments {

AndroidBnplUiDelegate::AndroidBnplUiDelegate(PaymentsAutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

AndroidBnplUiDelegate::~AndroidBnplUiDelegate() = default;

void AndroidBnplUiDelegate::ShowSelectBnplIssuerUi(
    std::vector<BnplIssuerContext> bnpl_issuer_context,
    std::string app_locale,
    base::OnceCallback<void(autofill::BnplIssuer)> selected_issuer_callback,
    base::OnceClosure cancel_callback,
    bool has_seen_ai_terms) {
  client_->ShowTouchToFillBnplIssuers(bnpl_issuer_context, app_locale,
                                      std::move(selected_issuer_callback),
                                      std::move(cancel_callback));
}

void AndroidBnplUiDelegate::UpdateBnplIssuerDialogUi(
    std::vector<BnplIssuerContext> issuer_contexts) {
  // TODO(crbug.com/438783909): Add JNI call to update the TouchToFill bottom
  // sheet once the new list of BNPL issuers comes back.
}

void AndroidBnplUiDelegate::RemoveSelectBnplIssuerOrProgressUi() {
  client_->SetTouchToFillVisible(/*visible=*/false);
}

void AndroidBnplUiDelegate::ShowBnplTosUi(BnplTosModel bnpl_tos_model,
                                          base::OnceClosure accept_callback,
                                          base::OnceClosure cancel_callback) {
  client_->ShowTouchToFillBnplTos(std::move(bnpl_tos_model),
                                  std::move(accept_callback),
                                  std::move(cancel_callback));
}

void AndroidBnplUiDelegate::RemoveBnplTosOrProgressUi() {
  // If the user accepted BNPL Terms of Service, then progress touch to fill
  // screen must be showing, so close it.
  client_->SetTouchToFillVisible(/*visible=*/false);
}

void AndroidBnplUiDelegate::ShowProgressUi(
    AutofillProgressDialogType autofill_progress_dialog_type,
    base::OnceClosure cancel_callback) {
  client_->ShowTouchToFillProgress(std::move(cancel_callback));
}

void AndroidBnplUiDelegate::CloseProgressUi(
    bool credit_card_fetched_successfully) {
  client_->HideTouchToFillPaymentMethod();
}

void AndroidBnplUiDelegate::ShowAutofillErrorUi(
    AutofillErrorDialogContext context) {
  client_->ShowTouchToFillError(context);
}

// Static.
int AndroidBnplUiDelegate::GetDuoBrandedIconForBnplIssuer(
    BnplIssuer::IssuerId issuer_id,
    bool is_dark_mode) {
  switch (issuer_id) {
    case BnplIssuer::IssuerId::kBnplAffirm:
      return is_dark_mode ? IDR_AUTOFILL_GOOGLE_PAY_AFFIRM_DARK
                          : IDR_AUTOFILL_GOOGLE_PAY_AFFIRM;
    case BnplIssuer::IssuerId::kBnplAfterpay:
      return is_dark_mode ? IDR_AUTOFILL_GOOGLE_PAY_AFTERPAY_DARK
                          : IDR_AUTOFILL_GOOGLE_PAY_AFTERPAY;
    case BnplIssuer::IssuerId::kBnplKlarna:
      return is_dark_mode ? IDR_AUTOFILL_GOOGLE_PAY_KLARNA_DARK
                          : IDR_AUTOFILL_GOOGLE_PAY_KLARNA;
    case BnplIssuer::IssuerId::kBnplZip:
      return is_dark_mode ? IDR_AUTOFILL_GOOGLE_PAY_ZIP_DARK
                          : IDR_AUTOFILL_GOOGLE_PAY_ZIP;
  }
  NOTREACHED();
}

}  // namespace autofill::payments
