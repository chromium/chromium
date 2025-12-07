// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_PLUS_ADDRESS_SERVICE_IMPL_H_
#define COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_PLUS_ADDRESS_SERVICE_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "components/autofill/core/browser/integrators/password_form_classification.h"
#include "components/autofill/core/browser/integrators/plus_addresses/autofill_plus_address_delegate.h"
#include "components/plus_addresses/core/browser/affiliations/plus_address_affiliation_match_helper.h"
#include "components/plus_addresses/core/browser/metrics/plus_address_submission_logger.h"
#include "components/plus_addresses/core/browser/plus_address_cache.h"
#include "components/plus_addresses/core/browser/plus_address_service.h"
#include "components/plus_addresses/core/browser/plus_address_types.h"
#include "components/plus_addresses/core/browser/settings/plus_address_setting_service.h"
#include "components/plus_addresses/core/browser/webdata/plus_address_webdata_service.h"
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
                               public PlusAddressWebDataService::Observer {
 public:
  using FeatureEnabledForProfileCheck =
      base::RepeatingCallback<bool(const base::Feature&)>;

  PlusAddressServiceImpl(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      PlusAddressSettingService* setting_service,
      std::unique_ptr<PlusAddressHttpClient> plus_address_http_client,
      scoped_refptr<PlusAddressWebDataService> webdata_service,
      affiliations::AffiliationService* affiliation_service,
      FeatureEnabledForProfileCheck feature_enabled_for_profile_check);
  ~PlusAddressServiceImpl() override;

  // autofill::AutofillPlusAddressDelegate:
  bool IsPlusAddress(const std::string& potential_plus_address) const override;
  bool MatchesPlusAddressFormat(const std::u16string& value) const override;
  bool IsPlusAddressFillingEnabled(const url::Origin& origin) const override;
  bool IsFieldEligibleForPlusAddress(
      const autofill::AutofillField& field) const override;
  void GetAffiliatedPlusAddresses(
      const url::Origin& origin,
      base::OnceCallback<void(std::vector<std::string>)> callback) override;
  std::vector<autofill::Suggestion> GetSuggestionsFromPlusAddresses(
      const std::vector<std::string>& plus_addresses) override;
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
  size_t GetPlusAddressesCount() override;
  std::map<std::string, std::string> GetPlusAddressHatsData() const override;

  // PlusAddressWebDataService::Observer:
  void OnWebDataChangedBySync(
      const std::vector<PlusAddressDataChange>& changes) override;

  void OnWebDataServiceRequestDone(WebDataServiceBase::Handle handle,
                                   std::unique_ptr<WDTypedResult> result);

  // PlusAddressService:
  void AddObserver(PlusAddressService::Observer* o) override;
  void RemoveObserver(PlusAddressService::Observer* o) override;
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

 private:
  // KeyedService.
  void Shutdown() override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  void HandleSignout();

  // Analyzes `maybe_profile` and saves it if it is a confirmed plus profile.
  // Returns `maybe_profile` to make for easier chaining of callbacks.
  const PlusProfileOrError& HandleCreateOrConfirmResponse(
      const PlusProfileOrError& maybe_profile);

  // Checks whether the `origin` supports plus address.
  // Returns `true` when origin is not opaque, not excluded, and scheme is
  // http or https.
  bool IsSupportedOrigin(const url::Origin& origin) const;

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

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::ScopedObservation<PlusAddressWebDataService,
                          PlusAddressWebDataService::Observer>
      webdata_service_observation_{this};

  base::ObserverList<PlusAddressService::Observer> observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PlusAddressServiceImpl> weak_ptr_factory_{this};
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_CORE_BROWSER_PLUS_ADDRESS_SERVICE_IMPL_H_
