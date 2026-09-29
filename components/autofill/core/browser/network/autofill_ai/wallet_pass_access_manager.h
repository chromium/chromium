// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_WALLET_PASS_ACCESS_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_WALLET_PASS_ACCESS_MANAGER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/wallet/core/browser/network/wallet_http_client.h"

namespace autofill {

// A client interface that allows Autofill AI to communicate with the Wallet
// backend via `wallet::WalletHttpClient`.
// It maps `autofill::EntityInstance`s to `wallet::WalletPass`es and vice versa
// to issue UpsertPass and GetUnmaskedPass requests.
class WalletPassAccessManager : public KeyedService {
 public:
  using UserEligibility = wallet::WalletHttpClient::UserEligibility;

  // Information retrieved from a `GetDetailsForUpsertPass` request.
  struct GetDetailsForUpsertPassResponse {
    LegalMessageLines legal_message_lines;
    std::string context_token;
    UserEligibility user_eligibility = UserEligibility::kUnspecified;

    friend bool operator==(const GetDetailsForUpsertPassResponse&,
                           const GetDetailsForUpsertPassResponse&) = default;
  };

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

  // Callback for `GetDetailsForUpsertPass` requests. On success, it returns
  // the response containing the legal disclosure message lines and context
  // token. On failure, it returns a
  // `wallet::WalletHttpClient::WalletRequestError`.
  using GetDetailsForUpsertPassCallback = base::OnceCallback<void(
      base::expected<GetDetailsForUpsertPassResponse,
                     wallet::WalletHttpClient::WalletRequestError>)>;

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

  // Pre-fetches the legal disclosure messages and context token for
  // `entity_type` in the background (typically called when entering an
  // add settings flow) and populates an internal cache so that a subsequent
  // call to `GetDetailsForUpsertPass` resolves immediately. If a request is
  // already in flight or cached, this is a no-op.
  virtual void PreloadDetailsForUpsertPass(EntityType entity_type) = 0;

  // Synchronously returns the details cached by a previous
  // `PreloadDetailsForUpsertPass` call for `entity_type`, or `std::nullopt` if
  // nothing is cached, e.g. because the preload is still in flight or failed.
  // The returned value never depends on a network request, so it suits UI
  // entry points that must not wait on the Wallet backend.
  //
  // Side effects on a cache hit:
  // - The entry is removed from the cache, since the context token is
  //   single-use.
  // - A new `PreloadDetailsForUpsertPass` is started to refill the cache, so
  //   a later flow also finds cached details.
  virtual std::optional<GetDetailsForUpsertPassResponse>
  ExtractPreloadedDetailsForUpsertPass(EntityType entity_type) = 0;

  // Issues a `GetDetailsForUpsertPass` request to the Wallet backend to fetch
  // legal disclosure messages and a context token for audit logging prior to
  // upserting a public non-readonly pass of type `entity_type`.
  //
  // If cached details are available from a previous
  // `PreloadDetailsForUpsertPass` call, the cached entry is consumed,
  // `callback` is invoked immediately, and a new preload refills the cache.
  //
  // If no cached details are present, a network request is dispatched to the
  // Wallet backend, and the resulting token is handed directly to `callback`
  // without being retained in the cache. If a background preload is already in
  // flight, this request runs concurrently to obtain its own dedicated token.
  virtual void GetDetailsForUpsertPass(
      EntityType entity_type,
      GetDetailsForUpsertPassCallback callback) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_WALLET_PASS_ACCESS_MANAGER_H_
