// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_client.h"

#include <utility>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/version_info/channel.h"

#if !BUILDFLAG(IS_IOS)
#include "components/autofill/core/browser/payments/test_internal_authenticator.h"
#endif

namespace autofill {

TestAutofillClient::TestAutofillClient(
    std::unique_ptr<TestPersonalDataManager> pdm)
    : test_personal_data_manager_(
          pdm ? std::move(pdm) : std::make_unique<TestPersonalDataManager>()),
      form_origin_(GURL("https://example.test")),
      last_committed_primary_main_frame_url_(GURL("https://example.test")),
      log_manager_(LogManager::Create(&log_router_, base::NullCallback())) {
  mock_iban_manager_ = std::make_unique<testing::NiceMock<MockIBANManager>>(
      test_personal_data_manager_.get());
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch("show-autofill-internals"))
    scoped_logging_subscription_.Observe(&log_router_);
}

TestAutofillClient::~TestAutofillClient() = default;

version_info::Channel TestAutofillClient::GetChannel() const {
  return channel_for_testing_;
}

TestPersonalDataManager* TestAutofillClient::GetPersonalDataManager() {
  return test_personal_data_manager_.get();
}

AutocompleteHistoryManager*
TestAutofillClient::GetAutocompleteHistoryManager() {
  return &mock_autocomplete_history_manager_;
}

IBANManager* TestAutofillClient::GetIBANManager() {
  return mock_iban_manager_.get();
}

MerchantPromoCodeManager* TestAutofillClient::GetMerchantPromoCodeManager() {
  return &mock_merchant_promo_code_manager_;
}

CreditCardCVCAuthenticator* TestAutofillClient::GetCVCAuthenticator() {
  if (!cvc_authenticator_)
    cvc_authenticator_ = std::make_unique<CreditCardCVCAuthenticator>(this);
  return cvc_authenticator_.get();
}

CreditCardOtpAuthenticator* TestAutofillClient::GetOtpAuthenticator() {
  if (!otp_authenticator_)
    otp_authenticator_ = std::make_unique<CreditCardOtpAuthenticator>(this);
  return otp_authenticator_.get();
}

PrefService* TestAutofillClient::GetPrefs() {
  return const_cast<PrefService*>(std::as_const(*this).GetPrefs());
}

const PrefService* TestAutofillClient::GetPrefs() const {
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

const GURL& TestAutofillClient::GetLastCommittedPrimaryMainFrameURL() const {
  return last_committed_primary_main_frame_url_;
}

url::Origin TestAutofillClient::GetLastCommittedPrimaryMainFrameOrigin() const {
  return url::Origin::Create(last_committed_primary_main_frame_url_);
}

security_state::SecurityLevel
TestAutofillClient::GetSecurityLevelForUmaHistograms() {
  return security_level_;
}

translate::LanguageState* TestAutofillClient::GetLanguageState() {
  return &mock_translate_driver_.GetLanguageState();
}

translate::TranslateDriver* TestAutofillClient::GetTranslateDriver() {
  return &mock_translate_driver_;
}

std::string TestAutofillClient::GetVariationConfigCountryCode() const {
  return variation_config_country_code_;
}

#if !BUILDFLAG(IS_IOS)
std::unique_ptr<webauthn::InternalAuthenticator>
TestAutofillClient::CreateCreditCardInternalAuthenticator(
    AutofillDriver* driver) {
  return std::make_unique<TestInternalAuthenticator>();
}
#endif

void TestAutofillClient::ShowAutofillSettings(bool show_credit_card_settings) {}

void TestAutofillClient::ShowUnmaskPrompt(
    const autofill::CreditCard& card,
    const autofill::CardUnmaskPromptOptions& card_unmask_prompt_options,
    base::WeakPtr<autofill::CardUnmaskDelegate> delegate) {}

void TestAutofillClient::OnUnmaskVerificationResult(PaymentsRpcResult result) {}

VirtualCardEnrollmentManager*
TestAutofillClient::GetVirtualCardEnrollmentManager() {
  return form_data_importer_->GetVirtualCardEnrollmentManager();
}

void TestAutofillClient::ShowVirtualCardEnrollDialog(
    const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
    base::OnceClosure accept_virtual_card_callback,
    base::OnceClosure decline_virtual_card_callback) {}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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
    const std::u16string& tip_message,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    MigrationDeleteCardCallback delete_local_card_callback) {}

void TestAutofillClient::ConfirmSaveIBANLocally(
    const IBAN& iban,
    bool should_show_prompt,
    LocalSaveIBANPromptCallback callback) {
  confirm_save_iban_locally_called_ = true;
  offer_to_save_iban_bubble_was_shown_ = should_show_prompt;
}

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

#else  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
void TestAutofillClient::ConfirmAccountNameFixFlow(
    base::OnceCallback<void(const std::u16string&)> callback) {
  credit_card_name_fix_flow_bubble_was_shown_ = true;
  std::move(callback).Run(std::u16string(u"Gaia Name"));
}

