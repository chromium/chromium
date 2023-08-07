// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_H_
#define CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_gstatic_reader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/payments/autofill_error_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/autofill_progress_dialog_controller_impl.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_controller.h"
#include "chrome/browser/ui/android/autofill/save_update_address_profile_flow_manager.h"
#include "components/autofill/core/browser/ui/fast_checkout_client.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_controller_impl.h"
#else
#include "chrome/browser/ui/autofill/payments/manage_migration_ui_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller.h"
#include "components/zoom/zoom_observer.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {

struct AutofillErrorDialogContext;
class AutofillOptimizationGuide;
class AutofillPopupControllerImpl;
#if BUILDFLAG(IS_ANDROID)
class AutofillSaveCardBottomSheetBridge;
class AutofillSnackbarControllerImpl;
#endif  // BUILDFLAG(IS_ANDROID)
struct VirtualCardEnrollmentFields;
class VirtualCardEnrollmentManager;
struct VirtualCardManualFallbackBubbleOptions;

namespace payments {
class MandatoryReauthManager;
}  // namespace payments

// Chrome implementation of AutofillClient.
//
// ChromeAutofillClient is instantiated once per WebContents, and usages of
// main frame refer to the primary main frame because WebContents only has a
// primary main frame.
//
// Production code should not depend on ChromeAutofillClient but only on
// ContentAutofillClient. This ensures that tests can inject different
// implementations of ContentAutofillClient without causing invalid casts to
// ChromeAutofillClient.
class ChromeAutofillClient : public ContentAutofillClient,
                             public content::WebContentsObserver
#if !BUILDFLAG(IS_ANDROID)
    ,
                             public zoom::ZoomObserver
