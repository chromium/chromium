// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/iban_save_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/iban_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/strike_databases/payments/iban_save_strike_database.h"

namespace autofill {

IbanSaveManager::IbanSaveManager(AutofillClient* client) : client_(client) {}

IbanSaveManager::~IbanSaveManager() = default;

// static
std::string IbanSaveManager::GetPartialIbanHashString(
    const std::string& value) {
  std::string iban_hash_value = base::NumberToString(StrToHash64Bit(value));
  return iban_hash_value.substr(0, iban_hash_value.length() / 2);
}

bool IbanSaveManager::AttemptToOfferIbanLocalSave(
    const Iban& iban_import_candidate) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  iban_save_candidate_ = iban_import_candidate;
  // If the max strikes limit has been reached, do not show the IBAN save
  // prompt.
  bool show_save_prompt =
      !GetIbanSaveStrikeDatabase()->ShouldBlockFeature(GetPartialIbanHashString(
          base::UTF16ToUTF8(iban_save_candidate_.value())));
  if (!show_save_prompt) {
    autofill_metrics::LogIbanSaveNotOfferedDueToMaxStrikesMetric(
        AutofillMetrics::SaveTypeMetric::LOCAL);
  }

  if (observer_for_testing_) {
    observer_for_testing_->OnOfferLocalSave();
  }

  // If `show_save_prompt`'s value is false, desktop builds will still offer
  // save in the omnibox without popping-up the bubble.
  client_->ConfirmSaveIbanLocally(
      iban_save_candidate_, show_save_prompt,
      base::BindOnce(&IbanSaveManager::OnUserDidDecideOnLocalSave,
                     weak_ptr_factory_.GetWeakPtr()));
  return show_save_prompt;
#else
  // IBAN save prompts do not currently exist on mobile.
  return false;
#endif
}

IbanSaveStrikeDatabase* IbanSaveManager::GetIbanSaveStrikeDatabase() {
  if (iban_save_strike_database_.get() == nullptr) {
    iban_save_strike_database_ = std::make_unique<IbanSaveStrikeDatabase>(
        IbanSaveStrikeDatabase(client_->GetStrikeDatabase()));
  }
  return iban_save_strike_database_.get();
}

void IbanSaveManager::OnUserDidDecideOnLocalSave(
    AutofillClient::SaveIbanOfferUserDecision user_decision,
    const absl::optional<std::u16string>& nickname) {
  if (nickname.has_value()) {
    std::u16string trimmed_nickname;
    base::TrimWhitespace(nickname.value(), base::TRIM_ALL, &trimmed_nickname);
    if (!trimmed_nickname.empty()) {
      iban_save_candidate_.set_nickname(trimmed_nickname);
    }
  }

  const std::string partial_iban_hash =
      GetPartialIbanHashString(base::UTF16ToUTF8(iban_save_candidate_.value()));
  switch (user_decision) {
    case AutofillClient::SaveIbanOfferUserDecision::kAccepted:
      autofill_metrics::LogStrikesPresentWhenIbanSaved(
          iban_save_strike_database_->GetStrikes(partial_iban_hash));
      // Clear all IbanSave strikes for this IBAN, so that if it's later removed
      // the strike count starts over with respect to re-saving it.
      GetIbanSaveStrikeDatabase()->ClearStrikes(partial_iban_hash);
      client_->GetPersonalDataManager()->OnAcceptedLocalIbanSave(
          iban_save_candidate_);
      if (observer_for_testing_) {
        observer_for_testing_->OnAcceptSaveIbanComplete();
      }
      break;
    case AutofillClient::SaveIbanOfferUserDecision::kIgnored:
    case AutofillClient::SaveIbanOfferUserDecision::kDeclined:
      GetIbanSaveStrikeDatabase()->AddStrike(partial_iban_hash);
      if (observer_for_testing_) {
        observer_for_testing_->OnDeclineSaveIbanComplete();
      }
      break;
  }
}

}  // namespace autofill
