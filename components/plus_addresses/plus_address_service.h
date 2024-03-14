// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>

#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/webdata/plus_address_webdata_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/origin.h"

class PrefService;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace plus_addresses {

class PlusAddressAllocator;
class PlusAddressHttpClient;

// An experimental class for filling plus addresses (asdf+123@some-domain.com).
// Not intended for widespread use.
class PlusAddressService : public KeyedService,
                           public autofill::AutofillPlusAddressDelegate,
                           public signin::IdentityManager::Observer,
                           public WebDataServiceConsumer {
 public:
  // The number of `HTTP_FORBIDDEN` responses that the user may receive before
  // `this` is disabled for this session. If a user makes a single successful
  // call, this limit no longer applies.
  static constexpr int kMaxHttpForbiddenResponses = 1;

  PlusAddressService(
      signin::IdentityManager* identity_manager,
      PrefService* pref_service,
      std::unique_ptr<PlusAddressHttpClient> plus_address_http_client,
      scoped_refptr<PlusAddressWebDataService> webdata_service);
  ~PlusAddressService() override;

  // autofill::AutofillPlusAddressDelegate:
  // Checks whether the passed-in string is a known plus address.
  bool IsPlusAddress(const std::string& potential_plus_address) const override;
  std::vector<autofill::Suggestion> GetSuggestions(
      const url::Origin& last_committed_primary_main_frame_origin,
      bool is_off_the_record,
      std::u16string_view focused_field_value,
      autofill::AutofillSuggestionTriggerSource trigger_source) override;
  void RecordAutofillSuggestionEvent(SuggestionEvent suggestion_event) override;

  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override;

  // Returns `true` when plus addresses are supported. This includes checks that
  // the `kPlusAddressesEnabled` base::Feature is enabled, that there's a
  // signed-in user, the ability to talk to the server, and that off-the-record
  // sessions will not offer new plus address creation.
  // Virtual to allow overriding the behavior in tests. This allows external
  // tests (e.g., those in autofill that depend on this class) to substitute
  // their own behavior.
  bool SupportsPlusAddresses(const url::Origin& origin,
                             bool is_off_the_record) const;

  // Same as `GetPlusAddress`, but packages the plus address along with its
  // eTLD+1.
  std::optional<PlusProfile> GetPlusProfile(const url::Origin& origin) const;

  // Returns all the cached plus profiles. There are no server requests
  // triggered by this method, only the cached responses are returned.
  std::vector<PlusProfile> GetPlusProfiles() const;

  // Gets a plus address, if one exists, for the passed-in origin. Note that all
  // plus address activity is scoped to eTLD+1. This class owns the conversion
  // of `origin` to its eTLD+1 form.
  std::optional<std::string> GetPlusAddress(const url::Origin& origin) const;

  // Saves a plus address for the given origin, which is converted to its eTLD+1
  // form prior to persistence.
  void SavePlusAddress(url::Origin origin, std::string plus_address);

  // Asks the PlusAddressHttpClient to reserve a plus address for use on
  // `origin` and returns the plus address via `on_completed`.
  //
  // Virtual to allow overriding the behavior in tests.
  virtual void ReservePlusAddress(const url::Origin& origin,
                                  PlusAddressRequestCallback on_completed);

  // Asks the PlusAddressHttpClient to confirm `plus_address` for use on
  // `origin` and returns the plus address via `on_completed`.
  //
  // Virtual to allow overriding the behavior in tests.
  virtual void ConfirmPlusAddress(const url::Origin& origin,
                                  const std::string& plus_address,
                                  PlusAddressRequestCallback on_completed);

  // Used for displaying the user's email address in the UI modal.
  // virtual to allow mocking in tests that don't want to do identity setup.
  virtual std::optional<std::string> GetPrimaryEmail();

  bool is_enabled() const;

  // Updates `plus_address_by_site_` and `plus_addresses_` using `map`.
  // TODO(b/322147254): This is only public for easier testing. Once sync
  // integration has finished, it can be removed entirely.
  void UpdatePlusAddressMap(const PlusAddressMap& map);

 private:
  // Creates and starts a timer to keep `plus_address_by_site_` and
  // `plus_addresses` in sync with a remote plus address server.
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
      const GoogleServiceAuthError& error) override;

  void HandleSignout();

  // Analyzes `maybe_profile` and, if is an error, it reacts to it (e.g.
  // by disabling the service for this user). If it is a confirmed plus profile,
  // it saves it.
  void HandleCreateOrConfirmResponse(const url::Origin& origin,
                                     PlusAddressRequestCallback callback,
                                     const PlusProfileOrError& maybe_profile);

  // Get and parse the excluded sites.
  std::set<std::string> GetAndParseExcludedSites();

  // Checks whether the `origin` supports plus address.
  // Returns `true` when origin is not opaque, ETLD+1 of `origin` is not
  // on `excluded_sites_` set, and scheme is http or https.
  bool IsSupportedOrigin(const url::Origin& origin) const;

  // The user's existing set of plus addresses, scoped to sites.
  PlusAddressMap plus_address_by_site_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Used to drive the `IsPlusAddress` function, and derived from the values of
  // `plus_profiles`.
  std::unordered_set<std::string> plus_addresses_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Stores pointer to IdentityManager instance. It must outlive the
  // PlusAddressService and can be null during tests.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // Stores pointer to a PrefService to create `repeating_timer_` when the user
  // signs in after PlusAddressService is created.
  const raw_ptr<PrefService> pref_service_;

  // A timer to periodically retrieve all plus addresses from a remote server
  // to keep this service in sync.
  base::RepeatingTimer polling_timer_;

  // Handles requests to a remote server that this service uses.
  std::unique_ptr<PlusAddressHttpClient> plus_address_http_client_;

  // Responsible for communicating with `PlusAddressTable`.
  scoped_refptr<PlusAddressWebDataService> webdata_service_;

  // Responsible for allocating new plus addresses.
  const std::unique_ptr<PlusAddressAllocator> plus_address_allocator_;

  // Store set of excluded sites ETLD+1 where PlusAddressService is not
  // supported.
  std::set<std::string> excluded_sites_;

  // Stores last auth error (potentially NONE) to toggle is_enabled() on/off.
  // Defaults to NONE to enable this service while refresh tokens (and potential
  // auth errors) are loading.
  GoogleServiceAuthError primary_account_auth_error_;

  // Counts the number of HTTP_FORBIDDEN that the client has received.
  int http_forbidden_responses_ = 0;

  // Stores whether the account for this ProfileKeyedService is forbidden from
  // using the remote server. This is populated once on the initial poll request
  // and not updated afterwards.
  std::optional<bool> account_is_forbidden_ = std::nullopt;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_
