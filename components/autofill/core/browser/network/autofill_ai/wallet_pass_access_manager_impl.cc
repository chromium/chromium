// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager_impl.h"

#include "base/notimplemented.h"

namespace autofill {

WalletPassAccessManagerImpl::~WalletPassAccessManagerImpl() = default;

void WalletPassAccessManagerImpl::SaveWalletEntityInstance(
    const EntityInstance& entity,
    UpsertEntityInstanceCallback callback) {
  NOTIMPLEMENTED();
}

void WalletPassAccessManagerImpl::UpdateWalletEntityInstance(
    const EntityInstance& entity,
    UpsertEntityInstanceCallback callback) {
  NOTIMPLEMENTED();
}

void WalletPassAccessManagerImpl::GetUnmaskedWalletEntityInstance(
    const EntityId& entity_id,
    GetUnmaskedEntityInstanceCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace autofill
