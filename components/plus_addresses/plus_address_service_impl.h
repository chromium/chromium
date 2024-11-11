// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_IMPL_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/password_form_classification.h"
#include "components/plus_addresses/affiliations/plus_address_affiliation_match_helper.h"
#include "components/plus_addresses/metrics/plus_address_submission_logger.h"
#include "components/plus_addresses/plus_address_cache.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/settings/plus_address_setting_service.h"
#include "components/plus_addresses/webdata/plus_address_webdata_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/origin.h"

class PrefService;

namespace affiliations {
class AffiliationService;
}

namespace autofill {
class FormData;
}  // namespace autofill

namespace signin {
class IdentityManager;
}  // namespace signin

namespace plus_addresses {

class PlusAddressAllocator;
class PlusAddressHttpClient;
class PlusAddressSettingService;

// An experimental class for filling plus addresses (asdf+123@some-domain.com).
// Not intended for widespread use.
class PlusAddressServiceImpl : public PlusAddressService,
                               public signin::IdentityManager::Observer,
                               public PlusAddressWebDataService::Observer,
                               public WebDataServiceConsumer {
 public:
  using FeatureEnabledForProfileCheck =
      base::RepeatingCallback<bool(const base::Feature&)>;
  using LaunchHatsSurvey = base::RepeatingCallback<void(hats::SurveyType)>;

  PlusAddressServiceImpl(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      PlusAddressSettingService* setting_service,
      std::unique_ptr<PlusAddressHttpClient> plus_address_http_client,
      scoped_refptr<PlusAddressWebDataService> webdata_service,
      affiliations::AffiliationService* affiliation_service,
      FeatureEnabledForProfileCheck feature_enabled_for_profile_check,
      LaunchHatsSurvey launch_hats_survey);
  ~PlusAddressServiceImpl() override;

  // autofill::AutofillPlusAddressDelegate:
  bool IsPlusAddress(const std::string& potential_plus_address) const override;
  bool IsPlusAddressFillingEnabled(const url::Origin& origin) const override;
  bool IsPlusAddressFullFormFillingEnabled() const override;
  void GetAffiliatedPlusAddresses(
      const url::Origin& origin,
      base::OnceCallback<void(std::vector<std::string>)> callback) override;
  std::vector<autofill::Suggestion> GetSuggestionsFromPlusAddresses(
      const std::vector<std::string>& plus_addresses,
      const url::Origin& origin,
      bool is_off_the_record,
      const autofill::FormData& focused_form,
      const base::flat_map<autofill::FieldGlobalId, autofill::FieldTypeGroup>&
          form_field_type_groups,
      const autofill::PasswordFormClassification& focused_form_classification,
      const autofill::FieldGlobalId& focused_field_id,
      autofill::AutofillSuggestionTriggerSource trigger_source) override;
  autofill::Suggestion GetManagePlusAddressSuggestion() const override;
  void RecordAutofillSuggestionEvent(SuggestionEvent suggestion_event) override;
  void OnPlusAddressSuggestionShown(
      autofill::AutofillManager& manager,
      autofill::FormGlobalId form,
      autofill::FieldGlobalId field,
      SuggestionContext suggestion_context,
      autofill::PasswordFormClassification::Type form_type,
      autofill::SuggestionType suggestion_type) override;
  void DidFillPlusAddress() override;
  void OnClickedRefreshInlineSuggestion(
      const url::Origin& last_committed_primary_main_frame_origin,
      base::span<const autofill::Suggestion> current_suggestions,
      size_t current_suggestion_index,
      UpdateSuggestionsCallback update_suggestions_callback) override;
  void OnShowedInlineSuggestion(
      const url::Origin& primary_main_frame_origin,
      base::span<const autofill::Suggestion> current_suggestions,
      UpdateSuggestionsCallback update_suggestions_callback) override;
  void OnAcceptedInlineSuggestion(
      const url::Origin& primary_main_frame_origin,
      base::span<const autofill::Suggestion> current_suggestions,
      size_t current_suggestion_index,
      UpdateSuggestionsCallback update_suggestions_callback,
      HideSuggestionsCallback hide_suggestions_callback,
      PlusAddressCallback fill_field_callback,
      ShowAffiliationErrorDialogCallback show_affiliation_error_dialog,
      ShowErrorDialogCallback show_error_dialog,
      base::OnceClosure reshow_suggestions) override;

  // PlusAddressWebDataService::Observer:
  void OnWebDataChangedBySync(
      const std::vector<PlusAddressDataChange>& changes) override;

  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override;

  // PlusAddressService:
  void AddObserver(PlusAddressService::Observer* o) override;
  void RemoveObserver(PlusAddressService::Observer* o) override;
  bool IsPlusAddressCreationEnabled(const url::Origin& origin,
                                    bool is_off_the_record) const override;
  void GetAffiliatedPlusProfiles(const url::Origin& origin,
                                 GetPlusProfilesCallback callback) override;
  base::span<const PlusProfile> GetPlusProfiles() const override;
  void ReservePlusAddress(const url::Origin& origin,
                          PlusAddressRequestCallback on_completed) override;
  void RefreshPlusAddress(const url::Origin& origin,
                          PlusAddressRequestCallback on_completed) override;
  void ConfirmPlusAddress(const url::Origin& origin,
                          const PlusAddress& plus_address,
                          PlusAddressRequestCallback on_completed) override;
  bool IsRefreshingSupported(const url::Origin& origin) override;
  std::optional<PlusAddress> GetPlusAddress(
      const affiliations::FacetURI& facet) const override;
  std::optional<PlusProfile> GetPlusProfile(
      const affiliations::FacetURI& facet) const override;
  std::optional<std::string> GetPrimaryEmail() override;
  bool ShouldShowManualFallback(const url::Origin& origin,
                                bool is_off_the_record) const override;
  void SavePlusProfile(const PlusProfile& profile) override;
  bool IsEnabled() const override;
  void TriggerUserPerceptionSurvey(hats::SurveyType survey_type) override;

 private:
  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;

  void HandleSignout();

  // Analyzes `maybe_profile` and saves it if it is a confirmed plus profile.
  // Returns `maybe_profile` to make for easier chaining of callbacks.
  const PlusProfileOrError& HandleCreateOrConfirmResponse(
      const PlusProfileOrError& maybe_profile);

  // Analyzes `maybe_profile` and triggers a HaTS survey if
  // * plus address was confirmed successfully.
  // * it's identical to the `requested_address` (it might be different if there
  //   is an affiliation error).
  // * the user has created 3 or more plus addresses.
  const PlusProfileOrError& MaybeTriggerUserPerceptionSurvey(
      const PlusAddress& requested_address,
      const PlusProfileOrError& maybe_profile);

  // Checks whether the `origin` supports plus address.
  // Returns `true` when origin is not opaque, not excluded, and scheme is
  // http or https.
  bool IsSupportedOrigin(const url::Origin& origin) const;

  // Reacts to the server response for confirming a plus address from an inline
  // suggestion.
  // - In all cases, it hides the showing suggestions.
  // - In the success case, it then fills the confirmed plus address.
  // - In the error case, it shows a modal dialog to either
  //   * fill an affiliated plus address, or
  //   * oinform the user that their quota is exhausted, or
  //   * retry by reshowing the suggestions(e.g. on timeout).
  void OnConfirmInlineCreation(
      HideSuggestionsCallback hide_callback,
      PlusAddressCallback fill_callback,
      ShowAffiliationErrorDialogCallback show_affiliation_error,
      ShowErrorDialogCallback show_error,
      base::OnceClosure reshow_suggestions,
      const PlusAddress& requested_address,
      const PlusProfileOrError& profile_or_error);

  const raw_ref<PrefService> pref_service_;

  const raw_ref<signin::IdentityManager> identity_manager_;

  // Allows reading and writing global (i.e. across device and across
  // application) settings state for plus addresses.
  const raw_ref<PlusAddressSettingService> setting_service_;

  metrics::PlusAddressSubmissionLogger submission_logger_;

  // Handles requests to a remote server that this service uses.
  std::unique_ptr<PlusAddressHttpClient> plus_address_http_client_;

  // Responsible for communicating with `PlusAddressTable`.
  scoped_refptr<PlusAddressWebDataService> webdata_service_;

  // Responsible for allocating new plus addresses.
  std::unique_ptr<PlusAddressAllocator> plus_address_allocator_;

  // Plus profiles in-memory cache.
  PlusAddressCache plus_address_cache_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Responsible for supplying a list of plus profiles with domains affiliated
  // to the the originally requested facet.
  PlusAddressAffiliationMatchHelper plus_address_match_helper_;

  // Allows checking whether a group-controlled feature is enabled for the
  // profile associated with this `KeyedService`.
  const FeatureEnabledForProfileCheck feature_enabled_for_profile_check_;

  // Allows launching feature perception surveys.
  const LaunchHatsSurvey launch_hats_survey_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::ScopedObservation<PlusAddressWebDataService,
                          PlusAddressWebDataService::Observer>
      webdata_service_observation_{this};

  base::ObserverList<PlusAddressService::Observer> observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PlusAddressServiceImpl> weak_factory_{this};
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_IMPL_H_
