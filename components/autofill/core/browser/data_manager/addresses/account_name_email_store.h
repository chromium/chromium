// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ACCOUNT_NAME_EMAIL_STORE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ACCOUNT_NAME_EMAIL_STORE_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"

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
class AccountNameEmailStore {
 public:
  AccountNameEmailStore(AddressDataManager& address_data_manager,
                        PrefService& pref_service);

  // Updates the `kAccountNameEmail` autofill profile with the newest signed-in
  // account `info`. If the `kAccountNameEmail` profile doesn't exist, it is
  // created.
  void UpdateOrCreateAccountNameEmail(const AccountInfo& info);

 private:
  friend class AccountNameEmailStoreTestApi;

  // Hashes concatenated full_name and email_address delimited by |.
  std::string HashAccountInfo(const AccountInfo& info) const;

  const raw_ref<AddressDataManager> address_data_manager_;
  const raw_ref<PrefService> pref_service_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_ACCOUNT_NAME_EMAIL_STORE_H_
