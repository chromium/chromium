// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/local_card_migration_manager.h"

#include <stddef.h>

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/local_card_migration_metrics.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace autofill {
namespace {

using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;

}  // namespace

MigratableCreditCard::MigratableCreditCard(const CreditCard& credit_card)
    : credit_card_(credit_card) {}

MigratableCreditCard::MigratableCreditCard(const MigratableCreditCard&) =
    default;

MigratableCreditCard::MigratableCreditCard(MigratableCreditCard&&) = default;

MigratableCreditCard& MigratableCreditCard::operator=(
    const MigratableCreditCard&) = default;

MigratableCreditCard& MigratableCreditCard::operator=(MigratableCreditCard&&) =
    default;

MigratableCreditCard::~MigratableCreditCard() = default;

LocalCardMigrationManager::LocalCardMigrationManager(
    AutofillClient* client,
    const std::string& app_locale)
    : client_(CHECK_DEREF(client)), app_locale_(app_locale) {}

LocalCardMigrationManager::~LocalCardMigrationManager() = default;

bool LocalCardMigrationManager::ShouldOfferLocalCardMigration(
    const std::optional<CreditCard>& extracted_credit_card,
    int credit_card_import_type) {
  // Reset and store the extracted credit card info for a later check of whether
  // the extracted card is supported.
  extracted_credit_card_number_.reset();
  if (extracted_credit_card) {
    extracted_credit_card_number_ = extracted_credit_card->number();
  }
  credit_card_import_type_ = credit_card_import_type;
  // Must be an existing card. New cards always get Upstream or local save.
  switch (credit_card_import_type_) {
    case FormDataImporter::CreditCardImportType::kLocalCard:
      local_card_migration_origin_ =
          autofill_metrics::LocalCardMigrationOrigin::UseOfLocalCard;
      break;
    case FormDataImporter::CreditCardImportType::kServerCard:
      local_card_migration_origin_ =
          autofill_metrics::LocalCardMigrationOrigin::UseOfServerCard;
      break;
    default:
      autofill_metrics::LogLocalCardMigrationDecisionMetric(
          autofill_metrics::LocalCardMigrationDecisionMetric::
              NOT_OFFERED_USE_NEW_CARD);
      return false;
  }

  if (!IsCreditCardMigrationEnabled()) {
    autofill_metrics::LogLocalCardMigrationDecisionMetric(
        autofill_metrics::LocalCardMigrationDecisionMetric::
            NOT_OFFERED_FAILED_PREREQUISITES);
    return false;
  }

  if (GetLocalCardMigrationStrikeDatabase()->ShouldBlockFeature()) {
    switch (credit_card_import_type_) {
      case FormDataImporter::CreditCardImportType::kLocalCard:
        autofill_metrics::LogLocalCardMigrationNotOfferedDueToMaxStrikesMetric(
            AutofillMetrics::SaveTypeMetric::LOCAL);
        break;
      case FormDataImporter::CreditCardImportType::kServerCard:
        autofill_metrics::LogLocalCardMigrationNotOfferedDueToMaxStrikesMetric(
            AutofillMetrics::SaveTypeMetric::SERVER);
        break;
    }
    autofill_metrics::LogLocalCardMigrationDecisionMetric(
        autofill_metrics::LocalCardMigrationDecisionMetric::
            NOT_OFFERED_REACHED_MAX_STRIKE_COUNT);
    return false;
  }

  // Fetch all migratable credit cards and store in |migratable_credit_cards_|.
  GetMigratableCreditCards();

  // If the form was submitted with a local card, only offer migration instead
  // of Upstream if there are other local cards to migrate as well. If the form
  // was submitted with a server card, offer migration if ANY local cards can be
  // migrated.
  if ((credit_card_import_type_ ==
           FormDataImporter::CreditCardImportType::kLocalCard &&
       migratable_credit_cards_.size() > 1) ||
      (credit_card_import_type_ ==
           FormDataImporter::CreditCardImportType::kServerCard &&
       !migratable_credit_cards_.empty())) {
    return true;
  } else if (credit_card_import_type_ ==
                 FormDataImporter::CreditCardImportType::kLocalCard &&
             migratable_credit_cards_.size() == 1) {
    autofill_metrics::LogLocalCardMigrationDecisionMetric(
        autofill_metrics::LocalCardMigrationDecisionMetric::
            NOT_OFFERED_SINGLE_LOCAL_CARD);
    return false;
  } else {
    autofill_metrics::LogLocalCardMigrationDecisionMetric(
        autofill_metrics::LocalCardMigrationDecisionMetric::
            NOT_OFFERED_NO_MIGRATABLE_CARDS);
    return false;
  }
}

void LocalCardMigrationManager::AttemptToOfferLocalCardMigration(
    bool is_from_settings_page) {
  payments::PaymentsNetworkInterface* payments_network_interface =
      client_->GetPaymentsAutofillClient()->GetPaymentsNetworkInterface();
  // If `payments_network_interface` is nullptr, we can not offer local card
  // migration as it requires a server call.
  if (!payments_network_interface) {
    return;
  }
  migration_request_ =
      payments::PaymentsNetworkInterface::MigrationRequestDetails();

  if (observer_for_testing_)
    observer_for_testing_->OnDecideToRequestLocalCardMigration();

  payments_network_interface->GetCardUploadDetails(
      std::vector<AutofillProfile>(), GetDetectedValues(),
      /*client_behavior_signals=*/std::vector<ClientBehaviorConstants>(),
      app_locale_,
      base::BindOnce(&LocalCardMigrationManager::OnDidGetUploadDetails,
                     weak_ptr_factory_.GetWeakPtr(), is_from_settings_page),
      payments::kMigrateCardsBillableServiceNumber,
      payments::GetBillingCustomerId(&payments_data_manager()),
      is_from_settings_page
          ? payments::PaymentsNetworkInterface::UploadCardSource::
                LOCAL_CARD_MIGRATION_SETTINGS_PAGE
          : payments::PaymentsNetworkInterface::UploadCardSource::
                LOCAL_CARD_MIGRATION_CHECKOUT_FLOW);
}

// Callback function when user agrees to migration on the intermediate dialog.
// Call ShowMainMigrationDialog() to pop up a larger, modal dialog showing the
// local cards to be uploaded.
void LocalCardMigrationManager::OnUserAcceptedIntermediateMigrationDialog() {
  autofill_metrics::LogLocalCardMigrationPromptMetric(
      local_card_migration_origin_,
      autofill_metrics::INTERMEDIATE_BUBBLE_ACCEPTED);
  ShowMainMigrationDialog();
}

// Send the migration request once risk data is available.
void LocalCardMigrationManager::OnUserAcceptedMainMigrationDialog(
    const std::vector<std::string>& selected_card_guids) {
  user_accepted_main_migration_dialog_ = true;
  autofill_metrics::LogLocalCardMigrationPromptMetric(
      local_card_migration_origin_, autofill_metrics::MAIN_DIALOG_ACCEPTED);

  // Log number of LocalCardMigration strikes when migration was accepted.
  base::UmaHistogramCounts1000(
      "Autofill.StrikeDatabase.StrikesPresentWhenLocalCardMigrationAccepted",
      GetLocalCardMigrationStrikeDatabase()->GetStrikes());

  // Update the |migratable_credit_cards_| with the |selected_card_guids|. This
  // will remove any card from |migratable_credit_cards_| of which the GUID is
  // not in |selected_card_guids|.
  auto card_is_selected = [&selected_card_guids](MigratableCreditCard& card) {
    return !base::Contains(selected_card_guids, card.credit_card().guid());
  };
  std::erase_if(migratable_credit_cards_, card_is_selected);
  // Populating risk data and offering migration two-round pop-ups occur
  // asynchronously. If |migration_risk_data_| has already been loaded, send the
  // migrate local cards request. Otherwise, continue to wait and let
  // OnDidGetUploadRiskData handle it.
  if (!migration_request_.risk_data.empty())
    SendMigrateLocalCardsRequest();
}

void LocalCardMigrationManager::OnUserDeletedLocalCardViaMigrationDialog(
    const std::string& deleted_card_guid) {
  client_->GetPersonalDataManager()->RemoveByGUID(deleted_card_guid);
}

bool LocalCardMigrationManager::IsCreditCardMigrationEnabled() {
  return ::autofill::IsCreditCardMigrationEnabled(
      client_->GetPersonalDataManager(), client_->GetSyncService(),
      /*is_test_mode=*/observer_for_testing_, client_->GetLogManager());
}

void LocalCardMigrationManager::OnDidGetUploadDetails(
    bool is_from_settings_page,
    PaymentsRpcResult result,
    const std::u16string& context_token,
    std::unique_ptr<base::Value::Dict> legal_message,
    std::vector<std::pair<int, int>> supported_card_bin_ranges) {
  if (observer_for_testing_)
    observer_for_testing_->OnReceivedGetUploadDetailsResponse();

  if (result == PaymentsRpcResult::kSuccess) {
    LegalMessageLine::Parse(*legal_message, &legal_message_lines_,
                            /*escape_apostrophes=*/true);

    if (legal_message_lines_.empty()) {
      autofill_metrics::LogLocalCardMigrationDecisionMetric(
          autofill_metrics::LocalCardMigrationDecisionMetric::
              NOT_OFFERED_INVALID_LEGAL_MESSAGE);
      return;
    }

    migration_request_.context_token = context_token;
    migration_request_.risk_data.clear();

    // If we successfully received the legal docs, trigger the offer-to-migrate
    // dialog. If triggered from settings page, we pop-up the main prompt
    // directly. If not, we pop up the intermediate bubble.
    if (is_from_settings_page) {
      // Set the origin to SettingsPage.
      local_card_migration_origin_ =
          autofill_metrics::LocalCardMigrationOrigin::SettingsPage;
      // Pops up a larger, modal dialog showing the local cards to be uploaded.
      ShowMainMigrationDialog();
    } else {
      // Check if an extracted local card is listed in
      // `supported_card_bin_ranges`. Abort the migration when the user uses an
      // unsupported local card.
      if (!supported_card_bin_ranges.empty() &&
          credit_card_import_type_ ==
              FormDataImporter::CreditCardImportType::kLocalCard &&
          extracted_credit_card_number_.has_value() &&
          !payments::IsCreditCardNumberSupported(
              extracted_credit_card_number_.value(),
              supported_card_bin_ranges)) {
        autofill_metrics::LogLocalCardMigrationDecisionMetric(
            autofill_metrics::LocalCardMigrationDecisionMetric::
                NOT_OFFERED_USE_UNSUPPORTED_LOCAL_CARD);
        return;
      }
      // Filter the migratable credit cards with |supported_card_bin_ranges|.
      FilterOutUnsupportedLocalCards(supported_card_bin_ranges);
      // Abandon the migration if no supported card left.
      if (migratable_credit_cards_.empty()) {
        autofill_metrics::LogLocalCardMigrationDecisionMetric(
            autofill_metrics::LocalCardMigrationDecisionMetric::
                NOT_OFFERED_NO_SUPPORTED_CARDS);
        return;
      }
      client_->GetPaymentsAutofillClient()->ShowLocalCardMigrationDialog(
          base::BindOnce(&LocalCardMigrationManager::
                             OnUserAcceptedIntermediateMigrationDialog,
                         weak_ptr_factory_.GetWeakPtr()));
      autofill_metrics::LogLocalCardMigrationPromptMetric(
          local_card_migration_origin_,
          autofill_metrics::INTERMEDIATE_BUBBLE_SHOWN);
    }

    client_->GetPaymentsAutofillClient()->LoadRiskData(
        base::BindOnce(&LocalCardMigrationManager::OnDidGetMigrationRiskData,
                       weak_ptr_factory_.GetWeakPtr()));
    autofill_metrics::LogLocalCardMigrationDecisionMetric(
        autofill_metrics::LocalCardMigrationDecisionMetric::OFFERED);
  } else {
    autofill_metrics::LogLocalCardMigrationDecisionMetric(
        autofill_metrics::LocalCardMigrationDecisionMetric::
            NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED);
  }
}

void LocalCardMigrationManager::OnDidMigrateLocalCards(
    PaymentsRpcResult result,
    std::unique_ptr<std::unordered_map<std::string, std::string>> save_result,
    const std::string& display_text) {
  if (observer_for_testing_)
    observer_for_testing_->OnReceivedMigrateCardsResponse();

  if (!save_result)
    return;

  if (result == PaymentsRpcResult::kSuccess) {
    std::vector<CreditCard> migrated_cards;
    // Traverse the migratable credit cards to update each migrated card status.
    for (MigratableCreditCard& card : migratable_credit_cards_) {
      // If it is run in a test, count all cards as successfully migrated.
      if (observer_for_testing_) {
        migrated_cards.push_back(card.credit_card());
        continue;
      }

      // Not every card exists in the |save_result| since some cards are
      // unchecked by the user and not migrated.
      auto it = save_result->find(card.credit_card().guid());
      // If current card does not exist in the |save_result|, skip it.
      if (it == save_result->end())
        continue;

      // Otherwise update its migration status. Server-side response can return
      // SUCCESS, TEMPORARY_FAILURE, or PERMANENT_FAILURE (see SaveResult
      // enum). Branch here depending on which is received.
      if (it->second == kMigrationResultPermanentFailure ||
          it->second == kMigrationResultTemporaryFailure) {
        card.set_migration_status(
            MigratableCreditCard::MigrationStatus::FAILURE_ON_UPLOAD);
      } else if (it->second == kMigrationResultSuccess) {
        card.set_migration_status(
            MigratableCreditCard::MigrationStatus::SUCCESS_ON_UPLOAD);
        migrated_cards.push_back(card.credit_card());
      } else {
        NOTREACHED_IN_MIGRATION();
      }
    }

    // Remove cards that were successfully migrated from local storage.
    payments_data_manager().DeleteLocalCreditCards(migrated_cards);
  }

  client_->GetPaymentsAutofillClient()->ShowLocalCardMigrationResults(
      result != PaymentsRpcResult::kSuccess, base::UTF8ToUTF16(display_text),
      migratable_credit_cards_,
      base::BindRepeating(
          &LocalCardMigrationManager::OnUserDeletedLocalCardViaMigrationDialog,
          weak_ptr_factory_.GetWeakPtr()));
}

void LocalCardMigrationManager::OnDidGetMigrationRiskData(
    const std::string& risk_data) {
  migration_request_.risk_data = risk_data;
  // Populating risk data and offering migration two-round pop-ups occur
  // asynchronously. If the main migration dialog has already been accepted,
  // send the migrate local cards request. Otherwise, continue to wait for the
  // user to accept the two round dialog.
  if (user_accepted_main_migration_dialog_)
    SendMigrateLocalCardsRequest();
}

// Send the migration request. Will call
// `client_->GetPaymentsAutofillClient()->GetPaymentsNetworkInterface()` to
// create a new PaymentsRequest. Also create a new callback function
// OnDidMigrateLocalCards.
void LocalCardMigrationManager::SendMigrateLocalCardsRequest() {
  if (observer_for_testing_)
    observer_for_testing_->OnSentMigrateCardsRequest();

  migration_request_.app_locale = app_locale_;
  migration_request_.billing_customer_number =
      payments::GetBillingCustomerId(&payments_data_manager());
  client_->GetPaymentsAutofillClient()
      ->GetPaymentsNetworkInterface()
      ->MigrateCards(
          migration_request_, migratable_credit_cards_,
          base::BindOnce(&LocalCardMigrationManager::OnDidMigrateLocalCards,
                         weak_ptr_factory_.GetWeakPtr()));
  user_accepted_main_migration_dialog_ = false;
}

LocalCardMigrationStrikeDatabase*
LocalCardMigrationManager::GetLocalCardMigrationStrikeDatabase() {
  if (local_card_migration_strike_database_.get() == nullptr) {
    local_card_migration_strike_database_ =
        std::make_unique<LocalCardMigrationStrikeDatabase>(
            LocalCardMigrationStrikeDatabase(client_->GetStrikeDatabase()));
  }
  return local_card_migration_strike_database_.get();
}

// Pops up a larger, modal dialog showing the local cards to be uploaded. Pass
// the reference of vector<MigratableCreditCard> and the callback function is
// OnUserAcceptedMainMigrationDialog(). Can be called when user agrees to
// migration on the intermediate dialog or directly from settings page.
void LocalCardMigrationManager::ShowMainMigrationDialog() {
  autofill_metrics::LogLocalCardMigrationPromptMetric(
      local_card_migration_origin_, autofill_metrics::MAIN_DIALOG_SHOWN);
  // Pops up a larger, modal dialog showing the local cards to be uploaded.
  client_->GetPaymentsAutofillClient()->ConfirmMigrateLocalCardToCloud(
      legal_message_lines_,
      payments_data_manager().GetAccountInfoForPaymentsServer().email,
      migratable_credit_cards_,
      base::BindOnce(
          &LocalCardMigrationManager::OnUserAcceptedMainMigrationDialog,
          weak_ptr_factory_.GetWeakPtr()));
}

int LocalCardMigrationManager::GetDetectedValues() const {
  int detected_values = 0;

  // If all cards to be migrated have a cardholder name, include it in the
  // detected values.
  bool all_cards_have_cardholder_name = true;
  for (MigratableCreditCard migratable_credit_card : migratable_credit_cards_) {
    all_cards_have_cardholder_name &=
        !migratable_credit_card.credit_card()
             .GetInfo(CREDIT_CARD_NAME_FULL, app_locale_)
             .empty();
  }
  if (all_cards_have_cardholder_name)
    detected_values |= CreditCardSaveManager::DetectedValue::CARDHOLDER_NAME;

  // Local card migration should ONLY be offered when the user already has a
  // Google Payments account.
  DCHECK_NE(0, payments::GetBillingCustomerId(&payments_data_manager()));
  detected_values |=
      CreditCardSaveManager::DetectedValue::HAS_GOOGLE_PAYMENTS_ACCOUNT;

  return detected_values;
}

void LocalCardMigrationManager::GetMigratableCreditCards() {
  std::vector<CreditCard*> local_credit_cards =
      payments_data_manager().GetLocalCreditCards();

  // Empty previous state.
  migratable_credit_cards_.clear();

  // Initialize the local credit card list and queue for showing and uploading.
  for (const CreditCard* credit_card : local_credit_cards) {
    // If the card is valid (has a valid card number, expiration date, and is
    // not expired) and is not a server card, add it to the list of migratable
    // cards.
    if (credit_card->IsValid() &&
        !payments_data_manager().IsServerCard(credit_card)) {
      migratable_credit_cards_.emplace_back(*credit_card);
    }
  }
}

void LocalCardMigrationManager::FilterOutUnsupportedLocalCards(
    const std::vector<std::pair<int, int>>& supported_card_bin_ranges) {
  if (!supported_card_bin_ranges.empty()) {
    // Update the |migratable_credit_cards_| with the
    // |supported_card_bin_ranges|. This will remove any card from
    // |migratable_credit_cards_| of which the card number is not in
    // |supported_card_bin_ranges|.
    auto card_is_unsupported =
        [&supported_card_bin_ranges](MigratableCreditCard& card) {
          return !payments::IsCreditCardNumberSupported(
              card.credit_card().number(), supported_card_bin_ranges);
        };
    std::erase_if(migratable_credit_cards_, card_is_unsupported);
  }
}

PaymentsDataManager& LocalCardMigrationManager::payments_data_manager() {
  return const_cast<PaymentsDataManager&>(
      const_cast<const LocalCardMigrationManager*>(this)
          ->payments_data_manager());
}

const PaymentsDataManager& LocalCardMigrationManager::payments_data_manager()
    const {
  return client_->GetPersonalDataManager()->payments_data_manager();
}

}  // namespace autofill
