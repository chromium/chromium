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
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_field_promo_controller.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client.h"
#include "chrome/browser/ui/hats/hats_service_desktop.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_controller.h"
#include "chrome/browser/ui/android/autofill/save_update_address_profile_flow_manager.h"
#include "components/autofill/core/browser/ui/fast_checkout_client.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_controller_impl.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_controller_impl.h"
#else
#include "chrome/browser/ui/autofill/payments/manage_migration_ui_controller.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {

class AutofillOptimizationGuide;
#if BUILDFLAG(IS_ANDROID)
class AutofillSnackbarControllerImpl;
class AutofillCvcSaveMessageDelegate;
#endif  // BUILDFLAG(IS_ANDROID)
class FormFieldData;
struct OfferNotificationOptions;
struct VirtualCardEnrollmentFields;
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
  bool IsOffTheRecord() const override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  AutofillCrowdsourcingManager* GetCrowdsourcingManager() override;
  AutofillOptimizationGuide* GetAutofillOptimizationGuide() const override;
  AutofillMlPredictionModelHandler* GetAutofillMlPredictionModelHandler()
      override;
  PersonalDataManager* GetPersonalDataManager() override;
  AutocompleteHistoryManager* GetAutocompleteHistoryManager() override;
  IbanManager* GetIbanManager() override;
  IbanAccessManager* GetIbanAccessManager() override;
  AutofillComposeDelegate* GetComposeDelegate() override;
  AutofillPlusAddressDelegate* GetPlusAddressDelegate() override;
  void OfferPlusAddressCreation(const url::Origin& main_frame_origin,
                                PlusAddressCallback callback) override;
  MerchantPromoCodeManager* GetMerchantPromoCodeManager() override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  syncer::SyncService* GetSyncService() override;
  signin::IdentityManager* GetIdentityManager() override;
  FormDataImporter* GetFormDataImporter() override;
  payments::ChromePaymentsAutofillClient* GetPaymentsAutofillClient() override;
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
  GeoIpCountryCode GetVariationConfigCountryCode() const override;
  profile_metrics::BrowserProfileType GetProfileType() const override;
  FastCheckoutClient* GetFastCheckoutClient() override;
  std::unique_ptr<webauthn::InternalAuthenticator>
  CreateCreditCardInternalAuthenticator(AutofillDriver* driver) override;
  void ShowAutofillSettings(FillingProduct main_filling_product) override;
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
#if !BUILDFLAG(IS_ANDROID)
  void HideVirtualCardEnrollBubbleAndIconIfVisible() override;
  void ShowWebauthnOfferDialog(
      WebauthnDialogCallback offer_dialog_callback) override;
  void ShowWebauthnVerifyPendingDialog(
      WebauthnDialogCallback verify_pending_dialog_callback) override;
  void UpdateWebauthnOfferDialogWithError() override;
  bool CloseWebauthnDialog() override;
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
  void ConfirmCreditCardFillAssist(const CreditCard& card,
                                   base::OnceClosure callback) override;
  void ShowEditAddressProfileDialog(
      const AutofillProfile& profile,
      AddressProfileSavePromptCallback on_user_decision_callback) override;
  void ShowDeleteAddressProfileDialog(
      const AutofillProfile& profile,
      AddressProfileDeleteDialogCallback delete_dialog_callback) override;
  void ConfirmSaveAddressProfile(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      SaveAddressProfilePromptOptions options,
      AddressProfileSavePromptCallback callback) override;
  bool HasCreditCardScanFeature() const override;
  void ScanCreditCard(CreditCardScanCallback callback) override;
  bool ShowTouchToFillCreditCard(
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const autofill::CreditCard> cards_to_suggest) override;
  bool ShowTouchToFillIban(
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const autofill::Iban> ibans_to_suggest) override;
  void HideTouchToFillCreditCard() override;
  void ShowAutofillSuggestions(
      const PopupOpenArgs& open_args,
      base::WeakPtr<AutofillSuggestionDelegate> delegate) override;
  void UpdateAutofillDataListValues(
      base::span<const SelectOption> datalist) override;
  base::span<const Suggestion> GetAutofillSuggestions() const override;
  void PinAutofillSuggestions() override;
  std::optional<PopupScreenLocation> GetPopupScreenLocation() const override;
  void UpdatePopup(const std::vector<Suggestion>& suggestions,
                   FillingProduct main_filling_product,
                   AutofillSuggestionTriggerSource trigger_source) override;
  void HideAutofillSuggestions(SuggestionHidingReason reason) override;
  void UpdateOfferNotification(
      const AutofillOfferData* offer,
      const OfferNotificationOptions& options) override;
  void DismissOfferNotification() override;
  void OnVirtualCardDataAvailable(
      const VirtualCardManualFallbackBubbleOptions& options) override;
  void TriggerUserPerceptionOfAutofillSurvey(
      FillingProduct filling_product,
      const std::map<std::string, std::string>& field_filling_stats_data)
      override;
  bool IsAutocompleteEnabled() const override;
  bool IsPasswordManagerEnabled() override;
  void DidFillOrPreviewForm(mojom::ActionPersistence action_persistence,
                            AutofillTriggerSource trigger_source,
                            bool is_refill) override;
  void DidFillOrPreviewField(const std::u16string& autofilled_value,
                             const std::u16string& profile_full_name) override;
  bool IsContextSecure() const override;
  void OpenPromoCodeOfferDetailsURL(const GURL& url) override;
  LogManager* GetLogManager() const override;
  FormInteractionsFlowId GetCurrentFormInteractionsFlowId() override;
  std::unique_ptr<device_reauth::DeviceAuthenticator> GetDeviceAuthenticator()
      override;
  void ShowAutofillFieldIphForManualFallbackFeature(
      const FormFieldData& field) override;
  void HideAutofillFieldIphForManualFallbackFeature() override;
  void NotifyAutofillManualFallbackUsed() override;

  // TODO(b/320634151): Create a test API.
  base::WeakPtr<AutofillSuggestionController>
  suggestion_controller_for_testing() {
    return suggestion_controller_;
  }
  void set_test_addresses(std::vector<AutofillProfile> test_addresses) override;
  base::span<const AutofillProfile> GetTestAddresses() const override;
