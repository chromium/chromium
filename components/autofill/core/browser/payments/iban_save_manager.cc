// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/iban_save_manager.h"

#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/payments/iban_save_strike_database.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {

IBANSaveManager::IBANSaveManager(AutofillClient* client)
    : client_(client),
      iban_save_strike_database_(std::make_unique<IBANSaveStrikeDatabase>(
          client->GetStrikeDatabase())) {}

IBANSaveManager::~IBANSaveManager() = default;

bool IBANSaveManager::AttemptToOfferIBANLocalSave(
    const IBAN& iban_import_candidate) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (client_->GetPersonalDataManager()->IsOffTheRecord())
    return false;

  // If the max strikes limit has been reached, do not show the IBAN save
  // prompt.
  show_save_prompt_ = !iban_save_strike_database_->ShouldBlockFeature(
      base::UTF16ToUTF8(iban_import_candidate.value()));

  iban_save_candidate_ = iban_import_candidate;
  // If `show_save_prompt_`'s value is false, desktop builds will still offer
  // save in the omnibox without popping-up the bubble.
  client_->ConfirmSaveIBANLocally(
      iban_save_candidate_, show_save_prompt_,
      base::BindOnce(&IBANSaveManager::OnUserDidDecideOnLocalSave,
                     weak_ptr_factory_.GetWeakPtr()));
  return show_save_prompt_;
#else
  // IBAN save prompts do not currently exist on mobile.
  return false;
#endif
}

void IBANSaveManager::OnUserDidDecideOnLocalSave(
    AutofillClient::SaveIBANOfferUserDecision user_decision,
    const absl::optional<std::u16string>& nickname) {
  if (nickname.has_value()) {
    std::u16string trimmed_nickname;
    base::TrimWhitespace(nickname.value(), base::TRIM_ALL, &trimmed_nickname);
    if (!trimmed_nickname.empty())
      iban_save_candidate_.set_nickname(trimmed_nickname);
  }

  switch (user_decision) {
    case AutofillClient::SaveIBANOfferUserDecision::kAccepted:
      // Clear all IBANSave strikes for this IBAN, so that if it's later removed
      // the strike count starts over with respect to re-saving it.
      iban_save_strike_database_->ClearStrikes(
          base::UTF16ToUTF8(iban_save_candidate_.value()));
      client_->GetPersonalDataManager()->OnAcceptedLocalIBANSave(
          iban_save_candidate_);
      break;
    case AutofillClient::SaveIBANOfferUserDecision::kIgnored:
    case AutofillClient::SaveIBANOfferUserDecision::kDeclined:
      iban_save_strike_database_->AddStrike(
          base::UTF16ToUTF8(iban_save_candidate_.value()));
      break;
  }
}

}  // namespace autofill
