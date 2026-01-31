// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_controller.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/payments/content/content_payment_request_delegate.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/content/secure_payment_confirmation_app.h"
#include "components/payments/content/secure_payment_confirmation_transaction_mode.h"
#include "components/payments/core/currency_formatter.h"
#include "components/payments/core/features.h"
#include "components/payments/core/method_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/url_constants.h"

namespace {

constexpr char kTransactionOutcomeHistogramName[] =
    "SecurePaymentRequest.Transaction.Outcome";

constexpr char kFallbackOutcomeHistogramName[] =
    "SecurePaymentRequest.Fallback.Outcome";

}  // namespace

namespace payments {

SecurePaymentConfirmationController::SecurePaymentConfirmationController(
    base::WeakPtr<PaymentRequest> request)
    : request_(request) {
  DCHECK(request_);
}

SecurePaymentConfirmationController::~SecurePaymentConfirmationController() =
    default;

void SecurePaymentConfirmationController::ShowDialog() {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED();
#else
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
#endif  // BUILDFLAG(IS_ANDROID)
}

void SecurePaymentConfirmationController::RetryDialog() {
  // Retry is not supported.
  OnCancel();
}

void SecurePaymentConfirmationController::CloseDialog() {
  if (view_) {
    view_->HideDialog();
  }
}

void SecurePaymentConfirmationController::ShowErrorMessage() {
  // Error message is not supported.
  OnCancel();
}

void SecurePaymentConfirmationController::ShowProcessingSpinner() {
  if (!view_) {
    return;
  }

  model_.set_progress_bar_visible(true);
  model_.set_verify_button_enabled(false);
  model_.set_cancel_button_enabled(false);
  view_->OnModelUpdated();
}

bool SecurePaymentConfirmationController::IsInteractive() const {
  return view_ && !model_.progress_bar_visible();
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

bool SecurePaymentConfirmationController::ClickOptOutForTesting() {
  // This should only be called when the view is showing.
  DCHECK(view_);
  return view_->ClickOptOutForTesting();
}

void SecurePaymentConfirmationController::OnInitialized(
    InitializationTask* initialization_task) {
  if (--number_of_initialization_tasks_ == 0) {
    SetupModelAndShowDialogIfApplicable();
  }
}

void SecurePaymentConfirmationController::OnConfirm() {
  if (is_error_dialog_) {
    base::UmaHistogramEnumeration(kFallbackOutcomeHistogramName,
                                  SecurePaymentRequestOutcome::kAccept);

    is_dialog_showing_ = false;
    CloseDialog();

    if (!request_) {
      return;
    }

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&PaymentRequest::OnUserAuthAnotherWay, request_));
  } else {
    base::UmaHistogramEnumeration(kTransactionOutcomeHistogramName,
                                  SecurePaymentRequestOutcome::kAccept);

    if (!request_) {
      return;
    }

    ShowProcessingSpinner();

    // This will trigger WebAuthn with OS-level UI (if any) on top of the
    // |view_| with its animated processing spinner. For example, on Linux,
    // there's no OS-level UI, while on MacOS, there's an OS-level prompt for
    // the Touch ID that shows on top of Chrome.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&PaymentRequest::Pay, request_));
  }
}

void SecurePaymentConfirmationController::OnAnotherWay() {
  base::UmaHistogramEnumeration(kTransactionOutcomeHistogramName,
                                SecurePaymentRequestOutcome::kAnotherWay);

  is_dialog_showing_ = false;
  CloseDialog();

  if (!request_) {
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PaymentRequest::OnUserAuthAnotherWay, request_));
}

void SecurePaymentConfirmationController::OnCancel() {
  // Dialog was already closed.
  if (!is_dialog_showing_) {
    return;
  }

  if (is_error_dialog_) {
    base::UmaHistogramEnumeration(kFallbackOutcomeHistogramName,
                                  SecurePaymentRequestOutcome::kCancel);
  } else {
    if (base::FeatureList::IsEnabled(
            blink::features::kSecurePaymentConfirmationUxRefresh)) {
      base::UmaHistogramEnumeration(kTransactionOutcomeHistogramName,
                                    SecurePaymentRequestOutcome::kCancel);
    } else {
      base::UmaHistogramEnumeration(kTransactionOutcomeHistogramName,
                                    SecurePaymentRequestOutcome::kAnotherWay);
    }
  }

  is_dialog_showing_ = false;
  CloseDialog();

  if (!request_) {
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PaymentRequest::OnUserCancelled, request_));
}

