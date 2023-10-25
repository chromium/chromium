// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_

#include <unordered_set>

#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/plus_addresses/plus_address_client.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

class PrefService;

namespace signin {
class IdentityManager;
class PersistentRepeatingTimer;
}  // namespace signin

namespace plus_addresses {

// An experimental class for filling plus addresses (asdf+123@some-domain.com).
// Not intended for widespread use.
class PlusAddressService : public KeyedService,
                           public signin::IdentityManager::Observer {
 public:
  // Used to simplify testing in cases where calls depending on external classes
  // can be mocked out.
  PlusAddressService();
  // Used to simplify testing in cases where calls depend on just the
  // `IdentityManager`.
  explicit PlusAddressService(signin::IdentityManager* identity_manager);
  ~PlusAddressService() override;

  // Initialize the PlusAddressService with a `IdentityManager`, `PrefService`,
  // and a `SharedURLLoaderFactory`.
  PlusAddressService(signin::IdentityManager* identity_manager,
                     PrefService* pref_service,
                     PlusAddressClient plus_address_client);

  // Returns `true` when plus addresses are supported. Currently requires only
  // that the `kPlusAddressesEnabled` base::Feature is enabled.
  // Virtual to allow overriding the behavior in tests. This allows external
  // tests (e.g., those in autofill that depend on this class) to substitute
  // their own behavior.
  virtual bool SupportsPlusAddresses(url::Origin origin);
  // Get a plus address, if one exists, for the passed-in origin. Note that all
  // plus address activity is scoped to eTLD+1. This class owns the conversion
  // of `origin` to its eTLD+1 form.
  absl::optional<std::string> GetPlusAddress(url::Origin origin);
  // Same as above, but packages the plus address along with its eTLD+1.
  absl::optional<PlusProfile> GetPlusProfile(url::Origin origin);
  // Save a plus address for the given origin, which is converted to its eTLD+1
  // form prior to persistence.
  void SavePlusAddress(url::Origin origin, std::string plus_address);
  // Check whether the passed-in string is a known plus address.
  bool IsPlusAddress(std::string potential_plus_address);

  // Asks the PlusAddressClient to get a plus address for use on `origin` and on
  // completion: runs`callback` with the created plus address, and stores the
  // plus address in this service.
  // Virtual to allow overriding the behavior in tests.
  // TODO (crbug.com/1467623): Remove this once dependencies are migrated away.
  virtual void OfferPlusAddressCreation(const url::Origin& origin,
                                        PlusAddressCallback callback);

  // Asks the PlusAddressClient to reserve a plus address for use on `origin`,
  // and returns the plus address via `on_completed`.
  //
  // Virtual to allow overriding the behavior in tests.
  virtual void ReservePlusAddress(const url::Origin& origin,
                                  PlusAddressRequestCallback on_completed);

  // Asks the PlusAddressClient to confirm `plus_address` for use on `origin`.
  // and returns the plus address via `on_completed`.
  //
  // Virtual to allow overriding the behavior in tests.
  virtual void ConfirmPlusAddress(const url::Origin& origin,
                                  const std::string& plus_address,
                                  PlusAddressRequestCallback on_completed);

  // The label for an autofill suggestion offering to create a new plus address.
  // While only debatably relevant to this class, this function allows for
  // further decoupling of PlusAddress generation and autofill.
  std::u16string GetCreateSuggestionLabel();

  // Used for displaying the user's email address in the UI modal.
  // virtual to allow mocking in tests that don't want to do identity setup.
  virtual absl::optional<std::string> GetPrimaryEmail();

  // Gets the up-to-date mapping from the remote server from the
  // PlusAddressClient and returns it via `callback`.
  // This is only intended to be called by the `repeating_timer_`.
  //
  // TODO (crbug.com/1467623): Make this private when testing improves.
  void SyncPlusAddressMapping();

  bool is_enabled() const;

 private:
  // Creates and starts a timer to keep `plus_address_by_site_` and
  // `plus_addresses` in sync with a remote plus address server.
  //
  // This has no effect if this service is not enabled, `pref_service_` is null
  // or `repeating_timer_` has already been created.
  void CreateAndStartTimer();

  // Updates `plus_address_by_site_` and `plus_addresses_` using `map`.
  void UpdatePlusAddressMap(const PlusAddressMap& map);

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error) override;

  void HandleSignout();

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
  std::unique_ptr<signin::PersistentRepeatingTimer> repeating_timer_;

  // Handles requests to a remote server that this service uses.
  PlusAddressClient plus_address_client_;

  // Stores last auth error (potentially NONE) to toggle is_enabled() on/off.
  // Defaults to NONE to enable this service while refresh tokens (and potential
  // auth errors) are loading.
  GoogleServiceAuthError primary_account_auth_error_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_SERVICE_H_
