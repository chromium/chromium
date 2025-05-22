// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_H_
#define CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_H_

#include <memory>
#include <optional>
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
#include "components/autofill/content/browser/autofill_log_router_factory.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_identity_credential_delegate.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/crowdsourcing/votes_uploader.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/integrators/identity_credential/identity_credential_delegate.h"
#include "components/autofill/core/browser/integrators/password_form_classification.h"
#include "components/autofill/core/browser/integrators/plus_addresses/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/browser/single_field_fillers/single_field_fill_router.h"
#include "components/autofill/core/browser/studies/autofill_ablation_study.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/autofill/autofill_snackbar_controller_impl.h"
#include "components/autofill/core/browser/integrators/fast_checkout/fast_checkout_client.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {

#if BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/364089352): When //c/b/ui/android/autofill gets modularized,
// //c/b/ui/autofill/ can depend directly on it. Now, forward declare the
// SaveUpdateAddressProfileFlowManager.
class SaveUpdateAddressProfileFlowManager;
#endif

class AutofillOptimizationGuide;
class AutofillAiDelegate;
class FormFieldData;
class LogRouter;
enum class SuggestionType;

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
//
// BEWARE OF SUBCLASSING in tests: virtual function calls during construction
// may lead to very surprising behavior. The class is not `final` because a few
// tests derive from it. Member functions should be final unless they need to be
// mocked or overridden in subclasses and you have verified that they are not
// called, directly or indirectly, from the constructor.
class ChromeAutofillClient : public ContentAutofillClient,
                             public content::WebContentsObserver {
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
  base::WeakPtr<AutofillClient> GetWeakPtr() final;
  const std::string& GetAppLocale() const final;
  version_info::Channel GetChannel() const final;
  bool IsOffTheRecord() const final;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() final;
  AutofillCrowdsourcingManager& GetCrowdsourcingManager() final;
  VotesUploader& GetVotesUploader() final;
  AutofillOptimizationGuide* GetAutofillOptimizationGuide() const final;
  FieldClassificationModelHandler* GetAutofillFieldClassificationModelHandler()
      final;
  FieldClassificationModelHandler*
  GetPasswordManagerFieldClassificationModelHandler() final;
  PersonalDataManager& GetPersonalDataManager() final;
  ValuablesDataManager* GetValuablesDataManager() final;
  EntityDataManager* GetEntityDataManager() final;
  SingleFieldFillRouter& GetSingleFieldFillRouter() final;
  AutocompleteHistoryManager* GetAutocompleteHistoryManager() final;
  AutofillComposeDelegate* GetComposeDelegate() final;
  AutofillPlusAddressDelegate* GetPlusAddressDelegate() final;
  PasswordManagerDelegate* GetPasswordManagerDelegate(
      const FieldGlobalId& field_id) final;
  void GetAiPageContent(GetAiPageContentCallback callback) final;
  AutofillAiDelegate* GetAutofillAiDelegate() final;
  AutofillAiModelCache* GetAutofillAiModelCache() final;
  AutofillAiModelExecutor* GetAutofillAiModelExecutor() final;
  IdentityCredentialDelegate* GetIdentityCredentialDelegate() final;
  void OfferPlusAddressCreation(const url::Origin& main_frame_origin,
                                bool is_manual_fallback,
                                PlusAddressCallback callback) final;
  void ShowPlusAddressError(PlusAddressErrorDialogType error_dialog_type,
                            base::OnceClosure on_accepted) final;
  void ShowPlusAddressAffiliationError(std::u16string affiliated_domain,
                                       std::u16string affiliated_plus_address,
                                       base::OnceClosure on_accepted) final;
  PrefService* GetPrefs() final;
  const PrefService* GetPrefs() const final;
  syncer::SyncService* GetSyncService() final;
  signin::IdentityManager* GetIdentityManager() final;
  const signin::IdentityManager* GetIdentityManager() const final;
  const GoogleGroupsManager* GetGoogleGroupsManager() const final;
  FormDataImporter* GetFormDataImporter() final;
  payments::ChromePaymentsAutofillClient* GetPaymentsAutofillClient() final;
  StrikeDatabase* GetStrikeDatabase() final;
  ukm::UkmRecorder* GetUkmRecorder() final;
  AddressNormalizer* GetAddressNormalizer() final;
  const GURL& GetLastCommittedPrimaryMainFrameURL() const final;
  url::Origin GetLastCommittedPrimaryMainFrameOrigin() const final;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() final;
  const translate::LanguageState* GetLanguageState() final;
  translate::TranslateDriver* GetTranslateDriver() final;
  GeoIpCountryCode GetVariationConfigCountryCode() const final;
  profile_metrics::BrowserProfileType GetProfileType() const final;
  FastCheckoutClient* GetFastCheckoutClient() final;
  void ShowAutofillSettings(SuggestionType suggestion_type) final;
  void ConfirmSaveAddressProfile(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      bool is_migration_to_account,
      AddressProfileSavePromptCallback callback) final;
  // Not called during construction -- safe to override in tests.
  SuggestionUiSessionId ShowAutofillSuggestions(
      const PopupOpenArgs& open_args,
      base::WeakPtr<AutofillSuggestionDelegate> delegate) override;
  void ShowPlusAddressEmailOverrideNotification(
      const std::string& original_email,
      EmailOverrideUndoCallback email_final) final;
  void UpdateAutofillDataListValues(
      base::span<const SelectOption> datalist) final;
  base::span<const Suggestion> GetAutofillSuggestions() const final;
  std::optional<PopupScreenLocation> GetPopupScreenLocation() const final;
  std::optional<SuggestionUiSessionId>
  GetSessionIdForCurrentAutofillSuggestions() const final;
  void UpdateAutofillSuggestions(
      const std::vector<Suggestion>& suggestions,
      FillingProduct main_filling_product,
      AutofillSuggestionTriggerSource trigger_source) final;
  void HideAutofillSuggestions(SuggestionHidingReason reason) final;
  void TriggerUserPerceptionOfAutofillSurvey(
      FillingProduct filling_product,
      const std::map<std::string, std::string>& field_filling_stats_data) final;
  bool IsAutofillEnabled() const final;
  bool IsAutofillProfileEnabled() const final;
  bool IsAutofillPaymentMethodsEnabled() const final;
  bool IsAutocompleteEnabled() const final;
  bool IsPasswordManagerEnabled() const final;
  void DidFillForm(AutofillTriggerSource trigger_source, bool is_refill) final;
  bool IsContextSecure() const final;
  LogManager* GetCurrentLogManager() final;
  autofill_metrics::FormInteractionsUkmLogger& GetFormInteractionsUkmLogger()
      final;

  const AutofillAblationStudy& GetAblationStudy() const final;
#if BUILDFLAG(IS_ANDROID)
  // The AutofillSnackbarController is used to show a snackbar notification
  // on Android.
  AutofillSnackbarControllerImpl* GetAutofillSnackbarController() final;
#endif
  FormInteractionsFlowId GetCurrentFormInteractionsFlowId() final;
  std::unique_ptr<device_reauth::DeviceAuthenticator> GetDeviceAuthenticator()
      final;
  bool ShowAutofillFieldIphForFeature(const FormFieldData& field,
                                      AutofillClient::IphFeature feature) final;
  void HideAutofillFieldIph() final;
  void NotifyIphFeatureUsed(AutofillClient::IphFeature feature) final;
  void set_test_addresses(std::vector<AutofillProfile> test_addresses) final;
  base::span<const AutofillProfile> GetTestAddresses() const final;
  PasswordFormClassification ClassifyAsPasswordForm(
      AutofillManager& manager,
      FormGlobalId form_id,
      FieldGlobalId field_id) const final;
  void TriggerPlusAddressUserPerceptionSurvey(
      plus_addresses::hats::SurveyType survey_type) final;

  // TODO(crbug.com/407666146): Create a test API.
  base::WeakPtr<AutofillSuggestionController>
  suggestion_controller_for_testing() {
    return suggestion_controller_;
  }
#if defined(UNIT_TEST)
  void SetKeepPopupOpenForTesting(bool keep_popup_open_for_testing) {
    keep_popup_open_for_testing_ = keep_popup_open_for_testing;
    if (suggestion_controller_) {
      suggestion_controller_->SetKeepPopupOpenForTesting(
          keep_popup_open_for_testing);
    }
  }
  void SetAutofillFieldPromoTesting(
      std::unique_ptr<AutofillFieldPromoController> test_controller) {
    autofill_field_promo_controller_ = std::move(test_controller);
  }
#if BUILDFLAG(IS_ANDROID)
  void SetAutofillSnackbarControllerImplForTesting(
      std::unique_ptr<AutofillSnackbarControllerImpl>
          autofill_snackbar_controller_impl) {
    autofill_snackbar_controller_impl_ =
        std::move(autofill_snackbar_controller_impl);
  }
#endif
#endif  // defined(UNIT_TEST)

  // ContentAutofillClient:
  std::unique_ptr<AutofillManager> CreateManager(
      base::PassKey<ContentAutofillDriver> pass_key,
      ContentAutofillDriver& driver) final;

 protected:
  explicit ChromeAutofillClient(content::WebContents* web_contents);

 private:
  Profile* GetProfile() const;
  bool SupportsConsentlessExecution(const url::Origin& origin);
  void ShowAutofillSuggestionsImpl(
      SuggestionUiSessionId session_id,
      const PopupOpenArgs& open_args,
      base::WeakPtr<AutofillSuggestionDelegate> delegate);

  const raw_ptr<LogRouter> log_router_ =
      AutofillLogRouterFactory::GetForBrowserContext(
          GetWebContents().GetBrowserContext());
  std::unique_ptr<LogManager> log_manager_;
  autofill_metrics::FormInteractionsUkmLogger form_interactions_ukm_logger_{
      this};

  // These members are initialized lazily in their respective getters.
  // Therefore, do not access the members directly.
  std::unique_ptr<AutofillCrowdsourcingManager> crowdsourcing_manager_;
  VotesUploader votes_uploader_{this};
  std::unique_ptr<FormDataImporter> form_data_importer_;

  payments::ChromePaymentsAutofillClient payments_autofill_client_{this};
  SingleFieldFillRouter single_field_fill_router_{
      // This call is during construction, so GetAutocompleteHistoryManager()
      // does not dispatch to more-derived classes, should there be any.
      GetAutocompleteHistoryManager(),
      payments_autofill_client_.GetIbanManager(),
      payments_autofill_client_.GetMerchantPromoCodeManager()};

  base::WeakPtr<AutofillSuggestionController> suggestion_controller_;
  FormInteractionsFlowId flow_id_;
  base::Time flow_id_date_;
  // If set to true, the popup will stay open regardless of external changes on
  // the test machine, that may normally cause the popup to be hidden
  bool keep_popup_open_for_testing_ = false;
#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<SaveUpdateAddressProfileFlowManager>
      save_update_address_profile_flow_manager_;
  std::unique_ptr<FastCheckoutClient> fast_checkout_client_;
  std::unique_ptr<AutofillSnackbarControllerImpl>
      autofill_snackbar_controller_impl_;
#endif
  std::unique_ptr<AutofillFieldPromoController>
      autofill_field_promo_controller_;
  // Test addresses used to allow developers to test their forms.
  std::vector<AutofillProfile> test_addresses_;
  const AutofillAblationStudy ablation_study_;

  ContentIdentityCredentialDelegate identity_credential_delegate_;
  base::WeakPtrFactory<ChromeAutofillClient> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_H_