void TestAutofillClient::ConfirmExpirationDateFixFlow(
    const CreditCard& card,
    base::OnceCallback<void(const std::u16string&, const std::u16string&)>
        callback) {
  credit_card_name_fix_flow_bubble_was_shown_ = true;
  std::move(callback).Run(
      std::u16string(u"03"),
      std::u16string(base::ASCIIToUTF16(test::NextYear().c_str())));
}
#endif

void TestAutofillClient::ConfirmSaveCreditCardLocally(
    const CreditCard& card,
    SaveCreditCardOptions options,
    LocalSaveCardPromptCallback callback) {
  confirm_save_credit_card_locally_called_ = true;
  offer_to_save_credit_card_bubble_was_shown_ = options.show_prompt;
  save_credit_card_options_ = options;
  std::move(callback).Run(AutofillClient::SaveCardOfferUserDecision::kAccepted);
}

void TestAutofillClient::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {
  offer_to_save_credit_card_bubble_was_shown_ = options.show_prompt;
  save_credit_card_options_ = options;
  std::move(callback).Run(AutofillClient::SaveCardOfferUserDecision::kAccepted,
                          {});
}

void TestAutofillClient::CreditCardUploadCompleted(bool card_saved) {}

void TestAutofillClient::ConfirmCreditCardFillAssist(
    const CreditCard& card,
    base::OnceClosure callback) {
  std::move(callback).Run();
}

void TestAutofillClient::ConfirmSaveAddressProfile(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    SaveAddressProfilePromptOptions options,
    AddressProfileSavePromptCallback callback) {}

bool TestAutofillClient::HasCreditCardScanFeature() {
  return false;
}

void TestAutofillClient::ScanCreditCard(CreditCardScanCallback callback) {}

bool TestAutofillClient::IsFastCheckoutSupported() {
  return false;
}

bool TestAutofillClient::IsFastCheckoutTriggerForm(const FormData& form,
                                                   const FormFieldData& field) {
  return false;
}

bool TestAutofillClient::ShowFastCheckout(
    base::WeakPtr<FastCheckoutDelegate> delegate) {
  return false;
}

void TestAutofillClient::HideFastCheckout() {}

bool TestAutofillClient::IsTouchToFillCreditCardSupported() {
  return false;
}

bool TestAutofillClient::ShowTouchToFillCreditCard(
    base::WeakPtr<TouchToFillDelegate> delegate,
    base::span<const autofill::CreditCard* const> cards_to_suggest) {
  return false;
}

void TestAutofillClient::HideTouchToFillCreditCard() {}

void TestAutofillClient::ShowAutofillPopup(
    const AutofillClient::PopupOpenArgs& open_args,
    base::WeakPtr<AutofillPopupDelegate> delegate) {}

void TestAutofillClient::UpdateAutofillPopupDataListValues(
    const std::vector<std::u16string>& values,
    const std::vector<std::u16string>& labels) {}

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

void TestAutofillClient::ShowVirtualCardErrorDialog(
    const AutofillErrorDialogContext& context) {
  virtual_card_error_dialog_shown_ = true;
  autofill_error_dialog_context_ = context;
}

bool TestAutofillClient::IsAutocompleteEnabled() const {
  return true;
}

bool TestAutofillClient::IsPasswordManagerEnabled() {
  return true;
}

void TestAutofillClient::PropagateAutofillPredictions(
    AutofillDriver* driver,
    const std::vector<FormStructure*>& forms) {}

void TestAutofillClient::DidFillOrPreviewField(
    const std::u16string& autofilled_value,
    const std::u16string& profile_full_name) {}

bool TestAutofillClient::IsContextSecure() const {
  // Simplified secure context check for tests.
  return form_origin_.SchemeIs("https");
}

bool TestAutofillClient::ShouldShowSigninPromo() {
  return false;
}

bool TestAutofillClient::AreServerCardsSupported() const {
  return true;
}

void TestAutofillClient::ExecuteCommand(int id) {}

void TestAutofillClient::OpenPromoCodeOfferDetailsURL(const GURL& url) {}

LogManager* TestAutofillClient::GetLogManager() const {
  return log_manager_.get();
}

FormInteractionsFlowId TestAutofillClient::GetCurrentFormInteractionsFlowId() {
  return {};
}

void TestAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  std::move(callback).Run("some risk data");
}

#if BUILDFLAG(IS_IOS)
bool TestAutofillClient::IsLastQueriedField(FieldGlobalId field_id) {
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

void TestAutofillClient::set_last_committed_primary_main_frame_url(
    const GURL& url) {
  last_committed_primary_main_frame_url_ = url;
}

ukm::TestUkmRecorder* TestAutofillClient::GetTestUkmRecorder() {
  return &test_ukm_recorder_;
}

}  // namespace autofill
