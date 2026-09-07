// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/signin/fake_identity_manager_provider.h"

#include "base/check.h"

namespace ash {

FakeIdentityManagerProvider::FakeIdentityManagerProvider() = default;

FakeIdentityManagerProvider::~FakeIdentityManagerProvider() = default;

void FakeIdentityManagerProvider::SetIdentityManagerForAccount(
    const AccountId& account_id,
    signin::IdentityManager* identity_manager) {
  if (identity_manager) {
    auto [it, inserted] =
        identity_managers_.try_emplace(account_id, identity_manager);
    CHECK(inserted);
  } else {
    identity_managers_.erase(account_id);
  }
}

signin::IdentityManager* FakeIdentityManagerProvider::Find(
    const AccountId& account_id) {
  auto it = identity_managers_.find(account_id);
  return it == identity_managers_.end() ? nullptr : it->second;
}

}  // namespace ash
