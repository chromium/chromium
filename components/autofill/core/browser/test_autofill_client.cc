// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_client.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"

#if !defined(OS_IOS)
#include "components/autofill/core/browser/payments/test_internal_authenticator.h"
#endif

namespace autofill {

TestAutofillClient::TestAutofillClient()
    : form_origin_(GURL("https://example.test")),
      last_committed_url_(GURL("https://example.test")) {}

TestAutofillClient::~TestAutofillClient() {}

PersonalDataManager* TestAutofillClient::GetPersonalDataManager() {
  return &test_personal_data_manager_;
}

AutocompleteHistoryManager*
TestAutofillClient::GetAutocompleteHistoryManager() {
  return &mock_autocomplete_history_manager_;
}

PrefService* TestAutofillClient::GetPrefs() {
  return prefs_.get();
}

syncer::SyncService* TestAutofillClient::GetSyncService() {
  return test_sync_service_;
}

signin::IdentityManager* TestAutofillClient::GetIdentityManager() {
  return identity_test_env_.identity_manager();
}

FormDataImporter* TestAutofillClient::GetFormDataImporter() {
  return form_data_importer_.get();
}

payments::PaymentsClient* TestAutofillClient::GetPaymentsClient() {
  return payments_client_.get();
}

StrikeDatabase* TestAutofillClient::GetStrikeDatabase() {
  return test_strike_database_.get();
}

ukm::UkmRecorder* TestAutofillClient::GetUkmRecorder() {
  return &test_ukm_recorder_;
}

ukm::SourceId TestAutofillClient::GetUkmSourceId() {
  if (source_id_ == -1) {
    source_id_ = ukm::UkmRecorder::GetNewSourceID();
    test_ukm_recorder_.UpdateSourceURL(source_id_, form_origin_);
  }
  return source_id_;
}

AddressNormalizer* TestAutofillClient::GetAddressNormalizer() {
  return &test_address_normalizer_;
}

AutofillOfferManager* TestAutofillClient::GetAutofillOfferManager() {
  return autofill_offer_manager_.get();
}

const GURL& TestAutofillClient::GetLastCommittedURL() {
  return last_committed_url_;
}

security_state::SecurityLevel
TestAutofillClient::GetSecurityLevelForUmaHistograms() {
  return security_level_;
}

translate::LanguageState* TestAutofillClient::GetLanguageState() {
  return &mock_translate_driver_.GetLanguageState();
}

#if !defined(OS_IOS)
std::unique_ptr<InternalAuthenticator>
TestAutofillClient::CreateCreditCardInternalAuthenticator(
    content::RenderFrameHost* rfh) {
  return std::make_unique<TestInternalAuthenticator>();
}
#endif

void TestAutofillClient::ShowAutofillSettings(bool show_credit_card_settings) {}

void TestAutofillClient::ShowUnmaskPrompt(
    const CreditCard& card,
    UnmaskCardReason reason,
    base::WeakPtr<CardUnmaskDelegate> delegate) {}

void TestAutofillClient::OnUnmaskVerificationResult(PaymentsRpcResult result) {}

#if !defined(OS_ANDROID) && !defined(OS_IOS)
std::vector<std::string>
TestAutofillClient::GetAllowedMerchantsForVirtualCards() {
  return allowed_merchants_;
}

std::vector<std::string>
TestAutofillClient::GetAllowedBinRangesForVirtualCards() {
  return allowed_bin_ranges_;
}

void TestAutofillClient::ShowLocalCardMigrationDialog(
    base::OnceClosure show_migration_dialog_closure) {
  std::move(show_migration_dialog_closure).Run();
}

void TestAutofillClient::ConfirmMigrateLocalCardToCloud(
    const LegalMessageLines& legal_message_lines,
    const std::string& user_email,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    LocalCardMigrationCallback start_migrating_cards_callback) {
  // If |migration_card_selection_| hasn't been preset by tests, default to
  // selecting all migratable cards.
  if (migration_card_selection_.empty()) {
    for (MigratableCreditCard card : migratable_credit_cards)
      migration_card_selection_.push_back(card.credit_card().guid());
  }
  std::move(start_migrating_cards_callback).Run(migration_card_selection_);
}

void TestAutofillClient::ShowLocalCardMigrationResults(
    const bool has_server_error,
    const base::string16& tip_message,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    MigrationDeleteCardCallback delete_local_card_callback) {}
void TestAutofillClient::ShowWebauthnOfferDialog(
    WebauthnDialogCallback offer_dialog_callback) {}

void TestAutofillClient::ShowWebauthnVerifyPendingDialog(
    WebauthnDialogCallback verify_pending_dialog_callback) {}

void TestAutofillClient::UpdateWebauthnOfferDialogWithError() {}

bool TestAutofillClient::CloseWebauthnDialog() {
  return true;
}

void TestAutofillClient::ConfirmSaveUpiIdLocally(
    const std::string& upi_id,
    base::OnceCallback<void(bool accept)> callback) {}

void TestAutofillClient::OfferVirtualCardOptions(
    const std::vector<CreditCard*>& candidates,
    base::OnceCallback<void(const std::string&)> callback) {}

#else  // defined(OS_ANDROID) || defined(OS_IOS)
void TestAutofillClient::ConfirmAccountNameFixFlow(
    base::OnceCallback<void(const base::string16&)> callback) {
  credit_card_name_fix_flow_bubble_was_shown_ = true;
  std::move(callback).Run(base::string16(base::ASCIIToUTF16("Gaia Name")));
}

void TestAutofillClient::ConfirmExpirationDateFixFlow(
    const CreditCard& card,
    base::OnceCallback<void(const base::string16&, const base::string16&)>
        callback) {
  credit_card_name_fix_flow_bubble_was_shown_ = true;
  std::move(callback).Run(
      base::string16(base::ASCIIToUTF16("03")),
      base::string16(base::ASCIIToUTF16(test::NextYear().c_str())));
}
#endif

void TestAutofillClient::ConfirmSaveCreditCardLocally(
    const CreditCard& card,
    SaveCreditCardOptions options,
    LocalSaveCardPromptCallback callback) {
  confirm_save_credit_card_locally_called_ = true;
  offer_to_save_credit_card_bubble_was_shown_ = options.show_prompt;
  save_credit_card_options_ = options;
  std::move(callback).Run(AutofillClient::ACCEPTED);
}

void TestAutofillClient::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {
  offer_to_save_credit_card_bubble_was_shown_ = options.show_prompt;
  save_credit_card_options_ = options;
  std::move(callback).Run(AutofillClient::ACCEPTED, {});
}

void TestAutofillClient::CreditCardUploadCompleted(bool card_saved) {}

void TestAutofillClient::ConfirmCreditCardFillAssist(
    const CreditCard& card,
    base::OnceClosure callback) {
  std::move(callback).Run();
}

bool TestAutofillClient::HasCreditCardScanFeature() {
  return false;
}

void TestAutofillClient::ScanCreditCard(CreditCardScanCallback callback) {}

void TestAutofillClient::ShowAutofillPopup(
    const AutofillClient::PopupOpenArgs& open_args,
    base::WeakPtr<AutofillPopupDelegate> delegate) {}

void TestAutofillClient::UpdateAutofillPopupDataListValues(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {}

base::span<const Suggestion> TestAutofillClient::GetPopupSuggestions() const {
  return base::span<const Suggestion>();
}

void TestAutofillClient::PinPopupView() {}

AutofillClient::PopupOpenArgs TestAutofillClient::GetReopenPopupArgs() const {
  return {};
}

void TestAutofillClient::UpdatePopup(const std::vector<Suggestion>& suggestions,
                                     PopupType popup_type) {}

void TestAutofillClient::HideAutofillPopup(PopupHidingReason reason) {}

bool TestAutofillClient::IsAutocompleteEnabled() {
  return true;
}

void TestAutofillClient::PropagateAutofillPredictions(
    content::RenderFrameHost* rfh,
    const std::vector<FormStructure*>& forms) {}

void TestAutofillClient::DidFillOrPreviewField(
    const base::string16& autofilled_value,
    const base::string16& profile_full_name) {}

bool TestAutofillClient::IsContextSecure() {
  // Simplified secure context check for tests.
  return form_origin_.SchemeIs("https");
}

bool TestAutofillClient::ShouldShowSigninPromo() {
  return false;
}

bool TestAutofillClient::AreServerCardsSupported() {
  return true;
}

void TestAutofillClient::ExecuteCommand(int id) {}

void TestAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  std::move(callback).Run("some risk data");
}

#if defined(OS_IOS)
bool TestAutofillClient::IsQueryIDRelevant(int query_id) {
  return true;
}
#endif

void TestAutofillClient::InitializeUKMSources() {
  test_ukm_recorder_.UpdateSourceURL(source_id_, form_origin_);
}

void TestAutofillClient::set_form_origin(const GURL& url) {
  form_origin_ = url;
  // Also reset source_id_.
  source_id_ = ukm::UkmRecorder::GetNewSourceID();
  test_ukm_recorder_.UpdateSourceURL(source_id_, form_origin_);
}

ukm::TestUkmRecorder* TestAutofillClient::GetTestUkmRecorder() {
  return &test_ukm_recorder_;
}

}  // namespace autofill
