// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager_impl.h"

#include <memory>
#include <utility>

#include "base/notimplemented.h"
#include "components/wallet/core/browser/network/wallet_http_client.h"

namespace autofill {

WalletPassAccessManagerImpl::WalletPassAccessManagerImpl(
    std::unique_ptr<wallet::WalletHttpClient> http_client)
    : http_client_(std::move(http_client)) {}

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
    const EntityInstance::EntityId& entity_id,
    GetUnmaskedEntityInstanceCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace autofill
