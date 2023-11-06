// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_SAVE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_SAVE_MANAGER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/strike_databases/payments/iban_save_strike_database.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

// Decides whether an IBAN local save should be offered and handles the workflow
// for local saves.
class IbanSaveManager {
 public:
  // An observer class used by browsertests that gets notified whenever
  // particular actions occur.
  class ObserverForTest {
   public:
    virtual ~ObserverForTest() = default;
    virtual void OnOfferLocalSave() {}
    virtual void OnAcceptSaveIbanComplete() {}
    virtual void OnDeclineSaveIbanComplete() {}
  };

  IbanSaveManager(PersonalDataManager* personal_data_manager,
                  AutofillClient* client);
  IbanSaveManager(const IbanSaveManager&) = delete;
  IbanSaveManager& operator=(const IbanSaveManager&) = delete;
  virtual ~IbanSaveManager();

  // Return the first half of hashed IBAN value.
  static std::string GetPartialIbanHashString(const std::string& value);

  // Returns true if uploading IBANs to Payments servers is enabled. This
  // requires the appropriate flags and user settings to be set.
  static bool IsIbanUploadEnabled(const syncer::SyncService* sync_service);

  // Checks that all requirements for offering local IBAN save are fulfilled.
  // Returns true if the save prompt was shown, and false otherwise.
  // Note that on desktop if this returns false, the show save prompt will not
  // be popped up but the omnibox icon still will be shown so the user can
  // trigger the save prompt manually.
  // TODO(b/296651801): Refactor to make only an `AttemptToOfferIbanSave`
  // method public, so that FormDataImporter remains unaware of the internal
  // logic managed by this class. Once possible, add `[[nodiscard]]` to the
  // method exposed to FormDataImporter.
  bool AttemptToOfferIbanLocalSave(const Iban& iban_import_candidate);

  void OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIbanOfferUserDecision user_decision,
      std::optional<std::u16string> nickname = absl::nullopt) {
    OnUserDidDecideOnLocalSave(user_decision, nickname);
  }

  // Returns the IbanSaveStrikeDatabase for `client_`.
  IbanSaveStrikeDatabase* GetIbanSaveStrikeDatabaseForTesting() {
    return GetIbanSaveStrikeDatabase();
  }

  void SetEventObserverForTesting(ObserverForTest* observer) {
    observer_for_testing_ = observer;
  }

  bool ShouldOfferUploadSaveForTesting(
      const Iban& iban_import_candidate) const {
    return ShouldOfferUploadSave(iban_import_candidate);
  }

  bool OfferUploadSaveForTesting(const Iban& iban) {
    return OfferUploadSave(iban);
  }

  bool HasContextTokenForTesting() const { return !context_token_.empty(); }

 private:
  // Returns true if local save should be offered for the
  // `iban_import_candidate`.
  bool ShouldOfferLocalSave(const Iban& iban_import_candidate) const;

  // Returns true if upload save should be offered for the
  // `iban_import_candidate`.
  bool ShouldOfferUploadSave(const Iban& iban_import_candidate) const;

  // Asynchronously attempts to offer an upload save prompt to the user. Will
  // fall back to a local save prompt if unable to offer server save.
  // Returns true if there will likely be a save prompt shown, and false if we
  // will definitely not be showing one.
  bool OfferUploadSave(const Iban& import_candidate);

  // Returns the IbanSaveStrikeDatabase for `client_`;
  IbanSaveStrikeDatabase* GetIbanSaveStrikeDatabase();

  // Called once the user makes a decision with respect to the local IBAN
  // offer-to-save-prompt. `nickname` is the nickname for the IBAN, which should
  // only be provided in the kAccepted case if the user entered a nickname.
  void OnUserDidDecideOnLocalSave(
      AutofillClient::SaveIbanOfferUserDecision user_decision,
      std::optional<std::u16string> nickname = absl::nullopt);

  // Called when a GetIbanUploadDetails call is completed. The
  // `legal_message` will be used for displaying the Terms of Service and
  // Privacy Notice within the upload-save IBAN bubble view. The `context_token`
  // will serve as the token to initiate the actual Upload IBAN request.
  // The upload flow will be executed only when there is a successful result and
  // the `legal_message` is parsed successfully. In all other cases, local save
  // will be offered if applicable.
  void OnDidGetUploadDetails(AutofillClient::PaymentsRpcResult result,
                             const std::u16string& context_token,
                             std::unique_ptr<base::Value::Dict> legal_message);

  // The IBAN to be saved if local IBAN save is accepted. It will be set if
  // imported IBAN is not empty. The record type of this IBAN candidate is
  // initially set to `kUnknown`.
  Iban iban_save_candidate_;

  // The personal data manager, used to save and load personal data to/from the
  // web database.
  const raw_ptr<PersonalDataManager> personal_data_manager_;

  // The associated autofill client.
  const raw_ptr<AutofillClient> client_;

  // StrikeDatabase used to check whether to offer to save the IBAN or not.
  std::unique_ptr<IbanSaveStrikeDatabase> iban_save_strike_database_;

  // The context token returned from GetIbanUploadDetails.
  std::u16string context_token_;

  // May be null.
  raw_ptr<ObserverForTest> observer_for_testing_ = nullptr;

  base::WeakPtrFactory<IbanSaveManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_SAVE_MANAGER_H_
