// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_WALLET_PASS_ACCESS_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_WALLET_PASS_ACCESS_MANAGER_H_

#include <optional>

#include "base/functional/callback.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {

// A client interface that allows Autofill AI to communicate with the Wallet
// backend via `wallet::WalletHttpClient`.
// It maps `autofill::EntityInstance`s to `wallet::WalletPass`es and vice versa
// to issue UpsertPass and GetUnmaskedPass requests.
class WalletPassAccessManager : public KeyedService {
 public:
  // Callback for save and update requests. On success, it returns
  // the masked `EntityInstance` as it is stored in the Wallet backend
  // (including its `id`). Returns `std::nullopt` on failure.
  using UpsertEntityInstanceCallback =
      base::OnceCallback<void(std::optional<EntityInstance>)>;

  // Callback for `GetUnmaskedWalletEntityInstance` requests. On success, it
  // returns the unmasked `EntityInstance` corresponding to the requested
  // `entity_id`. Returns `std::nullopt` on failure.
  using GetUnmaskedEntityInstanceCallback =
      base::OnceCallback<void(std::optional<EntityInstance>)>;

  // Issues an save request to the Wallet backend for the given `entity`.
  // Notably, the returned entity will always have a new entity id.
  // `session_id` identifies the consent that was logged through
  // `consent_auditor::ConsentAuditor::RecordWalletPrivatePassConsent()` prior
  // to the save.
  virtual void SaveWalletEntityInstance(
      const EntityInstance& entity,
      const consent_auditor::ConsentAuditor::SessionId& session_id,
      UpsertEntityInstanceCallback callback) = 0;

  // Issues an update request to the Wallet backend for the given `entity`.
  virtual void UpdateWalletEntityInstance(
      const EntityInstance& entity,
      UpsertEntityInstanceCallback callback) = 0;

  // Issues a GetUnmaskedPass request to the Wallet backend for the given
  // `entity_id`.
  virtual void GetUnmaskedWalletEntityInstance(
      const EntityInstance::EntityId& entity_id,
      GetUnmaskedEntityInstanceCallback callback) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_WALLET_PASS_ACCESS_MANAGER_H_
