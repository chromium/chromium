// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/identity_provider.h"

#include "base/observer_list.h"

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
    return;
  }
  for (auto& observer : observers_)
    observer.OnActiveAccountRefreshTokenUpdated();
}

void IdentityProvider::FireOnActiveAccountLogin() {
  for (auto& observer : observers_)
    observer.OnActiveAccountLogin();
}

void IdentityProvider::FireOnActiveAccountLogout() {
  for (auto& observer : observers_)
    observer.OnActiveAccountLogout();
}

}  // namespace invalidation
