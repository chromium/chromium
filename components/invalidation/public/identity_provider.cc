// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/identity_provider.h"

#include "base/i18n/time_formatting.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"

namespace invalidation {

IdentityProvider::IdentityProvider() = default;

IdentityProvider::~IdentityProvider() = default;

void IdentityProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void IdentityProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

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
    base::RepeatingCallback<void(base::Value::Dict)> return_callback) const {
  return_callback.Run(diagnostic_info_.CollectDebugData());
}

IdentityProvider::Diagnostics::Diagnostics() = default;

base::Value::Dict IdentityProvider::Diagnostics::CollectDebugData() const {
  base::Value::Dict status;

  status.SetByDottedPath(
      "IdentityProvider.token-removal-for-not-active-account",
      token_removal_for_not_active_account_count);
  status.SetByDottedPath("IdentityProvider.token-update-for-not-active-account",
                         token_update_for_not_active_account_count);
  status.SetByDottedPath(
      "IdentityProvider.account-token-updated",
      base::TimeFormatShortDateAndTime(account_token_updated));
  return status;
}

}  // namespace invalidation
