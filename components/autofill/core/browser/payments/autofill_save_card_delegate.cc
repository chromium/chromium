// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_save_card_delegate.h"

#include <variant>

#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill {

using PaymentsAutofillClient = payments::PaymentsAutofillClient;

AutofillSaveCardDelegate::AutofillSaveCardDelegate(
    std::variant<PaymentsAutofillClient::LocalSaveCardPromptCallback,
                 PaymentsAutofillClient::UploadSaveCardPromptCallback,
                 PaymentsAutofillClient::CardSaveAndFillDialogCallback>
        save_card_callback,
    PaymentsAutofillClient::SaveCreditCardOptions options)
    : options_(std::move(options)),
      had_user_interaction_(false),
      save_card_callback_(std::move(save_card_callback)) {}

AutofillSaveCardDelegate::~AutofillSaveCardDelegate() = default;

void AutofillSaveCardDelegate::OnUiShown() {
  // TODO(crbug.com/394337666): Remove logging `AutofillMetrics::InfoBarMetric`
  // for iOS.
  LogInfoBarAction(AutofillMetrics::INFOBAR_SHOWN);
}

void AutofillSaveCardDelegate::OnUiAccepted(base::OnceClosure callback) {
  on_finished_gathering_consent_callback_ = std::move(callback);
  // Credit card save acceptance can be logged immediately if:
  // 1. the user is accepting card local save.
  // 2. or when we don't need more info in order to upload.
  if (options_.card_save_type !=
          PaymentsAutofillClient::CardSaveType::kCvcSaveOnly &&
      (!is_for_upload() || !requires_fix_flow())) {
    LogSaveCreditCardPromptResult(
        autofill_metrics::SaveCreditCardPromptResult::kAccepted,
        is_for_upload(), options_);
  }
  LogInfoBarAction(AutofillMetrics::INFOBAR_ACCEPTED);
  GatherAdditionalConsentIfApplicable(/*user_provided_details=*/{});
}

void AutofillSaveCardDelegate::OnUiUpdatedAndAccepted(
    PaymentsAutofillClient::UserProvidedCardDetails user_provided_details) {
  LogInfoBarAction(AutofillMetrics::INFOBAR_ACCEPTED);
  GatherAdditionalConsentIfApplicable(user_provided_details);
}

void AutofillSaveCardDelegate::OnUiUpdatedAndAcceptedForSaveAndFill(
    PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails
        user_provided_details) {
  RunSaveAndFillCardDialogCallback(
      PaymentsAutofillClient::CardSaveAndFillDialogUserDecision::kAccepted,
      user_provided_details);
}

void AutofillSaveCardDelegate::OnUiCanceled() {
  if (is_for_upload() || is_for_local_save()) {
    RunSaveCardPromptCallback(
        PaymentsAutofillClient::SaveCardOfferUserDecision::kDeclined,
        /*user_provided_details=*/{});
    // TODO(crbug.com/394337666): Remove logging
    // `AutofillMetrics::InfoBarMetric` for iOS.
    LogInfoBarAction(AutofillMetrics::INFOBAR_DENIED);
    if (options_.card_save_type !=
        PaymentsAutofillClient::CardSaveType::kCvcSaveOnly) {
      LogSaveCreditCardPromptResult(
          autofill_metrics::SaveCreditCardPromptResult::kDenied,
          is_for_upload(), options_);
    }
  } else {
    RunSaveAndFillCardDialogCallback(
        PaymentsAutofillClient::CardSaveAndFillDialogUserDecision::kDeclined,
        /*user_provided_details=*/{});
  }
}

void AutofillSaveCardDelegate::OnUiIgnored() {
  if (!had_user_interaction_) {
    if (is_for_upload() || is_for_local_save()) {
      RunSaveCardPromptCallback(
          PaymentsAutofillClient::SaveCardOfferUserDecision::kIgnored,
          /*user_provided_details=*/{});
      LogInfoBarAction(AutofillMetrics::INFOBAR_IGNORED);
      if (options_.card_save_type !=
          PaymentsAutofillClient::CardSaveType::kCvcSaveOnly) {
        LogSaveCreditCardPromptResult(
            autofill_metrics::SaveCreditCardPromptResult::kIgnored,
            is_for_upload(), options_);
      }
    } else {
      RunSaveAndFillCardDialogCallback(
          PaymentsAutofillClient::CardSaveAndFillDialogUserDecision::kIgnored,
          /*user_provided_details=*/{});
    }
  }
}

const PaymentsAutofillClient::SaveCreditCardOptions&
AutofillSaveCardDelegate::GetSaveCreditCardOptions() const {
  return options_;
}

void AutofillSaveCardDelegate::OnFinishedGatheringConsent(
    PaymentsAutofillClient::SaveCardOfferUserDecision user_decision,
    PaymentsAutofillClient::UserProvidedCardDetails user_provided_details) {
  RunSaveCardPromptCallback(user_decision, user_provided_details);
  if (!on_finished_gathering_consent_callback_.is_null()) {
    std::move(on_finished_gathering_consent_callback_).Run();
  }
}

void AutofillSaveCardDelegate::RunSaveCardPromptCallback(
    PaymentsAutofillClient::SaveCardOfferUserDecision user_decision,
    PaymentsAutofillClient::UserProvidedCardDetails user_provided_details) {
  if (is_for_upload()) {
    PaymentsAutofillClient::UploadSaveCardPromptCallback
        upload_save_card_callback =
            std::get<PaymentsAutofillClient::UploadSaveCardPromptCallback>(
                std::move(save_card_callback_));
    if (upload_save_card_callback.is_null()) {
      return;
    }
    std::move(upload_save_card_callback)
        .Run(user_decision, user_provided_details);
  } else {
    PaymentsAutofillClient::LocalSaveCardPromptCallback
        local_save_card_callback =
            std::get<PaymentsAutofillClient::LocalSaveCardPromptCallback>(
                std::move(save_card_callback_));
    if (local_save_card_callback.is_null()) {
      return;
    }
    std::move(local_save_card_callback).Run(user_decision);
  }
}

void AutofillSaveCardDelegate::RunSaveAndFillCardDialogCallback(
    PaymentsAutofillClient::CardSaveAndFillDialogUserDecision user_decision,
    PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails
        user_provided_details) {
  PaymentsAutofillClient::CardSaveAndFillDialogCallback
      card_save_and_fill_dialog_callback =
          std::get<PaymentsAutofillClient::CardSaveAndFillDialogCallback>(
              std::move(save_card_callback_));
  if (card_save_and_fill_dialog_callback.is_null()) {
    return;
  }
  std::move(card_save_and_fill_dialog_callback)
      .Run(user_decision, user_provided_details);
}

void AutofillSaveCardDelegate::GatherAdditionalConsentIfApplicable(
    PaymentsAutofillClient::UserProvidedCardDetails user_provided_details) {
  OnFinishedGatheringConsent(
      PaymentsAutofillClient::SaveCardOfferUserDecision::kAccepted,
      user_provided_details);
}

void AutofillSaveCardDelegate::LogInfoBarAction(
    AutofillMetrics::InfoBarMetric action) {
  CHECK(!had_user_interaction_);
  if (options_.card_save_type ==
      PaymentsAutofillClient::CardSaveType::kCvcSaveOnly) {
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