#if defined(UNIT_TEST)
  void SetKeepPopupOpenForTesting(bool keep_popup_open_for_testing) {
    keep_popup_open_for_testing_ = keep_popup_open_for_testing;
    if (suggestion_controller_) {
      suggestion_controller_->SetKeepPopupOpenForTesting(
          keep_popup_open_for_testing);
    }
  }
  void SetAutofillFieldPromoControllerManualFallbackForTesting(
      std::unique_ptr<AutofillFieldPromoController> test_controller) {
    autofill_field_promo_controller_manual_fallback_ =
        std::move(test_controller);
  }
#endif  // defined(UNIT_TEST)

  // ContentAutofillClient:
  std::unique_ptr<AutofillManager> CreateManager(
      base::PassKey<ContentAutofillDriver> pass_key,
      ContentAutofillDriver& driver) override;

 protected:
  explicit ChromeAutofillClient(content::WebContents* web_contents);

 private:
  Profile* GetProfile() const;
  std::u16string GetAccountHolderName();
  bool SupportsConsentlessExecution(const url::Origin& origin);
  void ShowAutofillSuggestionsImpl(
      const PopupOpenArgs& open_args,
      base::WeakPtr<AutofillSuggestionDelegate> delegate);
  base::WeakPtr<ChromeAutofillClient> GetWeakPtr();

  std::unique_ptr<LogManager> log_manager_;

  // These members are initialized lazily in their respective getters.
  // Therefore, do not access the members directly.
  std::unique_ptr<AutofillCrowdsourcingManager> crowdsourcing_manager_;
  std::unique_ptr<payments::ChromePaymentsAutofillClient>
      payments_autofill_client_;
  std::unique_ptr<FormDataImporter> form_data_importer_;
  std::unique_ptr<payments::MandatoryReauthManager>
      payments_mandatory_reauth_manager_;
  std::unique_ptr<IbanAccessManager> iban_access_manager_;

  base::WeakPtr<AutofillSuggestionController> suggestion_controller_;
  FormInteractionsFlowId flow_id_;
  base::Time flow_id_date_;
  // If set to true, the popup will stay open regardless of external changes on
  // the test machine, that may normally cause the popup to be hidden
  bool keep_popup_open_for_testing_ = false;
#if BUILDFLAG(IS_ANDROID)
  CardExpirationDateFixFlowControllerImpl
      card_expiration_date_fix_flow_controller_;
  CardNameFixFlowControllerImpl card_name_fix_flow_controller_;
  SaveUpdateAddressProfileFlowManager save_update_address_profile_flow_manager_;
  TouchToFillPaymentMethodController touch_to_fill_payment_method_controller_{
      this};
  std::unique_ptr<AutofillSnackbarControllerImpl>
      autofill_snackbar_controller_impl_;
  std::unique_ptr<FastCheckoutClient> fast_checkout_client_;
  std::unique_ptr<AutofillCvcSaveMessageDelegate>
      autofill_cvc_save_message_delegate_;
#endif
  std::unique_ptr<AutofillFieldPromoController>
      autofill_field_promo_controller_manual_fallback_;
  // Test addresses used to allow developers to test their forms.
  std::vector<AutofillProfile> test_addresses_;
  base::WeakPtrFactory<ChromeAutofillClient> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_H_
