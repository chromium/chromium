// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/addresses/account_name_email_store.h"

#include <optional>

#include "base/hash/hash.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace autofill {

namespace {
constexpr std::string_view kSeparator = "|";
}

AccountNameEmailStore::AccountNameEmailStore(
    AddressDataManager& address_data_manager,
    PrefService& pref_service)
    : address_data_manager_(address_data_manager),
      pref_service_(pref_service) {}

void AccountNameEmailStore::UpdateOrCreateAccountNameEmail(
    const AccountInfo& info) {
  if (info.IsEmpty()) {
    return;
  }

  // TODO(crbug.com/356845298): Add prefs
  // name_and_email_profile_not_selected_counter and
  // was_name_and_email_profile_used, and check their value to possibly return
  // early.

  // Calculate hash and see if it's different than one cached in pref
  const std::string new_hash = HashAccountInfo(info);
  if (pref_service_->GetString(prefs::kAutofillNameAndEmailProfileSignature) ==
      new_hash) {
    // Name exists and has not changed - nothing to do.
    // This also (additionally) prevents recreation of Account Name Email
    // profile after its explicit deletion. This recreation should be prevented
    // by the counter pref.
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
  // Create profile with correct record type and country code based on GeoIP.
  address_data_manager_->AddProfile(
      {info, address_data_manager_->GetDefaultCountryCodeForNewAddress()});

  pref_service_->SetString(prefs::kAutofillNameAndEmailProfileSignature,
                           new_hash);

  // TODO(crbug.com/356845298): Add resetting
  // name_and_email_profile_not_selected_counter if
  // account name email wasn't explicitly deleted.
}

std::string AccountNameEmailStore::HashAccountInfo(
    const AccountInfo& info) const {
  return base::NumberToString(base::PersistentHash(
      base::StrCat({info.full_name, kSeparator, info.email})));
}

}  // namespace autofill
