// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/manta/fake_manta_service_provider.h"

#include "base/check.h"

namespace ash {

FakeMantaServiceProvider::FakeMantaServiceProvider() = default;

FakeMantaServiceProvider::~FakeMantaServiceProvider() = default;

void FakeMantaServiceProvider::SetMantaServiceForAccount(
    const AccountId& account_id,
    manta::MantaService* manta_service) {
  if (manta_service) {
    CHECK(manta_services_.try_emplace(account_id, manta_service).second);
  } else {
    manta_services_.erase(account_id);
  }
}

manta::MantaService* FakeMantaServiceProvider::Find(
    const AccountId& account_id) {
  auto it = manta_services_.find(account_id);
  return it == manta_services_.end() ? nullptr : it->second;
}

}  // namespace ash
