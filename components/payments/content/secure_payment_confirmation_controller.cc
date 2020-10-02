// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_controller.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/core/currency_formatter.h"
#include "components/payments/core/method_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"

namespace payments {

SecurePaymentConfirmationController::SecurePaymentConfirmationController(
    base::WeakPtr<PaymentRequest> request)
    : request_(request) {
  DCHECK(request_);
}

SecurePaymentConfirmationController::~SecurePaymentConfirmationController() =
    default;

void SecurePaymentConfirmationController::ShowDialog() {
#if defined(OS_ANDROID)
  NOTREACHED();
#endif  // OS_ANDROID

  if (!request_ || !request_->spec())
    return;

  if (!request_->state()->IsInitialized()) {
    number_of_initialization_tasks_++;
    request_->state()->AddInitializationObserver(this);
  }

  if (!request_->spec()->IsInitialized()) {
    number_of_initialization_tasks_++;
    request_->spec()->AddInitializationObserver(this);
  }

  if (number_of_initialization_tasks_ == 0)
    SetupModelAndShowDialogIfApplicable();
}

void SecurePaymentConfirmationController::
    SetupModelAndShowDialogIfApplicable() {
  DCHECK(!view_);
  // If no apps are available then don't show any UI. The payment_request.cc
  // code will reject the PaymentRequest.show() call with appropriate error
  // message on its own.
  if (!request_ || !request_->state() || !request_->spec() ||
      request_->state()->available_apps().empty()) {
    return;
  }

  if (!request_->web_contents() || !request_->state()->selected_app() ||
      request_->state()->selected_app()->type() != PaymentApp::Type::INTERNAL ||
      request_->state()->selected_app()->GetAppMethodNames().size() != 1 ||
      *request_->state()->selected_app()->GetAppMethodNames().begin() !=
          methods::kSecurePaymentConfirmation ||
      request_->state()->available_apps().size() != 1 || !request_->spec() ||
      !request_->spec()->IsSecurePaymentConfirmationRequested() ||
      request_->spec()->request_shipping() ||
      request_->spec()->request_payer_name() ||
      request_->spec()->request_payer_email() ||
      request_->spec()->request_payer_phone()) {
    OnCancel();
    return;
  }

  model_.set_verify_button_label(l10n_util::GetStringUTF16(
      IDS_SECURE_PAYMENT_CONFIRMATION_VERIFY_BUTTON_LABEL));
  model_.set_cancel_button_label(l10n_util::GetStringUTF16(IDS_CANCEL));
  model_.set_progress_bar_visible(false);

  model_.set_title(l10n_util::GetStringUTF16(
      IDS_SECURE_PAYMENT_CONFIRMATION_VERIFY_PURCHASE));

  model_.set_merchant_label(
      l10n_util::GetStringUTF16(IDS_SECURE_PAYMENT_CONFIRMATION_STORE_LABEL));
  model_.set_merchant_value(url_formatter::FormatUrlForSecurityDisplay(
      request_->state()->GetTopOrigin(),
      url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));

  model_.set_instrument_label(l10n_util::GetStringUTF16(
      IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME));
  PaymentApp* app = request_->state()->selected_app();
  model_.set_instrument_value(app->GetLabel());
  model_.set_instrument_icon(app->icon_bitmap());

  model_.set_total_label(
      l10n_util::GetStringUTF16(IDS_SECURE_PAYMENT_CONFIRMATION_TOTAL_LABEL));
  const mojom::PaymentItemPtr& total = request_->spec()->GetTotal(app);
  base::string16 total_value = base::UTF8ToUTF16(total->amount->currency);
  model_.set_total_value(base::StrCat(
      {base::UTF8ToUTF16(total->amount->currency), base::ASCIIToUTF16(" "),
       CurrencyFormatter(total->amount->currency,
                         request_->state()->GetApplicationLocale())
           .Format(total->amount->value)}));

  view_ = SecurePaymentConfirmationView::Create();
  view_->ShowDialog(
      request_->web_contents(), model_.GetWeakPtr(),
      base::BindOnce(&SecurePaymentConfirmationController::OnConfirm,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&SecurePaymentConfirmationController::OnCancel,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SecurePaymentConfirmationController::RetryDialog() {
  // Retry is not supported.
  OnCancel();
}

void SecurePaymentConfirmationController::CloseDialog() {
  if (view_)
    view_->HideDialog();
}

void SecurePaymentConfirmationController::ShowErrorMessage() {
  // Error message is not supported.
  OnCancel();
}

void SecurePaymentConfirmationController::ShowProcessingSpinner() {
  if (!view_)
    return;

  model_.set_progress_bar_visible(true);
  model_.set_verify_button_enabled(false);
  model_.set_cancel_button_enabled(false);
  view_->OnModelUpdated();
}

bool SecurePaymentConfirmationController::IsInteractive() const {
  return view_ && !model_.progress_bar_visible();
}

void SecurePaymentConfirmationController::ShowCvcUnmaskPrompt(
    const autofill::CreditCard& credit_card,
    base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
        result_delegate,
    content::WebContents* web_contents) {
  // CVC unmasking is nut supported.
  NOTREACHED();
}

void SecurePaymentConfirmationController::ShowPaymentHandlerScreen(
    const GURL& url,
    PaymentHandlerOpenWindowCallback callback) {
  // Payment handler screen is not supported.
  NOTREACHED();
}

void SecurePaymentConfirmationController::ConfirmPaymentForTesting() {
  OnConfirm();
}

void SecurePaymentConfirmationController::OnInitialized(
    InitializationTask* initialization_task) {
  if (--number_of_initialization_tasks_ == 0)
    SetupModelAndShowDialogIfApplicable();
}

void SecurePaymentConfirmationController::OnDismiss() {
  OnCancel();
}

void SecurePaymentConfirmationController::OnCancel() {
  CloseDialog();

  if (!request_)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PaymentRequest::UserCancelled, request_));
}

void SecurePaymentConfirmationController::OnConfirm() {
  if (!request_)
    return;

  ShowProcessingSpinner();

  // This will trigger WebAuthn with OS-level UI (if any) on top of the |view_|
  // with its animated processing spinner. For example, on Linux, there's no
  // OS-level UI, while on MacOS, there's an OS-level prompt for the Touch ID
  // that shows on top of Chrome.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PaymentRequest::Pay, request_));
}

base::WeakPtr<SecurePaymentConfirmationController>
SecurePaymentConfirmationController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace payments
