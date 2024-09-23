// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_SAVE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_SAVE_MANAGER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/strike_databases/payments/iban_save_strike_database.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

// The maximum number of IBANs allowed to be saved to Google Payments from
// Chrome for a single user. Created as a client-side check instead of a
// server-side one to optimize the user experience.
inline constexpr int kMaxNumServerIbans = 99;

class AutofillClient;
class PaymentsDataManager;

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
    virtual void OnOfferUploadSave() {}
    virtual void OnAcceptSaveIbanComplete() {}
    virtual void OnDeclineSaveIbanComplete() {}
    virtual void OnReceivedGetUploadDetailsResponse() {}
    virtual void OnSentUploadRequest() {}
    virtual void OnAcceptUploadSaveIbanComplete() {}
    virtual void OnAcceptUploadSaveIbanFailed() {}
  };

  // The type of save that should be offered for the IBAN candidate.
  enum class TypeOfOfferToSave {
    kDoNotOfferToSave = 0,
    kOfferServerSave = 1,
    kOfferLocalSave = 2,
    kMaxValue = kOfferLocalSave
  };

  explicit IbanSaveManager(AutofillClient* client);
  IbanSaveManager(const IbanSaveManager&) = delete;
  IbanSaveManager& operator=(const IbanSaveManager&) = delete;
  virtual ~IbanSaveManager();

  // Return the first half of hashed IBAN value.
  static std::string GetPartialIbanHashString(const std::string& value);

  // Returns true if uploading IBANs to Payments servers is enabled. This
  // requires the appropriate flags and user settings to be set.
  static bool IsIbanUploadEnabled(
      const syncer::SyncService* sync_service,
      AutofillMetrics::PaymentsSigninState signin_state_for_metrics);

  // Checks that all requirements for offering local/server IBAN save are
  // fulfilled, and if they are, offers save. Returns true if a save prompt was
  // likely shown, and false if a save prompt was definitely not shown.
  // Note that on Clank, the save prompt is *only* shown if this returns true.
  // While on desktop if this returns false, the show save prompt will not be
  // popped up but the omnibox icon still will be shown so the user can trigger
  // the save prompt manually.
  [[nodiscard]] bool AttemptToOfferSave(Iban& import_candidate);

  // TODO(b/352643261): Add TestApi for below ForTesting methods.
  void OnUserDidDecideOnLocalSaveForTesting(
      const Iban& import_candidate,
      payments::PaymentsAutofillClient::SaveIbanOfferUserDecision user_decision,
      std::u16string_view nickname = u"") {
    OnUserDidDecideOnLocalSave(import_candidate, user_decision, nickname);
  }

  void OnUserDidDecideOnUploadSaveForTesting(
      const Iban& import_candidate,
      bool show_save_prompt,
      payments::PaymentsAutofillClient::SaveIbanOfferUserDecision user_decision,
      std::u16string_view nickname = u"") {
    OnUserDidDecideOnUploadSave(import_candidate, show_save_prompt,
                                user_decision, nickname);
  }

  // Returns the IbanSaveStrikeDatabase for `client_`.
  IbanSaveStrikeDatabase* GetIbanSaveStrikeDatabaseForTesting() {
    return GetIbanSaveStrikeDatabase();
  }

  void SetEventObserverForTesting(ObserverForTest* observer) {
    observer_for_testing_ = observer;
  }

  // TODO(crbug.com/b/40937065): Iban needs to be immutable reference
  // and pass it by value in this case.
  bool AttemptToOfferLocalSaveForTesting(Iban& iban) {
    return AttemptToOfferLocalSave(iban);
  }

  bool AttemptToOfferUploadSaveForTesting(Iban& iban) {
    return AttemptToOfferUploadSave(iban);
  }

  TypeOfOfferToSave DetermineHowToSaveIbanForTesting(
      const Iban& import_candidate) {
    return DetermineHowToSaveIban(import_candidate);
  }

  void OnDidUploadIbanForTesting(
      const Iban& import_candidate,
      bool show_save_prompt,
      payments::PaymentsAutofillClient::PaymentsRpcResult result) {
    OnDidUploadIban(import_candidate, show_save_prompt, result);
  }

  bool HasContextTokenForTesting() const { return !context_token_.empty(); }

 private:
  // Sets the `record_type` of this given `import_candidate`.
  void UpdateRecordType(Iban& import_candidate);

  // Returns whether the given `import_candidate` should be offered to be saved
  // to GPay, locally, or not at all.
  TypeOfOfferToSave DetermineHowToSaveIban(const Iban& import_candidate) const;

  bool MatchesExistingLocalIban(const Iban& import_candidate) const;
  bool MatchesExistingServerIban(const Iban& import_candidate) const;

  // Returns true if the local save prompt was shown, and false otherwise.
  bool AttemptToOfferLocalSave(Iban& import_candidate);

  // Asynchronously attempts to offer an upload save prompt to the user. Will
  // fall back to a local save prompt if unable to offer server save.
  // Returns true if there will likely be a save prompt shown, and false if we
  // will definitely not be showing one.
  bool AttemptToOfferUploadSave(Iban& import_candidate);

  // Returns the IbanSaveStrikeDatabase for `client_`;
  IbanSaveStrikeDatabase* GetIbanSaveStrikeDatabase();

  // Called once the user makes a decision with respect to the local/server IBAN
  // offer-to-save-prompt. `nickname` is the nickname for the IBAN, which should
  // only be provided in the kAccepted case if the user entered a nickname.
  void OnUserDidDecideOnLocalSave(
      Iban import_candidate,
      payments::PaymentsAutofillClient::SaveIbanOfferUserDecision user_decision,
      std::u16string_view nickname = u"");
  void OnUserDidDecideOnUploadSave(
      Iban import_candidate,
      bool show_save_prompt,
      payments::PaymentsAutofillClient::SaveIbanOfferUserDecision user_decision,
      std::u16string_view nickname = u"");

  // Called when a GetIbanUploadDetails call is completed. `show_save_prompt`
  // being true implies that a save prompt is shown to the user. When false,
  // implies the offer to save will be icon-only on desktop and not shown at all
  // on mobile. The `legal_message` will be used for displaying the Terms of
  // Service and Privacy Notice within the upload-save IBAN bubble view. The
  // `validation_regex` will be used to validate that Google Payments will
  // accept the extracted IBAN value. The `context_token` will serve as the
  // token to initiate the actual Upload IBAN request. The upload flow will be
  // executed only when there is a successful result and the `legal_message` is
  // parsed successfully. In all other cases, local save will be offered if
  // applicable.
  void OnDidGetUploadDetails(
      bool show_save_prompt,
      Iban import_candidate,
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      const std::u16string& validation_regex,
      const std::u16string& context_token,
      std::unique_ptr<base::Value::Dict> legal_message);

  // Construct `UploadIbanRequestDetails` and send upload IBAN request via
  // PaymentsNetworkInterface.
  void SendUploadRequest(const Iban& import_candidate, bool show_save_prompt);

  // Called when an UploadIban call is completed.
  void OnDidUploadIban(
      const Iban& import_candidate,
      bool show_save_prompt,
      payments::PaymentsAutofillClient::PaymentsRpcResult result);

  PaymentsDataManager& payments_data_manager();
  const PaymentsDataManager& payments_data_manager() const;

  // The associated autofill client.
  const raw_ref<AutofillClient> client_;

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
