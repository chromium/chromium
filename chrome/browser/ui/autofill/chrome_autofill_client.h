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
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_ablation_study.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/autofill_prediction_improvements_delegate.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/password_form_classification.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/autofill/core/browser/ui/fast_checkout_client.h"
#else
#include "chrome/browser/ui/autofill/payments/manage_migration_ui_controller.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {

#if BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/364089352): When //c/b/ui/android/autofill gets modularized,
// //c/b/ui/autofill/ can depend directly on it. Now, forward declare the
// SaveUpdateAddressProfileFlowManager.
class SaveUpdateAddressProfileFlowManager;
#endif

class AutofillOptimizationGuide;
class FormFieldData;
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
  AutofillComposeDelegate* GetComposeDelegate() override;
  AutofillPlusAddressDelegate* GetPlusAddressDelegate() override;
  AutofillPredictionImprovementsDelegate*
  GetAutofillPredictionImprovementsDelegate() override;
  void OfferPlusAddressCreation(const url::Origin& main_frame_origin,
                                PlusAddressCallback callback) override;
  void ShowPlusAddressError(PlusAddressErrorDialogType error_dialog_type,
                            base::OnceClosure on_accepted) override;
  void ShowPlusAddressAffiliationError(std::u16string affiliated_domain,
                                       std::u16string affiliated_plus_address,
                                       base::OnceClosure on_accepted) override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  syncer::SyncService* GetSyncService() override;
  signin::IdentityManager* GetIdentityManager() override;
  const signin::IdentityManager* GetIdentityManager() const override;
  FormDataImporter* GetFormDataImporter() override;
  payments::ChromePaymentsAutofillClient* GetPaymentsAutofillClient() override;
  StrikeDatabase* GetStrikeDatabase() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  AddressNormalizer* GetAddressNormalizer() override;
  const GURL& GetLastCommittedPrimaryMainFrameURL() const override;
  url::Origin GetLastCommittedPrimaryMainFrameOrigin() const override;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override;
  const translate::LanguageState* GetLanguageState() override;
  translate::TranslateDriver* GetTranslateDriver() override;
  GeoIpCountryCode GetVariationConfigCountryCode() const override;
  profile_metrics::BrowserProfileType GetProfileType() const override;
  FastCheckoutClient* GetFastCheckoutClient() override;
  void ShowAutofillSettings(SuggestionType suggestion_type) override;
  void ShowEditAddressProfileDialog(
      const AutofillProfile& profile,
      AddressProfileSavePromptCallback on_user_decision_callback) override;
  void ShowDeleteAddressProfileDialog(
      const AutofillProfile& profile,
      AddressProfileDeleteDialogCallback delete_dialog_callback) override;
  void ConfirmSaveAddressProfile(
      const AutofillProfile& profile,
      const AutofillProfile* original_profile,
      bool is_migration_to_account,
      AddressProfileSavePromptCallback callback) override;
  SuggestionUiSessionId ShowAutofillSuggestions(
      const PopupOpenArgs& open_args,
      base::WeakPtr<AutofillSuggestionDelegate> delegate) override;
  void ShowPlusAddressEmailOverrideNotification(
      const std::string& original_email,
      EmailOverrideUndoCallback email_override_undo_callback) override;
  void UpdateAutofillDataListValues(
      base::span<const SelectOption> datalist) override;
  base::span<const Suggestion> GetAutofillSuggestions() const override;
  void PinAutofillSuggestions() override;
  std::optional<PopupScreenLocation> GetPopupScreenLocation() const override;
  std::optional<SuggestionUiSessionId>
  GetSessionIdForCurrentAutofillSuggestions() const override;
  void UpdateAutofillSuggestions(
      const std::vector<Suggestion>& suggestions,
      FillingProduct main_filling_product,
      AutofillSuggestionTriggerSource trigger_source) override;
  void HideAutofillSuggestions(SuggestionHidingReason reason) override;
  void TriggerUserPerceptionOfAutofillSurvey(
      FillingProduct filling_product,
      const std::map<std::string, std::string>& field_filling_stats_data)
      override;
  bool IsAutocompleteEnabled() const override;
  bool IsPasswordManagerEnabled() override;
  void DidFillOrPreviewForm(mojom::ActionPersistence action_persistence,
                            AutofillTriggerSource trigger_source,
                            bool is_refill) override;
  bool IsContextSecure() const override;
  LogManager* GetLogManager() const override;
  const AutofillAblationStudy& GetAblationStudy() const override;
  FormInteractionsFlowId GetCurrentFormInteractionsFlowId() override;
  std::unique_ptr<device_reauth::DeviceAuthenticator> GetDeviceAuthenticator()
      override;
  void ShowAutofillFieldIphForFeature(
      const FormFieldData& field,
      AutofillClient::IphFeature feature) override;
  void HideAutofillFieldIph() override;
  void NotifyAutofillManualFallbackUsed() override;
  void ShowSaveAutofillPredictionImprovementsBubble(
      const std::vector<optimization_guide::proto::UserAnnotationsEntry>&
          to_be_upserted_entries,
      base::OnceCallback<void(bool prompt_was_accepted)>
          prompt_acceptance_callback) override;
  void set_test_addresses(std::vector<AutofillProfile> test_addresses) override;
  base::span<const AutofillProfile> GetTestAddresses() const override;
  PasswordFormClassification ClassifyAsPasswordForm(
      AutofillManager& manager,
      FormGlobalId form_id,
      FieldGlobalId field_id) const override;

  // TODO(crbug.com/320634151): Create a test API.
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
  void SetAutofillFieldPromoControllerManualFallbackForTesting(
      std::unique_ptr<AutofillFieldPromoController> test_controller) {
    autofill_field_promo_controller_ = std::move(test_controller);
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
  bool SupportsConsentlessExecution(const url::Origin& origin);
  void ShowAutofillSuggestionsImpl(
      SuggestionUiSessionId session_id,
      const PopupOpenArgs& open_args,
      base::WeakPtr<AutofillSuggestionDelegate> delegate);
  base::WeakPtr<ChromeAutofillClient> GetWeakPtr();

  std::unique_ptr<LogManager> log_manager_;

  // These members are initialized lazily in their respective getters.
  // Therefore, do not access the members directly.
  std::unique_ptr<AutofillCrowdsourcingManager> crowdsourcing_manager_;
  std::unique_ptr<FormDataImporter> form_data_importer_;

  payments::ChromePaymentsAutofillClient payments_autofill_client_{this};

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
#endif
  std::unique_ptr<AutofillFieldPromoController>
      autofill_field_promo_controller_;
  // Test addresses used to allow developers to test their forms.
  std::vector<AutofillProfile> test_addresses_;
  const AutofillAblationStudy ablation_study_;
  base::WeakPtrFactory<ChromeAutofillClient> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_H_
