// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_save_card_delegate.h"

#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"

namespace autofill {

AutofillSaveCardDelegate::AutofillSaveCardDelegate(
    absl::variant<AutofillClient::LocalSaveCardPromptCallback,
                  AutofillClient::UploadSaveCardPromptCallback>
        save_card_callback,
    AutofillClient::SaveCreditCardOptions options)
    : options_(options),
      had_user_interaction_(false),
      save_card_callback_(std::move(save_card_callback)) {}

AutofillSaveCardDelegate::~AutofillSaveCardDelegate() = default;

void AutofillSaveCardDelegate::OnUiShown() {
  AutofillMetrics::LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_SHOWN,
                                              is_for_upload(), options_);
}

void AutofillSaveCardDelegate::OnUiAccepted(base::OnceClosure callback) {
  on_finished_gathering_consent_callback_ = std::move(callback);

  // Acceptance can be logged immediately if:
  // 1. the user is accepting local save.
  // 2. or when we don't need more info in order to upload.
  if (!is_for_upload() ||
      (!options_.should_request_name_from_user &&
       !options_.should_request_expiration_date_from_user)) {
    LogSaveCreditCardPromptResult(
        autofill_metrics::SaveCreditCardPromptResult::kAccepted,
        is_for_upload(), options_);
  }
  LogUserAction(AutofillMetrics::INFOBAR_ACCEPTED);
  GatherAdditionalConsentIfApplicable(/*user_provided_details=*/{});
}

void AutofillSaveCardDelegate::OnUiUpdatedAndAccepted(
    AutofillClient::UserProvidedCardDetails user_provided_details) {
  LogUserAction(AutofillMetrics::INFOBAR_ACCEPTED);
  GatherAdditionalConsentIfApplicable(user_provided_details);
}

void AutofillSaveCardDelegate::OnUiCanceled() {
  RunSaveCardPromptCallback(
      AutofillClient::SaveCardOfferUserDecision::kDeclined,
      /*user_provided_details=*/{});
  LogUserAction(AutofillMetrics::INFOBAR_DENIED);
  LogSaveCreditCardPromptResult(
      autofill_metrics::SaveCreditCardPromptResult::kDenied, is_for_upload(),
      options_);
}

void AutofillSaveCardDelegate::OnUiIgnored() {
  if (!had_user_interaction_) {
    RunSaveCardPromptCallback(
        AutofillClient::SaveCardOfferUserDecision::kIgnored,
        /*user_provided_details=*/{});
    LogUserAction(AutofillMetrics::INFOBAR_IGNORED);
    LogSaveCreditCardPromptResult(
        autofill_metrics::SaveCreditCardPromptResult::kIgnored, is_for_upload(),
        options_);
  }
}

void AutofillSaveCardDelegate::OnFinishedGatheringConsent(
    AutofillClient::SaveCardOfferUserDecision user_decision,
    AutofillClient::UserProvidedCardDetails user_provided_details) {
  RunSaveCardPromptCallback(user_decision, user_provided_details);
  if (!on_finished_gathering_consent_callback_.is_null()) {
    std::move(on_finished_gathering_consent_callback_).Run();
  }
}

void AutofillSaveCardDelegate::RunSaveCardPromptCallback(
    AutofillClient::SaveCardOfferUserDecision user_decision,
    AutofillClient::UserProvidedCardDetails user_provided_details) {
  if (is_for_upload()) {
    absl::get<AutofillClient::UploadSaveCardPromptCallback>(
        std::move(save_card_callback_))
        .Run(user_decision, user_provided_details);
  } else {
    absl::get<AutofillClient::LocalSaveCardPromptCallback>(
        std::move(save_card_callback_))
        .Run(user_decision);
  }
}

void AutofillSaveCardDelegate::GatherAdditionalConsentIfApplicable(
    AutofillClient::UserProvidedCardDetails user_provided_details) {
  OnFinishedGatheringConsent(
      AutofillClient::SaveCardOfferUserDecision::kAccepted,
      user_provided_details);
}

void AutofillSaveCardDelegate::LogUserAction(
    AutofillMetrics::InfoBarMetric user_action) {
  DCHECK(!had_user_interaction_);

  AutofillMetrics::LogCreditCardInfoBarMetric(user_action, is_for_upload(),
                                              options_);
  had_user_interaction_ = true;
}

}  // namespace autofill
