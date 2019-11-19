// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/blacklisted_credentials_cleaner.h"

#include <set>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

BlacklistedCredentialsCleaner::BlacklistedCredentialsCleaner(
    scoped_refptr<PasswordStore> store,
    PrefService* prefs)
    : store_(std::move(store)), prefs_(prefs) {}

BlacklistedCredentialsCleaner::~BlacklistedCredentialsCleaner() = default;

bool BlacklistedCredentialsCleaner::NeedsCleaning() {
  const bool needs_cleaning =
      !prefs_->GetBoolean(prefs::kBlacklistedCredentialsNormalized);
  base::UmaHistogramBoolean(
      "PasswordManager.BlacklistedSites.NeedNormalization", needs_cleaning);
  return needs_cleaning;
}

void BlacklistedCredentialsCleaner::StartCleaning(Observer* observer) {
  observer_ = observer;
  store_->GetBlacklistLogins(this);
}

void BlacklistedCredentialsCleaner::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  std::set<std::string> signon_realms;
  for (const auto& result : results) {
    DCHECK(result->blacklisted_by_user);
    if (!signon_realms.insert(result->signon_realm).second) {
      // Insertion failed due to already existing signon realm, so we remove the
      // duplicated entry from the store.
      store_->RemoveLogin(*result);
      continue;
    }

    autofill::PasswordForm blacklisted =
        password_manager_util::MakeNormalizedBlacklistedForm(
            PasswordStore::FormDigest(*result));
    blacklisted.date_created = result->date_created;
    // In case |blacklisted| and |result| differ, update the store.
    if (!ArePasswordFormUniqueKeysEqual(blacklisted, *result))
      store_->UpdateLoginWithPrimaryKey(blacklisted, *result);
    else if (blacklisted != *result)
      store_->UpdateLogin(blacklisted);
  }

  prefs_->SetBoolean(prefs::kBlacklistedCredentialsNormalized, true);
  observer_->CleaningCompleted();
}

}  // namespace password_manager