void SecurePaymentConfirmationController::OnOptOut() {
  if (is_error_dialog_) {
    base::UmaHistogramEnumeration(kFallbackOutcomeHistogramName,
                                  SecurePaymentRequestOutcome::kOptOut);
  } else {
    base::UmaHistogramEnumeration(kTransactionOutcomeHistogramName,
                                  SecurePaymentRequestOutcome::kOptOut);
  }

  is_dialog_showing_ = false;
  CloseDialog();

  if (!request_) {
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PaymentRequest::OnUserOptedOut, request_));
}

base::WeakPtr<SecurePaymentConfirmationController>
SecurePaymentConfirmationController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
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

  is_dialog_showing_ = true;

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
      request_->spec()->request_payer_phone() ||
      request_->spec()->method_data().size() != 1 ||
      !request_->spec()->method_data().front() ||
      request_->spec()->method_data().front()->supported_method !=
          methods::kSecurePaymentConfirmation ||
      !request_->spec()->method_data().front()->secure_payment_confirmation ||
      (request_->spec()
           ->method_data()
           .front()
           ->secure_payment_confirmation->payee_origin.has_value() &&
       request_->spec()
               ->method_data()
               .front()
               ->secure_payment_confirmation->payee_origin->scheme() !=
           url::kHttpsScheme)) {
    OnCancel();
    return;
  }

  SecurePaymentConfirmationApp* app =
      static_cast<SecurePaymentConfirmationApp*>(
          request_->state()->selected_app());
  if (base::FeatureList::IsEnabled(
          blink::features::kSecurePaymentConfirmationUxRefresh)) {
    is_error_dialog_ = app->IsErrorDialog();

    model_.set_header_logos(app->GetPaymentEntitiesLogos());

    std::optional<std::string>& payee_name =
        request_->spec()
            ->method_data()
            .front()
            ->secure_payment_confirmation->payee_name;
    if (payee_name.has_value()) {
      model_.set_merchant_name(
          std::optional<std::u16string>(base::UTF8ToUTF16(payee_name.value())));
    }
    std::optional<url::Origin>& origin =
        request_->spec()
            ->method_data()
            .front()
            ->secure_payment_confirmation->payee_origin;
    if (origin.has_value()) {
      model_.set_merchant_origin(std::optional<std::u16string>(
          url_formatter::FormatUrlForSecurityDisplay(
              origin.value().GetURL(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC)));
    }

    model_.set_instrument_value(app->GetLabel());
    model_.set_instrument_details_value(app->GetSublabel());
    model_.set_instrument_icon(app->icon_bitmap());

    const mojom::PaymentItemPtr& total = request_->spec()->GetTotal(app);
    std::u16string total_value = base::UTF8ToUTF16(total->amount->currency);
    model_.set_total_value(base::StrCat(
        {base::UTF8ToUTF16(total->amount->currency), u" ",
         CurrencyFormatter(total->amount->currency,
                           request_->state()->GetApplicationLocale())
             .Format(total->amount->value)}));

    model_.set_progress_bar_visible(false);

    model_.set_opt_out_visible(request_->spec()
                                   ->method_data()
                                   .front()
                                   ->secure_payment_confirmation->show_opt_out);
    model_.set_opt_out_label(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_OPT_OUT_TEXT));
    model_.set_opt_out_authenticator_label(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_OPT_OUT_AUTHENTICATOR_TEXT));
    model_.set_opt_out_link_label(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_OPT_OUT_LINK_TEXT));
    model_.set_relying_party_id(
        base::UTF8ToUTF16(request_->spec()
                              ->method_data()
                              .front()
                              ->secure_payment_confirmation->rp_id));

    model_.set_cancel_button_label(l10n_util::GetStringUTF16(IDS_CANCEL));

    model_.set_footer_label(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_FOOTNOTE_TEXT));
    model_.set_footer_link_label(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_FOOTNOTE_LINK_TEXT));

    if (is_error_dialog_) {
      model_.set_title(l10n_util::GetStringUTF16(
          IDS_SECURE_PAYMENT_CONFIRMATION_INFORM_ONLY_TITLE));
      model_.set_footer_visible(false);
      model_.set_verify_button_label(l10n_util::GetStringUTF16(
          IDS_SECURE_PAYMENT_CONFIRMATION_CONFIRM_BUTTON_LABEL));
    } else {
      model_.set_title(
          l10n_util::GetStringUTF16(IDS_SECURE_PAYMENT_CONFIRMATION_TITLE));
      model_.set_footer_visible(true);
      model_.set_verify_button_label(l10n_util::GetStringUTF16(
          IDS_SECURE_PAYMENT_CONFIRMATION_VERIFY_BUTTON_LABEL));
    }

    view_ = SecurePaymentConfirmationView::Create(
        request_->state()->GetPaymentRequestDelegate()->GetPaymentUIObserver());
  } else {
    model_.set_verify_button_label(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_VERIFY_BUTTON_LABEL));
    model_.set_cancel_button_label(l10n_util::GetStringUTF16(IDS_CANCEL));
    model_.set_progress_bar_visible(false);

    model_.set_title(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_VERIFY_PURCHASE));

    model_.set_merchant_label(
        l10n_util::GetStringUTF16(IDS_SECURE_PAYMENT_CONFIRMATION_STORE_LABEL));
    std::optional<std::string>& payee_name =
        request_->spec()
            ->method_data()
            .front()
            ->secure_payment_confirmation->payee_name;
    if (payee_name.has_value()) {
      model_.set_merchant_name(
          std::optional<std::u16string>(base::UTF8ToUTF16(payee_name.value())));
    }
    std::optional<url::Origin>& origin =
        request_->spec()
            ->method_data()
            .front()
            ->secure_payment_confirmation->payee_origin;
    if (origin.has_value()) {
      model_.set_merchant_origin(std::optional<std::u16string>(
          url_formatter::FormatUrlForSecurityDisplay(
              origin.value().GetURL(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC)));
    }

    model_.set_instrument_label(l10n_util::GetStringUTF16(
        IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME));
    model_.set_instrument_value(app->GetLabel());
    model_.set_instrument_icon(app->icon_bitmap());

    model_.set_total_label(
        l10n_util::GetStringUTF16(IDS_SECURE_PAYMENT_CONFIRMATION_TOTAL_LABEL));
    const mojom::PaymentItemPtr& total = request_->spec()->GetTotal(app);
    std::u16string total_value = base::UTF8ToUTF16(total->amount->currency);
    model_.set_total_value(base::StrCat(
        {base::UTF8ToUTF16(total->amount->currency), u" ",
         CurrencyFormatter(total->amount->currency,
                           request_->state()->GetApplicationLocale())
             .Format(total->amount->value)}));

    model_.set_opt_out_visible(request_->spec()
                                   ->method_data()
                                   .front()
                                   ->secure_payment_confirmation->show_opt_out);
    model_.set_opt_out_label(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_OPT_OUT_LABEL));
    model_.set_opt_out_link_label(l10n_util::GetStringUTF16(
        IDS_SECURE_PAYMENT_CONFIRMATION_OPT_OUT_LINK_LABEL));

    model_.set_relying_party_id(
        base::UTF8ToUTF16(request_->spec()
                              ->method_data()
                              .front()
                              ->secure_payment_confirmation->rp_id));

    view_ = SecurePaymentConfirmationView::Create(
        request_->state()->GetPaymentRequestDelegate()->GetPaymentUIObserver());
  }

  view_->ShowDialog(
      request_->web_contents(), model_.GetWeakPtr(),
      base::BindOnce(&SecurePaymentConfirmationController::OnConfirm,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&SecurePaymentConfirmationController::OnAnotherWay,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&SecurePaymentConfirmationController::OnCancel,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&SecurePaymentConfirmationController::OnOptOut,
                     weak_ptr_factory_.GetWeakPtr()));

  // For automated testing, SPC can be placed in an 'autoaccept' or
  // 'autoreject' mode, where the dialog should immediately be
  // accepted/rejected without user interaction. We deliberately wait until
  // after the dialog is created and shown to handle this, in order to keep the
  // automation codepath as close to the 'real' one as possible.
  switch (request_->spc_transaction_mode()) {
    case SPCTransactionMode::kAutoAccept:
      OnConfirm();
      break;
    case SPCTransactionMode::kAutoAuthAnotherWay:
      if (base::FeatureList::IsEnabled(
              blink::features::kSecurePaymentConfirmationUxRefresh)) {
        OnAnotherWay();
      } else {
        OnCancel();
      }
      break;
    case SPCTransactionMode::kAutoReject:
      OnCancel();
      break;
    case SPCTransactionMode::kAutoOptOut:
      OnOptOut();
      break;
    case SPCTransactionMode::kNone:
      break;
  }
}

}  // namespace payments
