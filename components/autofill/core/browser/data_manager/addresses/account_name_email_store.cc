// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/addresses/account_name_email_store.h"

#include <optional>

#include "base/hash/hash.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"

namespace autofill {

namespace {

constexpr std::string_view kSeparator = "|";

}  // namespace

AccountNameEmailStore::AccountNameEmailStore(
    AddressDataManager& address_data_manager,
    signin::IdentityManager& identity_manager,
    syncer::SyncService& sync_service,
    PrefService& pref_service)
    : address_data_manager_(address_data_manager),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      pref_service_(pref_service) {
  address_data_manager_observer_.Observe(&address_data_manager);
  identity_manager_observer_.Observe(&identity_manager);
  sync_service_observer_.Observe(&sync_service);
}

AccountNameEmailStore::~AccountNameEmailStore() = default;

void AccountNameEmailStore::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    RemoveAccountNameEmail();
  }
}

void AccountNameEmailStore::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  const std::optional<CoreAccountInfo>& primary_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (!primary_info.has_value() ||
      (!primary_info->IsEmpty() && info.gaia != primary_info->gaia)) {
    return;
  }
  UpdateOrCreateAccountNameEmail(info);
}

void AccountNameEmailStore::OnAddressDataChanged() {
  if (pref_service_->GetInteger(
          prefs::kAutofillNameAndEmailProfileNotSelectedCounter) >
      features::kAutofillNameAndEmailProfileNotSelectedThreshold.Get()) {
    // Return the kAccountNameEmail profile is already considered removed.
    return;
  }

  const std::vector<const AutofillProfile*> account_name_email_profiles =
      address_data_manager_->GetProfilesByRecordType(
          AutofillProfile::RecordType::kAccountNameEmail);

  if (identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin) &&
      account_name_email_profiles.empty()) {
    // The `kAccountNameEmail` is available to all signed in users. If it isn't,
    // that means that the user just removed it. Track this removal in prefs to
    // ensure that the profile isn't recreated. Independently of how the profile
    // was removed, the removal is tracked as if the user rejected a
    // `kAccountNameEmail` suggestion too many times.
    pref_service_->SetInteger(
        prefs::kAutofillNameAndEmailProfileNotSelectedCounter,
        features::kAutofillNameAndEmailProfileNotSelectedThreshold.Get() + 1);
  }
}

void AccountNameEmailStore::UpdateOrCreateAccountNameEmail(
    const AccountInfo& info) {
  // During signin the `OnExtendedAccountInfoUpdated` method might call this
  // method with an empty `info.full_name` since not all data arrives all at
  // once and `AccountiInfo` is updated multiple times. The `kAccountNameEmail`
  // profile and hash signature require non-empty `full_name` value.
  if (info.IsEmpty() || info.full_name.empty()) {
    return;
  }
  CHECK(!info.email.empty());

  // Calculate hash and see if it's different than one cached in pref.
  const std::string new_hash = HashAccountInfo(info);
  if (pref_service_->GetString(prefs::kAutofillNameAndEmailProfileSignature) ==
      new_hash) {
    // Name exists and has not changed - nothing to do.
    // This also (additionally) prevents recreation of Account Name Email
    // profile after its explicit or silent deletion.
    return;
  }

  // If the current `info` doesn't match the stored kAccountNameAndEmail
  // profile, the existing profile should be deleted and a new created.
  const std::vector<const AutofillProfile*> account_name_email_profiles =
      address_data_manager_->GetProfilesByRecordType(
          AutofillProfile::RecordType::kAccountNameEmail);
  if (!account_name_email_profiles.empty()) {
    CHECK_EQ(1u, account_name_email_profiles.size());
    address_data_manager_->RemoveProfile(
        account_name_email_profiles[0]->guid());
  }

  // If Account Name Email profile doesn't exist, create and add it.
  address_data_manager_->AddProfile(AutofillProfile{info});

  pref_service_->SetString(prefs::kAutofillNameAndEmailProfileSignature,
                           new_hash);
  // Reset `kAutofillNameAndEmailProfileNotSelectedCounter` after the user
  // changed their full name.
  pref_service_->SetInteger(
      prefs::kAutofillNameAndEmailProfileNotSelectedCounter, 0);
}

void AccountNameEmailStore::RemoveAccountNameEmail() {
  const std::vector<const AutofillProfile*> account_name_email_profiles =
      address_data_manager_->GetProfilesByRecordType(
          AutofillProfile::RecordType::kAccountNameEmail);
  if (account_name_email_profiles.empty()) {
    return;
  }
  CHECK_EQ(1u, account_name_email_profiles.size());

  address_data_manager_->RemoveProfile(account_name_email_profiles[0]->guid());
}

std::string AccountNameEmailStore::HashAccountInfo(
    const AccountInfo& info) const {
  return base::NumberToString(base::PersistentHash(
      base::StrCat({info.full_name, kSeparator, info.email})));
}

}  // namespace autofill
