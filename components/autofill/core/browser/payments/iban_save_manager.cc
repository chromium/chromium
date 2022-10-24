// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/iban_save_manager.h"

#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/personal_data_manager.h"

namespace autofill {

IBANSaveManager::IBANSaveManager(PersonalDataManager* personal_data_manager)
    : personal_data_manager_(personal_data_manager) {}

IBANSaveManager::~IBANSaveManager() = default;

bool IBANSaveManager::AttemptToOfferIBANLocalSave(
    const absl::optional<IBAN>& iban_import_candidate) {
  if (!iban_import_candidate || personal_data_manager_->IsOffTheRecord())
    return false;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  iban_save_candidate_ = iban_import_candidate.value();
  // TODO(crbug.com/1349109): Display the IBAN save prompt, and pass in a
  // callback to OnUserDidDecideOnLocalSave() that will be called when the user
  // makes a decision on the IBAN save prompt.
  return true;
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
      // TODO(crbug.com/1349109): Clear all IBANSave strikes for this IBAN.
      personal_data_manager_->OnAcceptedLocalIBANSave(iban_save_candidate_);
      break;
    case AutofillClient::SaveIBANOfferUserDecision::kIgnored:
    case AutofillClient::SaveIBANOfferUserDecision::kDeclined:
      // TODO(crbug.com/1349109): Add IBANSave strike for this IBAN.
      break;
  }
}

}  // namespace autofill
