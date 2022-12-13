// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_SAVE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_SAVE_MANAGER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/iban.h"

namespace autofill {

class IBANSaveStrikeDatabase;

// Decides whether an IBAN local save should be offered and handles the workflow
// for local saves.
class IBANSaveManager {
 public:
  explicit IBANSaveManager(AutofillClient* client);
  IBANSaveManager(const IBANSaveManager&) = delete;
  IBANSaveManager& operator=(const IBANSaveManager&) = delete;
  virtual ~IBANSaveManager();

  // Checks that all requirements for offering local IBAN save are fulfilled.
  // Returns true if the save prompt was shown, and false otherwise.
  // Note that on desktop if this returns false, the show save prompt will not
  // be popped up but the omnibox icon still will be shown so the user can
  // trigger the save prompt manually.
  [[nodiscard]] bool AttemptToOfferIBANLocalSave(
      const IBAN& iban_import_candidate);

  void OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIBANOfferUserDecision user_decision,
      const absl::optional<std::u16string>& nickname = absl::nullopt) {
    OnUserDidDecideOnLocalSave(user_decision, nickname);
  }

 private:
  // Called once the user makes a decision with respect to the local IBAN
  // offer-to-save-prompt. `nickname` is the nickname for the IBAN, which should
  // only be provided in the kAccepted case if the user entered a nickname.
  void OnUserDidDecideOnLocalSave(
      AutofillClient::SaveIBANOfferUserDecision user_decision,
      const absl::optional<std::u16string>& nickname = absl::nullopt);

  // Returns the IBANSaveStrikeDatabase for `client_`.
  IBANSaveStrikeDatabase* GetIBANSaveStrikeDatabase();

  // The IBAN to be saved if local IBAN save is accepted. It will be set if
  // imported IBAN is not empty.
  IBAN iban_save_candidate_;

  // True if the offer-to-save bubble should pop-up, false if not.
  bool show_save_prompt_ = false;

  // The associated autofill client. Weak reference.
  const raw_ptr<AutofillClient> client_;

  // StrikeDatabase used to check whether to offer to save the IBAN or not.
  std::unique_ptr<IBANSaveStrikeDatabase> iban_save_strike_database_;

  base::WeakPtrFactory<IBANSaveManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_SAVE_MANAGER_H_
