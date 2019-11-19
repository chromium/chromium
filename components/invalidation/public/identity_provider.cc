// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/identity_provider.h"

#include "base/i18n/time_formatting.h"

namespace invalidation {

IdentityProvider::Observer::~Observer() {}

IdentityProvider::~IdentityProvider() {}

void IdentityProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void IdentityProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

IdentityProvider::IdentityProvider() {}

void IdentityProvider::ProcessRefreshTokenUpdateForAccount(
    const CoreAccountId& account_id) {
  if (account_id != GetActiveAccountId()) {
    diagnostic_info_.token_update_for_not_active_account_count++;
    return;
  }
  diagnostic_info_.account_token_updated = base::Time::Now();
  for (auto& observer : observers_)
    observer.OnActiveAccountRefreshTokenUpdated();
}

void IdentityProvider::ProcessRefreshTokenRemovalForAccount(
    const CoreAccountId& account_id) {
  if (account_id != GetActiveAccountId()) {
    diagnostic_info_.token_removal_for_not_active_account_count++;
    return;
  }
  for (auto& observer : observers_)
    observer.OnActiveAccountRefreshTokenRemoved();
}

void IdentityProvider::FireOnActiveAccountLogin() {
  for (auto& observer : observers_)
    observer.OnActiveAccountLogin();
}

void IdentityProvider::FireOnActiveAccountLogout() {
  for (auto& observer : observers_)
    observer.OnActiveAccountLogout();
}

void IdentityProvider::RequestDetailedStatus(
    base::RepeatingCallback<void(const base::DictionaryValue&)> return_callback)
    const {
  return_callback.Run(diagnostic_info_.CollectDebugData());
}

IdentityProvider::Diagnostics::Diagnostics() {}

base::DictionaryValue IdentityProvider::Diagnostics::CollectDebugData() const {
  base::DictionaryValue status;

  status.SetInteger("IdentityProvider.token-removal-for-not-active-account",
                    token_removal_for_not_active_account_count);
  status.SetInteger("IdentityProvider.token-update-for-not-active-account",
                    token_update_for_not_active_account_count);
  status.SetString("IdentityProvider.account-token-updated",
                   base::TimeFormatShortDateAndTime(account_token_updated));
  return status;
}

}  // namespace invalidation
