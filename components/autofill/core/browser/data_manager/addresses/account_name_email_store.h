// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ACCOUNT_NAME_EMAIL_STORE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ACCOUNT_NAME_EMAIL_STORE_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

namespace autofill {

// The `kAccountNameEmail` autofill profile is an un-syncable, locally stored,
// profile generated automatically for every signed in user, unless they deleted
// it or didn't use it. This profile is composed of 2 pieces of data:
// - full name
// - email
// Keeping `kAccountNameEmail` profile's state up to date between the devices is
// handled through syncable prefs. `AccountNameEmailStore` is a class
// responsible for accessing and modifying those prefs, as well as managing
// (create, update, remove) `kAccountNameEmail` profile. In code
// `AccountNameEmailStore` is owned by and has the same lifetime as
// `AddressDataManager`.
class AccountNameEmailStore : public signin::IdentityManager::Observer,
                              public AddressDataManager::Observer,
                              public syncer::SyncServiceObserver {
 public:
  AccountNameEmailStore(AddressDataManager& address_data_manager,
                        signin::IdentityManager& identity_manager,
                        syncer::SyncService& sync_service,
                        PrefService& pref_service);
  ~AccountNameEmailStore() override;

  // IdentityManager::Observer:
  // Called when the user signs out. Used to remove `kAccountNameEmail` profile.
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;
  // Called when the account's extended information (e.g., full name) is
  // updated. Used to keep the `kAccountNameEmail` profile up to date.
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  // AddressDataManager::Observer:
  // Called when the address data of the `AddressDataManager` changes. If
  // `kAccountNameEmail` profile is missing but the user is still signed in,
  // `kAutofillNameAndEmailProfileNotSelectedCounter` is set to
  // `kAutofillNameAndEmailProfileNotSelectedThreshold` + 1.
  void OnAddressDataChanged() override;

  // Updates the `kAccountNameEmail` autofill profile with the newest signed-in
  // account `info`. If the `kAccountNameEmail` profile doesn't exist, it is
  // created.
  void UpdateOrCreateAccountNameEmail(const AccountInfo& info);

  // Removes the `kAccountNameEmail` autofill profile if it exists.
  void RemoveAccountNameEmail();

 private:
  friend class AccountNameEmailStoreTestApi;

  // Hashes concatenated full_name and email_address delimited by |.
  std::string HashAccountInfo(const AccountInfo& info) const;

  const raw_ref<AddressDataManager> address_data_manager_;
  const raw_ref<signin::IdentityManager> identity_manager_;
  const raw_ref<syncer::SyncService> sync_service_;
  const raw_ref<PrefService> pref_service_;

  // Used to update `kAutofillNameAndEmailProfileNotSelectedCounter` pref in
  // `OnAddressDataChanged` method.
  base::ScopedObservation<AddressDataManager, AddressDataManager::Observer>
      address_data_manager_observer_{this};

  // Used to update the `kAccountNameEmail` profile when the account name
  // changes.
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observer_{this};

  // Used to create/remove the `kAccountNameEmail` profile when autofill's sync
  // state changes.
  // TODO(crbug.com/356845298): Refactor removing and creating profile in
  // overridden `OnStateChanged(SyncService*)` method.
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observer_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ACCOUNT_NAME_EMAIL_STORE_H_
