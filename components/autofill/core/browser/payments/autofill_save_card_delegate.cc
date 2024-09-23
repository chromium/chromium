// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_save_card_delegate.h"

#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill {

AutofillSaveCardDelegate::AutofillSaveCardDelegate(
    absl::variant<
        payments::PaymentsAutofillClient::LocalSaveCardPromptCallback,
        payments::PaymentsAutofillClient::UploadSaveCardPromptCallback>
        save_card_callback,
    payments::PaymentsAutofillClient::SaveCreditCardOptions options)
    : options_(options),
      had_user_interaction_(false),
      save_card_callback_(std::move(save_card_callback)) {}

AutofillSaveCardDelegate::~AutofillSaveCardDelegate() = default;

void AutofillSaveCardDelegate::OnUiShown() {
  LogInfoBarAction(AutofillMetrics::INFOBAR_SHOWN);
}

void AutofillSaveCardDelegate::OnUiAccepted(base::OnceClosure callback) {
  on_finished_gathering_consent_callback_ = std::move(callback);
  // Credit card save acceptance can be logged immediately if:
  // 1. the user is accepting card local save.
  // 2. or when we don't need more info in order to upload.
  if (options_.card_save_type !=
          payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly &&
      (!is_for_upload() || !requires_fix_flow())) {
    LogSaveCreditCardPromptResult(
        autofill_metrics::SaveCreditCardPromptResult::kAccepted,
        is_for_upload(), options_);
  }
  LogInfoBarAction(AutofillMetrics::INFOBAR_ACCEPTED);
  GatherAdditionalConsentIfApplicable(/*user_provided_details=*/{});
}

void AutofillSaveCardDelegate::OnUiUpdatedAndAccepted(
    payments::PaymentsAutofillClient::UserProvidedCardDetails
        user_provided_details) {
  LogInfoBarAction(AutofillMetrics::INFOBAR_ACCEPTED);
  GatherAdditionalConsentIfApplicable(user_provided_details);
}

void AutofillSaveCardDelegate::OnUiCanceled() {
  RunSaveCardPromptCallback(
      payments::PaymentsAutofillClient::SaveCardOfferUserDecision::kDeclined,
      /*user_provided_details=*/{});
  LogInfoBarAction(AutofillMetrics::INFOBAR_DENIED);
  if (options_.card_save_type !=
      payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly) {
    LogSaveCreditCardPromptResult(
        autofill_metrics::SaveCreditCardPromptResult::kDenied, is_for_upload(),
        options_);
  }
}

void AutofillSaveCardDelegate::OnUiIgnored() {
  if (!had_user_interaction_) {
    RunSaveCardPromptCallback(
        payments::PaymentsAutofillClient::SaveCardOfferUserDecision::kIgnored,
        /*user_provided_details=*/{});
    LogInfoBarAction(AutofillMetrics::INFOBAR_IGNORED);
    if (options_.card_save_type !=
        payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly) {
      LogSaveCreditCardPromptResult(
          autofill_metrics::SaveCreditCardPromptResult::kIgnored,
          is_for_upload(), options_);
    }
  }
}

void AutofillSaveCardDelegate::OnFinishedGatheringConsent(
    payments::PaymentsAutofillClient::SaveCardOfferUserDecision user_decision,
    payments::PaymentsAutofillClient::UserProvidedCardDetails
        user_provided_details) {
  RunSaveCardPromptCallback(user_decision, user_provided_details);
  if (!on_finished_gathering_consent_callback_.is_null()) {
    std::move(on_finished_gathering_consent_callback_).Run();
  }
}

void AutofillSaveCardDelegate::RunSaveCardPromptCallback(
    payments::PaymentsAutofillClient::SaveCardOfferUserDecision user_decision,
    payments::PaymentsAutofillClient::UserProvidedCardDetails
        user_provided_details) {
  if (is_for_upload()) {
    absl::get<payments::PaymentsAutofillClient::UploadSaveCardPromptCallback>(
        std::move(save_card_callback_))
        .Run(user_decision, user_provided_details);
  } else {
    absl::get<payments::PaymentsAutofillClient::LocalSaveCardPromptCallback>(
        std::move(save_card_callback_))
        .Run(user_decision);
  }
}

void AutofillSaveCardDelegate::GatherAdditionalConsentIfApplicable(
    payments::PaymentsAutofillClient::UserProvidedCardDetails
        user_provided_details) {
  OnFinishedGatheringConsent(
      payments::PaymentsAutofillClient::SaveCardOfferUserDecision::kAccepted,
      user_provided_details);
}

void AutofillSaveCardDelegate::LogInfoBarAction(
    AutofillMetrics::InfoBarMetric action) {
  CHECK(!had_user_interaction_);
  if (options_.card_save_type ==
      payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly) {
    autofill_metrics::LogCvcInfoBarMetric(action, is_for_upload());
  } else {
    AutofillMetrics::LogCreditCardInfoBarMetric(action, is_for_upload(),
                                                options_);
  }
  if (action != AutofillMetrics::INFOBAR_SHOWN) {
    had_user_interaction_ = true;
  }
}

}  // namespace autofill