#endif  // !BUILDFLAG(IS_ANDROID)
{
 public:
  // Creates a new ChromeAutofillClient for the given `web_contents` if no
  // ContentAutofillClient is associated with the `web_contents` yet. Otherwise,
  // it's a no-op.
  static void CreateForWebContents(content::WebContents* web_contents);

  // Only tests that require ChromeAutofillClient's `*ForTesting()` functions
  // may use this function.
  //
  // Generally, code should use ContentAutofillClient::FromWebContents() if
  // possible. This is because many tests inject clients that do not inherit
  // from ChromeAutofillClient.
  static ChromeAutofillClient* FromWebContentsForTesting(
      content::WebContents* web_contents) {
    return static_cast<ChromeAutofillClient*>(FromWebContents(web_contents));
  }

  ChromeAutofillClient(const ChromeAutofillClient&) = delete;
  ChromeAutofillClient& operator=(const ChromeAutofillClient&) = delete;
  ~ChromeAutofillClient() override;

  // AutofillClient:
  version_info::Channel GetChannel() const override;
  bool IsOffTheRecord() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  AutofillDownloadManager* GetDownloadManager() override;
  AutofillOptimizationGuide* GetAutofillOptimizationGuide() const override;
  PersonalDataManager* GetPersonalDataManager() override;
  AutocompleteHistoryManager* GetAutocompleteHistoryManager() override;
  IBANManager* GetIBANManager() override;
  MerchantPromoCodeManager* GetMerchantPromoCodeManager() override;
  CreditCardCvcAuthenticator* GetCvcAuthenticator() override;
  CreditCardOtpAuthenticator* GetOtpAuthenticator() override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  syncer::SyncService* GetSyncService() override;
  signin::IdentityManager* GetIdentityManager() override;
  FormDataImporter* GetFormDataImporter() override;
  payments::PaymentsClient* GetPaymentsClient() override;
  StrikeDatabase* GetStrikeDatabase() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  AddressNormalizer* GetAddressNormalizer() override;
  AutofillOfferManager* GetAutofillOfferManager() override;
  const GURL& GetLastCommittedPrimaryMainFrameURL() const override;
  url::Origin GetLastCommittedPrimaryMainFrameOrigin() const override;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override;
  const translate::LanguageState* GetLanguageState() override;
  translate::TranslateDriver* GetTranslateDriver() override;
  std::string GetVariationConfigCountryCode() const override;
  profile_metrics::BrowserProfileType GetProfileType() const override;
  FastCheckoutClient* GetFastCheckoutClient() override;
  std::unique_ptr<webauthn::InternalAuthenticator>
  CreateCreditCardInternalAuthenticator(AutofillDriver* driver) override;

  void ShowAutofillSettings(PopupType popup_type) override;
  void ShowCardUnmaskOtpInputDialog(
      const CardUnmaskChallengeOption& challenge_option,
      base::WeakPtr<OtpUnmaskDelegate> delegate) override;
  void OnUnmaskOtpVerificationResult(OtpUnmaskResult unmask_result) override;
  void ShowUnmaskPrompt(
      const CreditCard& card,
      const CardUnmaskPromptOptions& card_unmask_prompt_options,
      base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(PaymentsRpcResult result) override;
  void ShowUnmaskAuthenticatorSelectionDialog(
      const std::vector<CardUnmaskChallengeOption>& challenge_options,
      base::OnceCallback<void(const std::string&)>
          confirm_unmask_challenge_option_callback,
      base::OnceClosure cancel_unmasking_closure) override;
  void DismissUnmaskAuthenticatorSelectionDialog(bool server_success) override;
  VirtualCardEnrollmentManager* GetVirtualCardEnrollmentManager() override;
  void ShowVirtualCardEnrollDialog(
      const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
      base::OnceClosure accept_virtual_card_callback,
      base::OnceClosure decline_virtual_card_callback) override;
  payments::MandatoryReauthManager* GetOrCreatePaymentsMandatoryReauthManager()
      override;
  void ShowMandatoryReauthOptInPrompt(
      base::OnceClosure accept_mandatory_reauth_callback,
      base::OnceClosure cancel_mandatory_reauth_callback,
      base::RepeatingClosure close_mandatory_reauth_callback) override;
  void ShowMandatoryReauthOptInConfirmation() override;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void HideVirtualCardEnrollBubbleAndIconIfVisible() override;
#endif
#if !BUILDFLAG(IS_ANDROID)
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
      const std::u16string& tip_message,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      MigrationDeleteCardCallback delete_local_card_callback) override;
  void ConfirmSaveIBANLocally(const IBAN& iban,
                              bool should_show_prompt,
                              LocalSaveIBANPromptCallback callback) override;
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
#else  // !BUILDFLAG(IS_ANDROID)
  void ConfirmAccountNameFixFlow(
      base::OnceCallback<void(const std::u16string&)> callback) override;
  void ConfirmExpirationDateFixFlow(
      const CreditCard& card,
      base::OnceCallback<void(const std::u16string&, const std::u16string&)>
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
  void ShowDeleteAddressProfileDialog() override;
  void ConfirmSaveAddressProfile(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      SaveAddressProfilePromptOptions options,
      AddressProfileSavePromptCallback callback) override;
  bool HasCreditCardScanFeature() override;
  void ScanCreditCard(CreditCardScanCallback callback) override;
  bool IsTouchToFillCreditCardSupported() override;
  bool ShowTouchToFillCreditCard(
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const autofill::CreditCard> cards_to_suggest) override;
  void HideTouchToFillCreditCard() override;
  void ShowAutofillPopup(
      const PopupOpenArgs& open_args,
      base::WeakPtr<AutofillPopupDelegate> delegate) override;
  void UpdateAutofillPopupDataListValues(
      const std::vector<std::u16string>& values,
      const std::vector<std::u16string>& labels) override;
  std::vector<Suggestion> GetPopupSuggestions() const override;
  void PinPopupView() override;
  PopupOpenArgs GetReopenPopupArgs(
      AutofillSuggestionTriggerSource trigger_source) const override;
  void UpdatePopup(const std::vector<Suggestion>& suggestions,
                   PopupType popup_type,
                   AutofillSuggestionTriggerSource trigger_source) override;
  void HideAutofillPopup(PopupHidingReason reason) override;
  void UpdateOfferNotification(const AutofillOfferData* offer,
                               bool notification_has_been_shown) override;
  void DismissOfferNotification() override;
  void OnVirtualCardDataAvailable(
      const VirtualCardManualFallbackBubbleOptions& options) override;
  void ShowVirtualCardErrorDialog(
      const AutofillErrorDialogContext& context) override;
  void ShowAutofillProgressDialog(
      AutofillProgressDialogType autofill_progress_dialog_type,
      base::OnceClosure cancel_callback) override;
  void CloseAutofillProgressDialog(
      bool show_confirmation_before_closing,
      base::OnceClosure no_interactive_authentication_callback) override;
  bool IsAutocompleteEnabled() const override;
  bool IsPasswordManagerEnabled() override;
  void PropagateAutofillPredictionsDeprecated(
      AutofillDriver* driver,
      const std::vector<FormStructure*>& forms) override;
  void DidFillOrPreviewForm(mojom::AutofillActionPersistence action_persistence,
                            AutofillTriggerSource trigger_source,
                            bool is_refill) override;
  void DidFillOrPreviewField(const std::u16string& autofilled_value,
                             const std::u16string& profile_full_name) override;
  bool IsContextSecure() const override;
  void OpenPromoCodeOfferDetailsURL(const GURL& url) override;
  LogManager* GetLogManager() const override;
  FormInteractionsFlowId GetCurrentFormInteractionsFlowId() override;
  scoped_refptr<device_reauth::DeviceAuthenticator> GetDeviceAuthenticator()
      const override;

  // RiskDataLoader:
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;

  // content::WebContentsObserver implementation.
  void PrimaryMainFrameWasResized(bool width_changed) override;
  void WebContentsDestroyed() override;
  void OnWebContentsLostFocus(
      content::RenderWidgetHost* render_widget_host) override;
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;

  base::WeakPtr<AutofillPopupControllerImpl> popup_controller_for_testing() {
    return popup_controller_;
  }
  void KeepPopupOpenForTesting() { keep_popup_open_for_testing_ = true; }
  std::unique_ptr<CardUnmaskPromptControllerImpl>
  SetCardUnmaskControllerForTesting(
      std::unique_ptr<CardUnmaskPromptControllerImpl> test_controller) {
    return std::exchange(unmask_controller_, std::move(test_controller));
  }

#if !BUILDFLAG(IS_ANDROID)
  // ZoomObserver:
  void OnZoomControllerDestroyed(zoom::ZoomController* source) override;
  void OnZoomChanged(
      const zoom::ZoomController::ZoomChangedEventData& data) override;
#endif

  AutofillProgressDialogControllerImpl*
  AutofillProgressDialogControllerForTesting() {
    return autofill_progress_dialog_controller_.get();
  }

 protected:
  explicit ChromeAutofillClient(content::WebContents* web_contents);
#if BUILDFLAG(IS_ANDROID)
  void SetAutofillSaveCardBottomSheetBridgeForTesting(
      std::unique_ptr<AutofillSaveCardBottomSheetBridge>
          autofill_save_card_bottom_sheet_bridge);
#endif

 private:
  Profile* GetProfile() const;
  bool IsMultipleAccountUser();
  std::u16string GetAccountHolderName();
  std::u16string GetAccountHolderEmail();
  bool SupportsConsentlessExecution(const url::Origin& origin);

  std::unique_ptr<LogManager> log_manager_;

  // These members are initialized lazily in their respective getters.
  // Therefore, do not access the members directly.
  std::unique_ptr<AutofillDownloadManager> download_manager_;
  std::unique_ptr<payments::PaymentsClient> payments_client_;
  std::unique_ptr<CreditCardCvcAuthenticator> cvc_authenticator_;
  std::unique_ptr<CreditCardOtpAuthenticator> otp_authenticator_;
  std::unique_ptr<FormDataImporter> form_data_importer_;
  std::unique_ptr<payments::MandatoryReauthManager>
      payments_mandatory_reauth_manager_;

  base::WeakPtr<AutofillPopupControllerImpl> popup_controller_;
  FormInteractionsFlowId flow_id_{};
  base::Time flow_id_date_;
  // If set to true, the popup will stay open regardless of external changes on
  // the test machine, that may normally cause the popup to be hidden
  bool keep_popup_open_for_testing_ = false;
#if BUILDFLAG(IS_ANDROID)
  CardExpirationDateFixFlowControllerImpl
      card_expiration_date_fix_flow_controller_;
  CardNameFixFlowControllerImpl card_name_fix_flow_controller_;
  SaveUpdateAddressProfileFlowManager save_update_address_profile_flow_manager_;
  TouchToFillCreditCardController touch_to_fill_credit_card_controller_{this};
  std::unique_ptr<AutofillSnackbarControllerImpl>
      autofill_snackbar_controller_impl_;
  std::unique_ptr<FastCheckoutClient> fast_checkout_client_;
  std::unique_ptr<AutofillSaveCardBottomSheetBridge>
      autofill_save_card_bottom_sheet_bridge_;
#endif
  std::unique_ptr<CardUnmaskPromptControllerImpl> unmask_controller_;
  AutofillErrorDialogControllerImpl autofill_error_dialog_controller_;
  std::unique_ptr<AutofillProgressDialogControllerImpl>
      autofill_progress_dialog_controller_;

#if !BUILDFLAG(IS_ANDROID)
  base::ScopedObservation<zoom::ZoomController, zoom::ZoomObserver>
      zoom_observation_{this};
#endif

  // True if and only if the associated web_contents() is currently focused.
  bool has_focus_ = false;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_H_
