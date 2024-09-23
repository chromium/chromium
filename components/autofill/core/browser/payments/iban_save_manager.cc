// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/iban_save_manager.h"

#include "base/check_deref.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/strike_databases/payments/iban_save_strike_database.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/sync/service/sync_user_settings.h"

namespace autofill {

using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;

IbanSaveManager::IbanSaveManager(AutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

IbanSaveManager::~IbanSaveManager() = default;

// static
std::string IbanSaveManager::GetPartialIbanHashString(
    const std::string& value) {
  std::string iban_hash_value = base::NumberToString(StrToHash64Bit(value));
  return iban_hash_value.substr(0, iban_hash_value.length() / 2);
}

// static
bool IbanSaveManager::IsIbanUploadEnabled(
    const syncer::SyncService* sync_service,
    AutofillMetrics::PaymentsSigninState signin_state_for_metrics) {
  // If Chrome sync is not active, upload IBAN save is not offered, since the
  // user would not be able to see the IBANs until sync is active again.
  if (!sync_service) {
    autofill_metrics::LogIbanUploadEnabledMetric(
        autofill_metrics::IbanUploadEnabledStatus::kSyncServiceNull,
        signin_state_for_metrics);
    return false;
  }

  // The sync service being paused implies a persistent auth error (for example,
  // the user is signed out). Uploading an IBAN should not be offered in this
  // case since the user is not authenticated, and they won't be able to see the
  // IBANs until sync is turned on.
  if (sync_service->GetTransportState() ==
      syncer::SyncService::TransportState::PAUSED) {
    autofill_metrics::LogIbanUploadEnabledMetric(
        autofill_metrics::IbanUploadEnabledStatus::kSyncServicePaused,
        signin_state_for_metrics);
    return false;
  }

  // When sync service is missing `AUTOFILL_WALLET_DATA` active data type,
  // upload IBAN save is not offered, since the user won't be able to see the
  // IBANs in the settings page.
  if (!sync_service->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA)) {
    autofill_metrics::LogIbanUploadEnabledMetric(
        autofill_metrics::IbanUploadEnabledStatus::
            kSyncServiceMissingAutofillWalletDataActiveType,
        signin_state_for_metrics);
    return false;
  }

  // Also don't offer upload for users that have an explicit sync passphrase.
  // Users who have enabled a passphrase have chosen to not make their sync
  // information accessible to Google. Since upload makes IBAN data available
  // to other Google systems, disable it for passphrase users.
  if (sync_service->GetUserSettings()->IsUsingExplicitPassphrase()) {
    autofill_metrics::LogIbanUploadEnabledMetric(
        autofill_metrics::IbanUploadEnabledStatus::kUsingExplicitSyncPassphrase,
        signin_state_for_metrics);
    return false;
  }

  // Don't offer upload for users that are only syncing locally, since they
  // won't receive the IBANs back from Google Payments.
  if (sync_service->IsLocalSyncEnabled()) {
    autofill_metrics::LogIbanUploadEnabledMetric(
        autofill_metrics::IbanUploadEnabledStatus::kLocalSyncEnabled,
        signin_state_for_metrics);
    return false;
  }

  autofill_metrics::LogIbanUploadEnabledMetric(
      autofill_metrics::IbanUploadEnabledStatus::kEnabled,
      signin_state_for_metrics);
  return true;
}

bool IbanSaveManager::AttemptToOfferSave(Iban& import_candidate) {
#if !BUILDFLAG(IS_IOS)
  UpdateRecordType(import_candidate);
  switch (DetermineHowToSaveIban(import_candidate)) {
    case TypeOfOfferToSave::kDoNotOfferToSave:
      return false;
    case TypeOfOfferToSave::kOfferServerSave:
      return AttemptToOfferUploadSave(import_candidate);
    case TypeOfOfferToSave::kOfferLocalSave:
      return AttemptToOfferLocalSave(import_candidate);
  }
#else
  // IBAN save prompts do not currently exist on iOS.
  return false;
#endif
}

void IbanSaveManager::UpdateRecordType(Iban& import_candidate) {
  if (MatchesExistingServerIban(import_candidate)) {
    import_candidate.set_record_type(Iban::RecordType::kServerIban);
    return;
  }
  if (MatchesExistingLocalIban(import_candidate)) {
    import_candidate.set_record_type(Iban::RecordType::kLocalIban);
    return;
  }
  import_candidate.set_record_type(Iban::RecordType::kUnknown);
}

IbanSaveManager::TypeOfOfferToSave IbanSaveManager::DetermineHowToSaveIban(
    const Iban& import_candidate) const {
  // Server IBANs are ideal and should not offer to resave to the server or
  // locally.
  if (import_candidate.record_type() == Iban::kServerIban) {
    return TypeOfOfferToSave::kDoNotOfferToSave;
  }

  // Trigger server save if available, otherwise local save as long as the IBAN
  // isn't already saved locally.
  if (base::FeatureList::IsEnabled(features::kAutofillEnableServerIban) &&
      IsIbanUploadEnabled(
          client_->GetSyncService(),
          payments_data_manager().GetPaymentsSigninStateForMetrics()) &&
      payments_data_manager().GetServerIbans().size() <= kMaxNumServerIbans) {
    autofill_metrics::LogIbanSaveOfferedCountry(
        import_candidate.GetCountryCode());
    return TypeOfOfferToSave::kOfferServerSave;
  } else if (import_candidate.record_type() != Iban::kLocalIban) {
    autofill_metrics::LogIbanSaveOfferedCountry(
        import_candidate.GetCountryCode());
    return TypeOfOfferToSave::kOfferLocalSave;
  }
  return TypeOfOfferToSave::kDoNotOfferToSave;
}

bool IbanSaveManager::MatchesExistingLocalIban(
    const Iban& import_candidate) const {
  return std::ranges::any_of(payments_data_manager().GetLocalIbans(),
                             [&](const Iban* iban) {
                               return iban->value() == import_candidate.value();
                             });
}

bool IbanSaveManager::MatchesExistingServerIban(
    const Iban& import_candidate) const {
  return std::ranges::any_of(
      payments_data_manager().GetServerIbans(),
      [&import_candidate](const auto& iban) {
        return iban->MatchesPrefixAndSuffix(import_candidate);
      });
}

bool IbanSaveManager::AttemptToOfferLocalSave(Iban& import_candidate) {
  if (observer_for_testing_) {
    observer_for_testing_->OnOfferLocalSave();
  }

  bool show_save_prompt = !GetIbanSaveStrikeDatabase()->ShouldBlockFeature(
      GetPartialIbanHashString(base::UTF16ToUTF8(import_candidate.value())));
  if (!show_save_prompt) {
    autofill_metrics::LogIbanSaveNotOfferedDueToMaxStrikesMetric(
        AutofillMetrics::SaveTypeMetric::LOCAL);
  }

  client_->GetPaymentsAutofillClient()->ConfirmSaveIbanLocally(
      import_candidate, show_save_prompt,
      base::BindOnce(&IbanSaveManager::OnUserDidDecideOnLocalSave,
                     weak_ptr_factory_.GetWeakPtr(), import_candidate));

  return show_save_prompt;
}

bool IbanSaveManager::AttemptToOfferUploadSave(Iban& import_candidate) {
  autofill_metrics::LogUploadIbanMetric(
      import_candidate.record_type() == Iban::kLocalIban
          ? autofill_metrics::UploadIbanOriginMetric::kLocalIban
          : autofill_metrics::UploadIbanOriginMetric::kNewIban,
      autofill_metrics::UploadIbanActionMetric::kOffered);
  bool show_save_prompt = !GetIbanSaveStrikeDatabase()->ShouldBlockFeature(
      GetPartialIbanHashString(base::UTF16ToUTF8(import_candidate.value())));
  client_->GetPaymentsAutofillClient()
      ->GetPaymentsNetworkInterface()
      ->GetIbanUploadDetails(
          client_->GetPersonalDataManager()->app_locale(),
          payments::GetBillingCustomerId(&payments_data_manager()),
          payments::kUploadPaymentMethodBillableServiceNumber,
          import_candidate.GetCountryCode(),
          base::BindOnce(&IbanSaveManager::OnDidGetUploadDetails,
                         weak_ptr_factory_.GetWeakPtr(), show_save_prompt,
                         import_candidate));
  return show_save_prompt;
}

IbanSaveStrikeDatabase* IbanSaveManager::GetIbanSaveStrikeDatabase() {
  if (iban_save_strike_database_.get() == nullptr) {
    iban_save_strike_database_ = std::make_unique<IbanSaveStrikeDatabase>(
        IbanSaveStrikeDatabase(client_->GetStrikeDatabase()));
  }
  return iban_save_strike_database_.get();
}

void IbanSaveManager::OnUserDidDecideOnLocalSave(
    Iban import_candidate,
    payments::PaymentsAutofillClient::SaveIbanOfferUserDecision user_decision,
    std::u16string_view nickname) {
  if (!nickname.empty()) {
    std::u16string trimmed_nickname;
    base::TrimWhitespace(nickname, base::TRIM_ALL, &trimmed_nickname);
    import_candidate.set_nickname(trimmed_nickname);
  }

  const std::string& partial_iban_hash =
      GetPartialIbanHashString(base::UTF16ToUTF8(import_candidate.value()));
  switch (user_decision) {
    case payments::PaymentsAutofillClient::SaveIbanOfferUserDecision::kAccepted:
      autofill_metrics::LogStrikesPresentWhenIbanSaved(
          iban_save_strike_database_->GetStrikes(partial_iban_hash),
          /*is_upload_save=*/false);
      autofill_metrics::LogIbanSaveAcceptedCountry(
          import_candidate.GetCountryCode());
      // Clear all IbanSave strikes for this IBAN, so that if it's later removed
      // the strike count starts over with respect to re-saving it.
      GetIbanSaveStrikeDatabase()->ClearStrikes(partial_iban_hash);
      payments_data_manager().OnAcceptedLocalIbanSave(
          std::move(import_candidate));
      if (observer_for_testing_) {
        observer_for_testing_->OnAcceptSaveIbanComplete();
      }
      break;
    case payments::PaymentsAutofillClient::SaveIbanOfferUserDecision::kIgnored:
    case payments::PaymentsAutofillClient::SaveIbanOfferUserDecision::kDeclined:
      GetIbanSaveStrikeDatabase()->AddStrike(partial_iban_hash);
      if (observer_for_testing_) {
        observer_for_testing_->OnDeclineSaveIbanComplete();
      }
      break;
  }
}

void IbanSaveManager::OnUserDidDecideOnUploadSave(
    Iban import_candidate,
    bool show_save_prompt,
    payments::PaymentsAutofillClient::SaveIbanOfferUserDecision user_decision,
    std::u16string_view nickname) {
  CHECK_NE(import_candidate.record_type(), Iban::kServerIban);
  if (!nickname.empty()) {
    std::u16string trimmed_nickname;
    base::TrimWhitespace(nickname, base::TRIM_ALL, &trimmed_nickname);
    if (!trimmed_nickname.empty()) {
      import_candidate.set_nickname(trimmed_nickname);
    }
  }

  autofill_metrics::UploadIbanActionMetric action_metric;
  switch (user_decision) {
    case payments::PaymentsAutofillClient::SaveIbanOfferUserDecision::kAccepted:
      action_metric = autofill_metrics::UploadIbanActionMetric::kAccepted;
      autofill_metrics::LogIbanSaveAcceptedCountry(
          import_candidate.GetCountryCode());
      SendUploadRequest(import_candidate, show_save_prompt);
      break;
    case payments::PaymentsAutofillClient::SaveIbanOfferUserDecision::kIgnored:
      action_metric = autofill_metrics::UploadIbanActionMetric::kIgnored;
      GetIbanSaveStrikeDatabase()->AddStrike(GetPartialIbanHashString(
          base::UTF16ToUTF8(import_candidate.value())));
      break;
    case payments::PaymentsAutofillClient::SaveIbanOfferUserDecision::kDeclined:
      action_metric = autofill_metrics::UploadIbanActionMetric::kDeclined;
      GetIbanSaveStrikeDatabase()->AddStrike(GetPartialIbanHashString(
          base::UTF16ToUTF8(import_candidate.value())));
      if (observer_for_testing_) {
        observer_for_testing_->OnDeclineSaveIbanComplete();
      }
      break;
  }
  autofill_metrics::LogUploadIbanMetric(
      import_candidate.record_type() == Iban::kLocalIban
          ? autofill_metrics::UploadIbanOriginMetric::kLocalIban
          : autofill_metrics::UploadIbanOriginMetric::kNewIban,
      action_metric);
}

void IbanSaveManager::OnDidGetUploadDetails(
    bool show_save_prompt,
    Iban import_candidate,
    PaymentsRpcResult result,
    const std::u16string& validation_regex,
    const std::u16string& context_token,
    std::unique_ptr<base::Value::Dict> legal_message) {
  if (observer_for_testing_) {
    observer_for_testing_->OnReceivedGetUploadDetailsResponse();
  }

  // Upload should only be offered when result is `kSuccess` and the IBAN passes
  // regex validation.
  if (result == PaymentsRpcResult::kSuccess &&
      MatchesRegex(import_candidate.value(), *CompileRegex(validation_regex))) {
    // Upload should only be offered when legal messages are parsed
    // successfully.
    LegalMessageLines parsed_legal_message_lines;
    if (LegalMessageLine::Parse(*legal_message, &parsed_legal_message_lines,
                                /*escape_apostrophes=*/true)) {
      context_token_ = context_token;
      client_->GetPaymentsAutofillClient()->ConfirmUploadIbanToCloud(
          import_candidate, std::move(parsed_legal_message_lines),
          show_save_prompt,
          base::BindOnce(&IbanSaveManager::OnUserDidDecideOnUploadSave,
                         weak_ptr_factory_.GetWeakPtr(), import_candidate,
                         show_save_prompt));
      // If `show_save_prompt`'s value is false, desktop builds will still offer
      // save in the omnibox without popping-up the bubble.
      if (observer_for_testing_) {
        observer_for_testing_->OnOfferUploadSave();
      }
      return;
    }
  }

  // If the upload details request failed, attempt to offer local save.
  if (!MatchesExistingLocalIban(import_candidate)) {
    AttemptToOfferLocalSave(import_candidate);
  }
}

void IbanSaveManager::SendUploadRequest(const Iban& import_candidate,
                                        bool show_save_prompt) {
  if (observer_for_testing_) {
    observer_for_testing_->OnSentUploadRequest();
  }
  payments::PaymentsNetworkInterface::UploadIbanRequestDetails details;
  details.app_locale = client_->GetPersonalDataManager()->app_locale();
  details.billable_service_number =
      payments::kUploadPaymentMethodBillableServiceNumber;
  details.billing_customer_number =
      payments::GetBillingCustomerId(&payments_data_manager());
  details.context_token = context_token_;
  details.value = import_candidate.value();
  details.nickname = import_candidate.nickname();
  client_->GetPaymentsAutofillClient()
      ->GetPaymentsNetworkInterface()
      ->UploadIban(details, base::BindOnce(&IbanSaveManager::OnDidUploadIban,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           import_candidate, show_save_prompt));
}

void IbanSaveManager::OnDidUploadIban(const Iban& import_candidate,
                                      bool show_save_prompt,
                                      PaymentsRpcResult result) {
  const std::string& partial_iban_hash =
      GetPartialIbanHashString(base::UTF16ToUTF8(import_candidate.value()));
  if (result == PaymentsRpcResult::kSuccess) {
    // Clear all IbanSave strikes for this IBAN, so that if it's later removed
    // the strike count starts over with respect to re-saving it.
    autofill_metrics::LogStrikesPresentWhenIbanSaved(
        iban_save_strike_database_->GetStrikes(partial_iban_hash),
        /*is_upload_save=*/true);
    GetIbanSaveStrikeDatabase()->ClearStrikes(partial_iban_hash);
  } else {
    // If upload save failed, check if the IBAN already exists locally. If not,
    // automatically save the IBAN locally so that the IBAN is not left unsaved
    // since the user intended to save it.
    bool should_local_save = !MatchesExistingLocalIban(import_candidate);
    if (should_local_save) {
      payments_data_manager().AddAsLocalIban(import_candidate);
    }
    autofill_metrics::LogIbanUploadSaveFailed(should_local_save);

    // If the upload failed and the bubble was actually shown (NOT just the
    // icon), count that as a strike against offering upload in the future.
    if (show_save_prompt) {
      GetIbanSaveStrikeDatabase()->AddStrike(partial_iban_hash);
    }
  }

  // Display the IBAN upload save confirmation dialog based on the result.
  client_->GetPaymentsAutofillClient()->IbanUploadCompleted(
      result == PaymentsRpcResult::kSuccess,
      GetIbanSaveStrikeDatabase()->ShouldBlockFeature(partial_iban_hash));
  if (observer_for_testing_) {
    if (result == PaymentsRpcResult::kSuccess) {
      observer_for_testing_->OnAcceptUploadSaveIbanComplete();
    } else {
      observer_for_testing_->OnAcceptUploadSaveIbanFailed();
    }
  }
}

PaymentsDataManager& IbanSaveManager::payments_data_manager() {
  return const_cast<PaymentsDataManager&>(
      const_cast<const IbanSaveManager*>(this)->payments_data_manager());
}

const PaymentsDataManager& IbanSaveManager::payments_data_manager() const {
  return client_->GetPersonalDataManager()->payments_data_manager();
}

}  // namespace autofill
