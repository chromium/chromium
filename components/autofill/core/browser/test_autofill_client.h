// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/test_address_normalizer.h"
#include "components/autofill/core/browser/test_strike_database.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/identity/public/cpp/identity_test_environment.h"

namespace autofill {

// This class is for easier writing of tests.
class TestAutofillClient : public AutofillClient {
 public:
  TestAutofillClient();
  ~TestAutofillClient() override;

  // AutofillClient implementation.
  PersonalDataManager* GetPersonalDataManager() override;
  scoped_refptr<AutofillWebDataService> GetDatabase() override;
  PrefService* GetPrefs() override;
  syncer::SyncService* GetSyncService() override;
  identity::IdentityManager* GetIdentityManager() override;
  StrikeDatabase* GetStrikeDatabase() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  AddressNormalizer* GetAddressNormalizer() override;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override;
  void ShowAutofillSettings(bool show_credit_card_settings) override;
  void ShowUnmaskPrompt(const CreditCard& card,
                        UnmaskCardReason reason,
                        base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(PaymentsRpcResult result) override;
  void ShowLocalCardMigrationDialog(
      base::OnceClosure show_migration_dialog_closure) override;
  void ConfirmMigrateLocalCardToCloud(
      std::unique_ptr<base::DictionaryValue> legal_message,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      LocalCardMigrationCallback start_migrating_cards_callback) override;
  void ConfirmSaveAutofillProfile(const AutofillProfile& profile,
                                  base::OnceClosure callback) override;
  void ConfirmSaveCreditCardLocally(const CreditCard& card,
                                    bool show_prompt,
                                    base::OnceClosure callback) override;
  void ConfirmSaveCreditCardToCloud(
      const CreditCard& card,
      std::unique_ptr<base::DictionaryValue> legal_message,
      bool should_request_name_from_user,
      bool show_prompt,
      base::OnceCallback<void(const base::string16&)> callback) override;
  void ConfirmCreditCardFillAssist(const CreditCard& card,
                                   const base::Closure& callback) override;
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;
  bool HasCreditCardScanFeature() override;
  void ScanCreditCard(const CreditCardScanCallback& callback) override;
  void ShowAutofillPopup(
      const gfx::RectF& element_bounds,
      base::i18n::TextDirection text_direction,
      const std::vector<Suggestion>& suggestions,
      bool autoselect_first_suggestion,
      base::WeakPtr<AutofillPopupDelegate> delegate) override;
  void UpdateAutofillPopupDataListValues(
      const std::vector<base::string16>& values,
      const std::vector<base::string16>& labels) override;
  void HideAutofillPopup() override;
  bool IsAutocompleteEnabled() override;
  void PropagateAutofillPredictions(
      content::RenderFrameHost* rfh,
      const std::vector<autofill::FormStructure*>& forms) override;
  void DidFillOrPreviewField(const base::string16& autofilled_value,
                             const base::string16& profile_full_name) override;
  void DidInteractWithNonsecureCreditCardInput() override;
  // By default, TestAutofillClient will report that the context is
  // secure. This can be adjusted by calling set_form_origin() with an
  // http:// URL.
  bool IsContextSecure() override;
  bool ShouldShowSigninPromo() override;
  bool AreServerCardsSupported() override;
  void ExecuteCommand(int id) override;
  // Initializes UKM source from form_origin_. This needs to be called
  // in unittests after calling Purge for ukm recorder to re-initialize
  // sources.
  void InitializeUKMSources();

  void SetPrefs(std::unique_ptr<PrefService> prefs) {
    prefs_ = std::move(prefs);
  }

  void set_test_strike_database(
      std::unique_ptr<TestStrikeDatabase> test_strike_database) {
    test_strike_database_ = std::move(test_strike_database);
  }

  void set_form_origin(const GURL& url);

  void set_sync_service(syncer::SyncService* test_sync_service) {
    test_sync_service_ = test_sync_service;
  }

  void set_security_level(security_state::SecurityLevel security_level) {
    security_level_ = security_level;
  }

  bool ConfirmSaveCardLocallyWasCalled() {
    return confirm_save_credit_card_locally_called_;
  }

  bool get_offer_to_save_credit_card_bubble_was_shown() {
    return offer_to_save_credit_card_bubble_was_shown_.value();
  }

  void set_migration_card_selections(
      const std::vector<std::string>& migration_card_selection) {
    migration_card_selection_ = migration_card_selection;
  }

  GURL form_origin() { return form_origin_; }

  static void UpdateSourceURL(ukm::UkmRecorder* ukm_recorder,
                              ukm::SourceId source_id,
                              GURL url);

 private:
  identity::IdentityTestEnvironment identity_test_env_;
  syncer::SyncService* test_sync_service_ = nullptr;
  TestAddressNormalizer test_address_normalizer_;

  // NULL by default.
  std::unique_ptr<PrefService> prefs_;
  std::unique_ptr<TestStrikeDatabase> test_strike_database_;
  GURL form_origin_;
  ukm::SourceId source_id_ = -1;

  security_state::SecurityLevel security_level_ =
      security_state::SecurityLevel::NONE;

  bool confirm_save_credit_card_locally_called_ = false;

  // Populated if save was offered. True if bubble was shown, false otherwise.
  base::Optional<bool> offer_to_save_credit_card_bubble_was_shown_;

  std::vector<std::string> migration_card_selection_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillClient);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_
