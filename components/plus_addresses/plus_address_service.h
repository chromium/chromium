// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/plus_addresses/affiliations/plus_address_affiliation_match_helper.h"
#include "components/plus_addresses/metrics/plus_address_submission_logger.h"
#include "components/plus_addresses/plus_address_cache.h"
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
class FormFieldData;
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
class PlusAddressService : public KeyedService,
                           public autofill::AutofillPlusAddressDelegate,
                           public signin::IdentityManager::Observer,
                           public PlusAddressWebDataService::Observer,
                           public WebDataServiceConsumer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever the set of plus addresses cached in the service
    // gets modified (e.g. `SavePlusProfile` calls, sync updates, etc).
    // `changes` represents a sequence of addition or removal operations
    // triggered on the local cache. Notably, update operations are emulated as
    // a remove operation of the old value followed by an addition of the
    // updated value.
    virtual void OnPlusAddressesChanged(
        const std::vector<PlusAddressDataChange>& changes) = 0;

    // Called when the observed PlusAddressService is being destroyed.
    virtual void OnPlusAddressServiceShutdown() = 0;
  };

  using FeatureEnabledForProfileCheck =
      base::RepeatingCallback<bool(const base::Feature&)>;

  // Callback to return the list of plus profiles.
  using GetPlusProfilesCallback =
      base::OnceCallback<void(std::vector<PlusProfile>)>;
  // The number of `HTTP_FORBIDDEN` responses that the user may receive before
  // `this` is disabled for this session. If a user makes a single successful
  // call, this limit no longer applies.
  static constexpr int kMaxHttpForbiddenResponses = 1;

  PlusAddressService(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      PlusAddressSettingService* setting_service,
      std::unique_ptr<PlusAddressHttpClient> plus_address_http_client,
      scoped_refptr<PlusAddressWebDataService> webdata_service,
      affiliations::AffiliationService* affiliation_service,
      FeatureEnabledForProfileCheck feature_enabled_for_profile_check);
  ~PlusAddressService() override;

  void AddObserver(Observer* o) { observers_.AddObserver(o); }
  void RemoveObserver(Observer* o) { observers_.RemoveObserver(o); }

  // autofill::AutofillPlusAddressDelegate:
  // Checks whether the passed-in string is a known plus address.
  bool IsPlusAddress(const std::string& potential_plus_address) const override;
  void GetSuggestions(
      const url::Origin& last_committed_primary_main_frame_origin,
      bool is_off_the_record,
      const autofill::AutofillClient::PasswordFormClassification&
          focused_form_classification,
      const autofill::FormFieldData& focused_field,
      autofill::AutofillSuggestionTriggerSource trigger_source,
      GetSuggestionsCallback callback) override;
  autofill::Suggestion GetManagePlusAddressSuggestion() const override;
  bool ShouldMixWithSingleFieldFormFillSuggestions() const override;
  void RecordAutofillSuggestionEvent(SuggestionEvent suggestion_event) override;
  void OnPlusAddressSuggestionShown(
      autofill::AutofillManager& manager,
      autofill::FormGlobalId form,
      autofill::FieldGlobalId field,
      SuggestionContext suggestion_context,
      autofill::AutofillClient::PasswordFormClassification::Type form_type,
      autofill::SuggestionType suggestion_type) override;

  // PlusAddressWebDataService::Observer:
  void OnWebDataChangedBySync(
      const std::vector<PlusAddressDataChange>& changes) override;

  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override;

  // Returns whether plus address filling is supported for the given `origin`.
  // This is true iff:
  // - the `PlusAddressService` is enabled and
  // - `origin` is not a blocked origin.
  virtual bool IsPlusAddressFillingEnabled(const url::Origin& origin) const;

  // Returns whether plus address creation is supported for the given `origin`.
  // This is true iff:
  // - the plus address filling is enabled,
  // - `is_off_the_record` is `false`, and
  // - plus address global toggle is on.
  virtual bool IsPlusAddressCreationEnabled(const url::Origin& origin,
                                            bool is_off_the_record) const;

  // Returns whether a manual fallback suggestion should be shown on `origin`.
  // This is true iff
  // - plus address creation is supported or
  // - `is_off_the_record` is `true`  and the user has at least 1 plus address
  // for the given `origin`.
  bool ShouldShowManualFallback(const url::Origin& origin,
                                bool is_off_the_record) const;

  // Gets a plus address, if one exists, for the passed-in facet.
  std::optional<PlusAddress> GetPlusAddress(
      const PlusProfile::facet_t& facet) const;

  // Same as `GetPlusAddress()`, but returns the entire profile.
  std::optional<PlusProfile> GetPlusProfile(
      const PlusProfile::facet_t& facet) const;

  // Returns a list of plus profiles for the `origin` and all affiliated
  // domains.
  virtual void GetAffiliatedPlusProfiles(const url::Origin& origin,
                                         GetPlusProfilesCallback callback);

  // Returns all the cached plus profiles. There are no server requests
  // triggered by this method, only the cached responses are returned.
  virtual base::span<const PlusProfile> GetPlusProfiles() const;

  // Saves a confirmed plus profile for its facet.
  void SavePlusProfile(const PlusProfile& profile);

  // Asks the PlusAddressHttpClient to reserve a plus address for use on
  // `origin` and returns the plus address via `on_completed`.
  //
  // Virtual to allow overriding the behavior in tests.
  virtual void ReservePlusAddress(const url::Origin& origin,
                                  PlusAddressRequestCallback on_completed);

  // Asks the PlusAddressHttpClient to refresh the plus address for `origin` and
  // calls `on_completed` with the result.
  virtual void RefreshPlusAddress(const url::Origin& origin,
                                  PlusAddressRequestCallback on_completed);

  // Returns whether refreshing a plus address on `origin` is supported.
  bool IsRefreshingSupported(const url::Origin& origin);

  // Asks the PlusAddressHttpClient to confirm `plus_address` for use on
  // `origin` and returns the plus address via `on_completed`.
  //
  // Virtual to allow overriding the behavior in tests.
  virtual void ConfirmPlusAddress(const url::Origin& origin,
                                  const PlusAddress& plus_address,
                                  PlusAddressRequestCallback on_completed);

  // Used for displaying the user's email address in the UI modal.
  // virtual to allow mocking in tests that don't want to do identity setup.
  virtual std::optional<std::string> GetPrimaryEmail();

  bool IsEnabled() const;

 private:
  // Creates and starts a timer to keep `plus_profiles_` and
  // `plus_addresses_` in sync with a remote plus address server.
  // This has no effect if this service is not enabled or the timer is already
  // running.
  void CreateAndStartTimer();

  // Gets the up-to-date plus address mapping mapping from the remote server
  // from the PlusAddressHttpClient.
  void SyncPlusAddressMapping();

  // Checks whether `error` is a `HTTP_FORBIDDEN` network error and, if there
  // have been more than `kMaxAllowedForbiddenResponses` such calls without a
  // successful one, disables plus addresses for the session.
  void HandlePlusAddressRequestError(const PlusAddressRequestError& error);

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;

  void HandleSignout();

  // Analyzes `maybe_profile` and, if is an error, it reacts to it (e.g.
  // by disabling the service for this user). If it is a confirmed plus profile,
  // it saves it.
  void HandleCreateOrConfirmResponse(const url::Origin& origin,
                                     PlusAddressRequestCallback callback,
                                     const PlusProfileOrError& maybe_profile);

  // Checks whether the `origin` supports plus address.
  // Returns `true` when origin is not opaque, ETLD+1 of `origin` is not
  // on `excluded_sites_` set, and scheme is http or https.
  bool IsSupportedOrigin(const url::Origin& origin) const;

  // Updates `plus_profiles_` and `plus_addresses_` using `map`.
  // TODO(b/322147254): Remove once integration has finished.
  void UpdatePlusAddressMap(const PlusAddressMap& map);

  // Called when PlusAddressService::OnGetAffiliatedPlusProfiles is resolved.
  // Builds a list of suggestions from the list of `affiliated_profiles` and
  // returns it via the `callback`.
  // TODO(crbug.com/340494671): Move to the unnamed namespace.
  void OnGetAffiliatedPlusProfiles(
      const autofill::AutofillClient::PasswordFormClassification&
          focused_form_classification,
      const autofill::FormFieldData& focused_field,
      autofill::AutofillSuggestionTriggerSource trigger_source,
      bool is_off_the_record,
      GetSuggestionsCallback callback,
      std::vector<PlusProfile> affiliated_profiles);

  const raw_ref<PrefService> pref_service_;

  const raw_ref<signin::IdentityManager> identity_manager_;

  // Allows reading and writing global (i.e. across device and across
  // application) settings state for plus addresses.
  const raw_ref<PlusAddressSettingService> setting_service_;

  metrics::PlusAddressSubmissionLogger submission_logger_;

  // A timer to periodically retrieve all plus addresses from a remote server
  // to keep this service in sync.
  base::RepeatingTimer polling_timer_;

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

  // Store set of excluded sites ETLD+1 where PlusAddressService is not
  // supported.
  // TODO(crbug.com/324556906): Remove once `kPlusAddressBlocklistEnabled` is
  // launched.
  base::flat_set<std::string> excluded_sites_;

  // Counts the number of HTTP_FORBIDDEN that the client has received.
  int http_forbidden_responses_ = 0;

  // Stores whether the account for this ProfileKeyedService is forbidden from
  // using the remote server. This is populated once on the initial poll request
  // and not updated afterwards.
  std::optional<bool> account_is_forbidden_ = std::nullopt;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::ScopedObservation<PlusAddressWebDataService,
                          PlusAddressWebDataService::Observer>
      webdata_service_observation_{this};

  base::ObserverList<Observer> observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PlusAddressService> weak_factory_{this};
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_
