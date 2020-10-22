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
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/mock_autocomplete_history_manager.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/payments/test_strike_database.h"
#include "components/autofill/core/browser/test_address_normalizer.h"
#include "components/autofill/core/browser/test_form_data_importer.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"

#if !defined(OS_IOS)
#include "components/autofill/core/browser/payments/internal_authenticator.h"
#endif

namespace autofill {

// This class is for easier writing of tests.
class TestAutofillClient : public AutofillClient {
 public:
  TestAutofillClient();
  ~TestAutofillClient() override;

  // AutofillClient:
  PersonalDataManager* GetPersonalDataManager() override;
  AutocompleteHistoryManager* GetAutocompleteHistoryManager() override;
  PrefService* GetPrefs() override;
  syncer::SyncService* GetSyncService() override;
  signin::IdentityManager* GetIdentityManager() override;
  FormDataImporter* GetFormDataImporter() override;
  payments::PaymentsClient* GetPaymentsClient() override;
  StrikeDatabase* GetStrikeDatabase() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  AddressNormalizer* GetAddressNormalizer() override;
  AutofillOfferManager* GetAutofillOfferManager() override;
  const GURL& GetLastCommittedURL() override;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override;
  translate::LanguageState* GetLanguageState() override;
#if !defined(OS_IOS)
  std::unique_ptr<InternalAuthenticator> CreateCreditCardInternalAuthenticator(
      content::RenderFrameHost* rfh) override;
#endif

