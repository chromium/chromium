// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/payments/save_and_fill_manager_impl.h"

#include "base/check_deref.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
#include "components/autofill/core/browser/payments/multiple_request_payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/save_and_fill_manager.h"
#include "components/autofill/core/browser/strike_databases/payments/save_and_fill_strike_database.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"

namespace autofill::payments {

namespace {

// If set, overrides the return value of IsCreditCardUploadEnabled() for tests.
std::optional<bool> credit_card_upload_enabled_override_;

}  // namespace

SaveAndFillManagerImpl::SaveAndFillManagerImpl(AutofillClient* autofill_client)
    : autofill_client_(CHECK_DEREF(autofill_client)) {}

SaveAndFillManagerImpl::~SaveAndFillManagerImpl() = default;

void SaveAndFillManagerImpl::OnDidAcceptCreditCardSaveAndFillSuggestion(
    FillCardCallback fill_card_callback) {
  fill_card_callback_ = std::move(fill_card_callback);

  if (IsCreditCardUploadEnabled()) {
    payments_autofill_client()->ShowCreditCardSaveAndFillPendingDialog();

    PopulateInitialUploadDetails();

    payments_autofill_client()
        ->GetMultipleRequestPaymentsNetworkInterface()
        ->GetDetailsForCreateCard(
            upload_details_,
            base::BindOnce(
                &SaveAndFillManagerImpl::OnDidGetDetailsForCreateCard,
                weak_ptr_factory_.GetWeakPtr()));
  } else {
    OfferLocalSaveAndFill();
  }
}

bool SaveAndFillManagerImpl::IsMaxStrikesLimitReached() {
  if (auto* strike_database = GetSaveAndFillStrikeDatabase()) {
    return strike_database->ShouldBlockFeature();
  }
  return false;
}

void SaveAndFillManagerImpl::OnUserDidDecideOnLocalSave(
    CardSaveAndFillDialogUserDecision user_decision,
    const UserProvidedCardSaveAndFillDetails&
        user_provided_card_save_and_fill_details) {
  switch (user_decision) {
    case CardSaveAndFillDialogUserDecision::kAccepted: {
      CreditCard card_save_candidate;
      PopulateCreditCardInfo(card_save_candidate,
                             user_provided_card_save_and_fill_details);

      // The CVC value should still be filled as long as the user provided it
      // even if CVC storage isn't enabled.
      if (fill_card_callback_) {
        std::move(fill_card_callback_).Run(card_save_candidate);
      }

      if (!card_save_candidate.cvc().empty() &&
          !payments_autofill_client()
               ->GetPaymentsDataManager()
               .IsPaymentCvcStorageEnabled()) {
        card_save_candidate.clear_cvc();
      }
      payments_autofill_client()
          ->GetPaymentsDataManager()
          .OnAcceptedLocalCreditCardSave(card_save_candidate);
      break;
    }
    case CardSaveAndFillDialogUserDecision::kDeclined:
      break;
  }
  fill_card_callback_.Reset();
}

void SaveAndFillManagerImpl::SetCreditCardUploadEnabledOverrideForTesting(
    bool credit_card_upload_enabled_override) {
  credit_card_upload_enabled_override_ = credit_card_upload_enabled_override;
}

void SaveAndFillManagerImpl::OfferLocalSaveAndFill() {
  payments_autofill_client()->ShowCreditCardLocalSaveAndFillDialog(
      base::BindOnce(&SaveAndFillManagerImpl::OnUserDidDecideOnLocalSave,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SaveAndFillManagerImpl::PopulateCreditCardInfo(
    CreditCard& card,
    const UserProvidedCardSaveAndFillDetails&
        user_provided_card_save_and_fill_details) {
  const std::string app_locale =
      payments_autofill_client()->GetPaymentsDataManager().app_locale();

  card.SetInfo(CREDIT_CARD_NUMBER,
               user_provided_card_save_and_fill_details.card_number,
               app_locale);
  card.SetInfo(CREDIT_CARD_NAME_FULL,
               user_provided_card_save_and_fill_details.cardholder_name,
               app_locale);
  card.SetInfo(
      CREDIT_CARD_VERIFICATION_CODE,
      user_provided_card_save_and_fill_details.security_code.value_or(u""),
      app_locale);
  card.SetInfo(CREDIT_CARD_EXP_MONTH,
               user_provided_card_save_and_fill_details.expiration_date_month,
               app_locale);
  card.SetInfo(CREDIT_CARD_EXP_2_DIGIT_YEAR,
               user_provided_card_save_and_fill_details.expiration_date_year,
               app_locale);
}

bool SaveAndFillManagerImpl::IsCreditCardUploadEnabled() const {
  if (credit_card_upload_enabled_override_.has_value()) {
    return credit_card_upload_enabled_override_.value();
  }
  const PaymentsDataManager& payments_data_manager =
      payments_autofill_client()->GetPaymentsDataManager();
  return autofill::IsCreditCardUploadEnabled(
      autofill_client_->GetSyncService(), *autofill_client_->GetPrefs(),
      payments_data_manager.GetCountryCodeForExperimentGroup(),
      payments_data_manager.GetPaymentsSigninStateForMetrics(),
      autofill_client_->GetCurrentLogManager());
}

void SaveAndFillManagerImpl::OnDidGetDetailsForCreateCard(
    PaymentsAutofillClient::PaymentsRpcResult result,
    const std::u16string& context_token,
    std::unique_ptr<base::Value::Dict> legal_message,
    std::vector<std::pair<int, int>> supported_card_bin_ranges) {
  if (result == PaymentsRpcResult::kSuccess) {
    LegalMessageLines parsed_legal_message_lines;
    LegalMessageLine::Parse(*legal_message, &parsed_legal_message_lines,
                            /*escape_apostrophes=*/true);
    if (parsed_legal_message_lines.empty()) {
      // If parsing the legal messages fails, upload Save and Fill should not be
      // offered. Offer local Save and Fill instead.
      OfferLocalSaveAndFill();
      return;
    }
    upload_details_.context_token = context_token;
    OfferUploadSaveAndFill(parsed_legal_message_lines);
  } else {
    // If the pre-flight call fails, fall back to offering local Save and Fill.
    OfferLocalSaveAndFill();
  }
}

void SaveAndFillManagerImpl::PopulateInitialUploadDetails() {
  // For "Save and Fill" flow, we don't know whether CVC will be provided by the
  // user so we only check the CVC storage user preference to populate the
  // signal.
  if (payments_autofill_client()
          ->GetPaymentsDataManager()
          .IsPaymentCvcStorageEnabled()) {
    upload_details_.client_behavior_signals.emplace_back(
        ClientBehaviorConstants::kOfferingToSaveCvc);
  }
  // TODO(crbug.com/432100446): Add kShowAccountEmailInLegalMessage to
  // `client_behavior_signals` when feature launched to mobile.

  upload_details_.upload_card_source = UploadCardSource::UPSTREAM_SAVE_AND_FILL;
  upload_details_.billing_customer_number = payments::GetBillingCustomerId(
      payments_autofill_client()->GetPaymentsDataManager());
  upload_details_.app_locale = autofill_client_->GetAppLocale();

  // Calculate the unique address from the most recently used
  // addresses. Can be empty if there is none.
  auto comparator = [](AutofillProfile a, AutofillProfile b) {
    return a.GetAddress() != b.GetAddress();
  };
  std::set<AutofillProfile, decltype(comparator)> candidate_profiles;
  constexpr base::TimeDelta fifteen_minutes = base::Minutes(15);
  for (const AutofillProfile* profile :
       autofill_client_->GetPersonalDataManager()
           .address_data_manager()
           .GetProfiles()) {
    if ((base::Time::Now() - profile->usage_history().use_date()) <=
            fifteen_minutes ||
        (base::Time::Now() - profile->usage_history().modification_date()) <=
            fifteen_minutes) {
      candidate_profiles.emplace(*profile);
    }
  }
  if (candidate_profiles.size() == 1U) {
    upload_details_.profiles.emplace_back(*candidate_profiles.begin());
  }
}

void SaveAndFillManagerImpl::OfferUploadSaveAndFill(
    const LegalMessageLines& parsed_legal_message_lines) {
  payments_autofill_client()->ShowCreditCardUploadSaveAndFillDialog(
      std::move(parsed_legal_message_lines),
      base::BindOnce(&SaveAndFillManagerImpl::OnUserDidDecideOnUploadSave,
                     weak_ptr_factory_.GetWeakPtr()));

  payments_autofill_client()->LoadRiskData(
      base::BindOnce(&SaveAndFillManagerImpl::OnDidLoadRiskData,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SaveAndFillManagerImpl::OnUserDidDecideOnUploadSave(
    CardSaveAndFillDialogUserDecision user_decision,
    const UserProvidedCardSaveAndFillDetails&
        user_provided_card_save_and_fill_details) {
  // TODO(crbug.com/378164165): Implement logic to handle user decision for
  // upload Save and Fill dialog.
  switch (user_decision) {
    case CardSaveAndFillDialogUserDecision::kAccepted:
      upload_save_and_fill_dialog_accepted_ = true;
      break;
    case CardSaveAndFillDialogUserDecision::kDeclined:
      break;
  }
}

void SaveAndFillManagerImpl::OnDidLoadRiskData(const std::string& risk_data) {
  upload_details_.risk_data = risk_data;
  if (upload_save_and_fill_dialog_accepted_) {
    SendCreateCardRequest();
  }
}

void SaveAndFillManagerImpl::SendCreateCardRequest() {
  payments_autofill_client()
      ->GetMultipleRequestPaymentsNetworkInterface()
      ->CreateCard(upload_details_,
                   base::BindOnce(&SaveAndFillManagerImpl::OnDidCreateCard,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void SaveAndFillManagerImpl::OnDidCreateCard(
    PaymentsAutofillClient::PaymentsRpcResult result,
    const std::string& instrument_id) {
  // TODO(crbug.com/378164165): Implement logic to handle CreateCard response
  // and the instrument id.
}

SaveAndFillStrikeDatabase*
SaveAndFillManagerImpl::GetSaveAndFillStrikeDatabase() {
  if (!autofill_client_->GetStrikeDatabase()) {
    return nullptr;
  }
  if (!save_and_fill_strike_database_) {
    save_and_fill_strike_database_ =
        std::make_unique<SaveAndFillStrikeDatabase>(
            autofill_client_->GetStrikeDatabase());
  }
  return save_and_fill_strike_database_.get();
}

}  // namespace autofill::payments
