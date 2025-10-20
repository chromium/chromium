// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ACCOUNT_NAME_EMAIL_STORE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ACCOUNT_NAME_EMAIL_STORE_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

namespace autofill {

// The kAccountNameEmail autofill profile is an un-syncable, locally stored,
// profile generated automatically for the every signed in user with the
// Autofill sync toggle enabled, unless they have deleted it or have not used
// it. There are two ways this profile can be removed:
// 1. Soft remove - which happens when the user does one of the following:
// - turns off the autofill sync toggle,
// - signs out.
// When the toggle is turned on again, or the user signs in again, the
// kAccountNameEmail profile reappears.
// 2. Hard remove - which happens when the user does one of the following:
// - explicitly removes profile from the autofill settings,
// - does not use the kAccountNameEmail profile during the first
//   `kAutofillNameAndEmailProfileNotSelectedThreshold` times it was suggested,
// - accepts an import of an `AutofillProfile` that is a superset of
//   kAccountNameEmail (`AutofillProfileImportType::kNameEmailSuperset` and
//   `AutofillProfileImportType::kHomeWorkNameEmailMerge`).
// The profile will always be recreated after removal (including a hard remove)
// when the users account full name changes (assuming that other feature
// conditions are met).
//
// This profile is composed of 2 pieces of data:
// - full name
// - email
// `kLegacyHierarchyCountryCode` is used to not reveal the country of this
// profile.
//
// Keeping kAccountNameEmail profile's state up to date between the devices is
// handled through priority prefs. `AccountNameEmailStore` is a class
// responsible for accessing and modifying those prefs, as well as managing
// (create, update, remove) kAccountNameEmail profile. In code
// `AccountNameEmailStore` is owned by and has the same lifetime as
// `AddressDataManager`.
class AccountNameEmailStore : public signin::IdentityManager::Observer,
                              public syncer::SyncServiceObserver {
 public:
  AccountNameEmailStore(AddressDataManager& address_data_manager,
                        signin::IdentityManager& identity_manager,
                        syncer::SyncService& sync_service,
                        PrefService& pref_service);
  ~AccountNameEmailStore() override;

  // IdentityManager::Observer:
  // Called when the account's extended information (e.g. full name) is
  // updated. Used to keep the kAccountNameEmail profile up to date.
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  // syncer::SyncServiceObserver:
  void OnSyncShutdown(syncer::SyncService* sync) override;
  void OnStateChanged(syncer::SyncService* sync_service) override;

  // Checks that the necessary data is available and that the user has enabled
  // autofill sync before updating/creating the kAccountNameEmail profile.
  // This prevents premature update/create without all of the relevant data.
  void MaybeUpdateOrCreateAccountNameEmail();

#if BUILDFLAG(IS_IOS)
  // The same as MaybeUpdateOrCreateAccountNameEmail(), but creates/updates the
  // kAccountNameEmail profile using `account_name` and `email`.
  // TODO(crbug.com/449708427): Remove once `AccountInfo` supports full_name on
  // IOS.
  void MaybeUpdateOrCreateAccountNameEmail(const std::string& account_name,
                                           const std::string& email);
#endif

  // Persists the `change` in prefs, if it applies to kAccountNameEmail
  // profile.
  void ApplyChange(const AutofillProfileChange& change);

  // Removes the kAccountNameEmail autofill profile if it exists. If
  // `is_soft_removal` is true then the AccountNameEmail profile will be
  // recreated when conditions are met again, otherwise it will be recreated iff
  // the account name changed.
  void RemoveAccountNameEmail(bool is_soft_removal);

 private:
  friend class AccountNameEmailStoreTestApi;

  enum class ProfileUpdateBlockReason {
    // The user is signed-in, but explicitly disabled
    // address syncing.
    kAutofillSyncToggleDisabled = 0,
    // The user is signed-in, but not all address data
    // or priority prefs have been loaded.
    kDataNotLoaded = 1,
    // Signed-out.
    kUserSignedOut = 2,
    // The user is signed-in, but sync-the-feature is disabled.
    // TODO(crbug.com/40066949): Do not block profile creation when
    // sync-the-feature gets removed.
    kSyncDisabled = 3,
  };

  // Updates the kAccountNameEmail autofill profile with the account `info`. If
  // the kAccountNameEmail profile doesn't exist, it is created.
  void UpdateOrCreateAccountNameEmail(AccountInfo& info);

  // Hashes concatenated full_name and email_address delimited by |.
  std::string HashAccountInfo(const AccountInfo& info) const;

  // Determines if the conditions for creating or updating the kAccountNameEmail
  // profile are not met. The operation should be blocked if sync is disabled
  // for Autofill or if sync is enabled but the necessary data has not yet been
  // downloaded.
  // Returns a blocking reason or nullopt if the operation shouldn't be blocked.
  std::optional<ProfileUpdateBlockReason>
  GetBlockAccountNameEmailUpdateReason();

  // Called when `prefs::kAutofillNameAndEmailProfileNotSelectedCounter` pref is
  // updated. If it's value exceeds
  // `kAutofillNameAndEmailProfileNotSelectedThreshold` the kAccountNameEmail
  // profile will be removed.
  void OnCounterPrefUpdated();

  // Returns true if primary account exists and there are no blocking reasons,
  // false otherwise.
  bool ShouldUpdateOrCreateAccountNameEmail();

  // `this` is owned by `address_data_manager_`, so `address_data_manager_` will
  // outlive this class.
  const raw_ref<AddressDataManager> address_data_manager_;
  raw_ref<PrefService> pref_service_;

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

  // Used to observe `prefs::kAutofillNameAndEmailProfileNotSelectedCounter` and
  // possibly remove the kAccountNameEmail profile.
  PrefChangeRegistrar pref_registrar_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ACCOUNT_NAME_EMAIL_STORE_H_