  void ShowAutofillSettings(bool show_credit_card_settings) override;
  void ShowUnmaskPrompt(const CreditCard& card,
                        UnmaskCardReason reason,
                        base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(PaymentsRpcResult result) override;

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  std::vector<std::string> GetAllowedMerchantsForVirtualCards() override;
  std::vector<std::string> GetAllowedBinRangesForVirtualCards() override;

  void ShowLocalCardMigrationDialog(
      base::OnceClosure show_migration_dialog_closure) override;
  void ConfirmMigrateLocalCardToCloud(
      const LegalMessageLines& legal_message_lines,
      const std::string& user_email,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      LocalCardMigrationCallback start_migrating_cards_callback) override;
  void ShowLocalCardMigrationResults(
      const bool has_server_error,
      const base::string16& tip_message,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      MigrationDeleteCardCallback delete_local_card_callback) override;
  void ShowWebauthnOfferDialog(
      WebauthnDialogCallback offer_dialog_callback) override;
  void ShowWebauthnVerifyPendingDialog(
      WebauthnDialogCallback verify_pending_dialog_callback) override;
  void UpdateWebauthnOfferDialogWithError() override;
  bool CloseWebauthnDialog() override;
  void ConfirmSaveUpiIdLocally(
      const std::string& upi_id,
      base::OnceCallback<void(bool accept)> callback) override;
  void OfferVirtualCardOptions(
      const std::vector<CreditCard*>& candidates,
      base::OnceCallback<void(const std::string&)> callback) override;
#else  // defined(OS_ANDROID) || defined(OS_IOS)
  void ConfirmAccountNameFixFlow(
      base::OnceCallback<void(const base::string16&)> callback) override;
  void ConfirmExpirationDateFixFlow(
      const CreditCard& card,
      base::OnceCallback<void(const base::string16&, const base::string16&)>
          callback) override;
#endif
  void ConfirmSaveCreditCardLocally(
      const CreditCard& card,
      SaveCreditCardOptions options,
      LocalSaveCardPromptCallback callback) override;

  void ConfirmSaveCreditCardToCloud(
      const CreditCard& card,
      const LegalMessageLines& legal_message_lines,
      SaveCreditCardOptions options,
      UploadSaveCardPromptCallback callback) override;
  void CreditCardUploadCompleted(bool card_saved) override;
  void ConfirmCreditCardFillAssist(const CreditCard& card,
                                   base::OnceClosure callback) override;
  bool HasCreditCardScanFeature() override;
  void ScanCreditCard(CreditCardScanCallback callback) override;
  void ShowAutofillPopup(
      const AutofillClient::PopupOpenArgs& open_args,
      base::WeakPtr<AutofillPopupDelegate> delegate) override;
  void UpdateAutofillPopupDataListValues(
      const std::vector<base::string16>& values,
      const std::vector<base::string16>& labels) override;
  base::span<const Suggestion> GetPopupSuggestions() const override;
  void PinPopupView() override;
  AutofillClient::PopupOpenArgs GetReopenPopupArgs() const override;
  void UpdatePopup(const std::vector<Suggestion>& suggestions,
                   PopupType popup_type) override;
  void HideAutofillPopup(PopupHidingReason reason) override;
  bool IsAutocompleteEnabled() override;
  void PropagateAutofillPredictions(
      content::RenderFrameHost* rfh,
      const std::vector<FormStructure*>& forms) override;
  void DidFillOrPreviewField(const base::string16& autofilled_value,
                             const base::string16& profile_full_name) override;
  // By default, TestAutofillClient will report that the context is
  // secure. This can be adjusted by calling set_form_origin() with an
  // http:// URL.
  bool IsContextSecure() override;
  bool ShouldShowSigninPromo() override;
  bool AreServerCardsSupported() override;
  void ExecuteCommand(int id) override;

  // RiskDataLoader:
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;

#if defined(OS_IOS)
  bool IsQueryIDRelevant(int query_id) override;
#endif

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

  void set_test_payments_client(
      std::unique_ptr<payments::TestPaymentsClient> payments_client) {
    payments_client_ = std::move(payments_client);
  }

  void set_test_form_data_importer(
      std::unique_ptr<TestFormDataImporter> form_data_importer) {
    form_data_importer_ = std::move(form_data_importer);
  }

  void set_form_origin(const GURL& url);

  void set_sync_service(syncer::SyncService* test_sync_service) {
    test_sync_service_ = test_sync_service;
  }

  void set_security_level(security_state::SecurityLevel security_level) {
    security_level_ = security_level;
  }

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  void set_allowed_merchants(
      const std::vector<std::string>& merchant_allowlist) {
    allowed_merchants_ = merchant_allowlist;
  }

  void set_allowed_bin_ranges(
      const std::vector<std::string>& bin_range_allowlist) {
    allowed_bin_ranges_ = bin_range_allowlist;
  }
#endif

  void set_should_save_autofill_profiles(bool value) {
    should_save_autofill_profiles_ = value;
  }

  bool ConfirmSaveCardLocallyWasCalled() {
    return confirm_save_credit_card_locally_called_;
  }

  bool get_offer_to_save_credit_card_bubble_was_shown() {
    return offer_to_save_credit_card_bubble_was_shown_.value();
  }

  SaveCreditCardOptions get_save_credit_card_options() {
    return save_credit_card_options_.value();
  }

  MockAutocompleteHistoryManager* GetMockAutocompleteHistoryManager() {
    return &mock_autocomplete_history_manager_;
  }

  void set_migration_card_selections(
      const std::vector<std::string>& migration_card_selection) {
    migration_card_selection_ = migration_card_selection;
  }

  void set_autofill_offer_manager(
      std::unique_ptr<AutofillOfferManager> autofill_offer_manager) {
    autofill_offer_manager_ = std::move(autofill_offer_manager);
  }

  GURL form_origin() { return form_origin_; }

  ukm::TestUkmRecorder* GetTestUkmRecorder();

 private:
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  signin::IdentityTestEnvironment identity_test_env_;
  syncer::SyncService* test_sync_service_ = nullptr;
  TestAddressNormalizer test_address_normalizer_;
  TestPersonalDataManager test_personal_data_manager_;
  MockAutocompleteHistoryManager mock_autocomplete_history_manager_;
  std::unique_ptr<AutofillOfferManager> autofill_offer_manager_;

  // NULL by default.
  std::unique_ptr<PrefService> prefs_;
  std::unique_ptr<TestStrikeDatabase> test_strike_database_;
  std::unique_ptr<payments::PaymentsClient> payments_client_;
  std::unique_ptr<FormDataImporter> form_data_importer_;
  GURL form_origin_;
  ukm::SourceId source_id_ = -1;

  security_state::SecurityLevel security_level_ =
      security_state::SecurityLevel::NONE;

  bool should_save_autofill_profiles_ = true;

  bool confirm_save_credit_card_locally_called_ = false;

  // Populated if save was offered. True if bubble was shown, false otherwise.
  base::Optional<bool> offer_to_save_credit_card_bubble_was_shown_;

  // Populated if name fix flow was offered. True if bubble was shown, false
  // otherwise.
  base::Optional<bool> credit_card_name_fix_flow_bubble_was_shown_;

  // Populated if local save or upload was offered.
  base::Optional<SaveCreditCardOptions> save_credit_card_options_;

  std::vector<std::string> migration_card_selection_;

  // A mock translate driver which provides the language state.
  translate::testing::MockTranslateDriver mock_translate_driver_;

  // The last URL submitted by the user in the URL bar. Set in the constructor.
  GURL last_committed_url_;

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  std::vector<std::string> allowed_merchants_;
  std::vector<std::string> allowed_bin_ranges_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TestAutofillClient);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_CLIENT_H_
