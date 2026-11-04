// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/test/fake_trusted_vault_client.h"

#include <algorithm>
#include <cstdint>
#include <list>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "crypto/random.h"

namespace trusted_vault {

namespace {

constexpr size_t kSharedKeyLength = 32;

}  // namespace

FakeTrustedVaultClient::CachedKeysPerUser::CachedKeysPerUser() = default;
FakeTrustedVaultClient::CachedKeysPerUser::~CachedKeysPerUser() = default;

FakeTrustedVaultClient::FakeServer::RecoveryMethod::RecoveryMethod(
    const std::vector<uint8_t>& public_key,
    int method_type_hint)
    : public_key(public_key), method_type_hint(method_type_hint) {}

FakeTrustedVaultClient::FakeServer::RecoveryMethod::RecoveryMethod(
    const RecoveryMethod&) = default;
FakeTrustedVaultClient::FakeServer::RecoveryMethod::~RecoveryMethod() = default;

FakeTrustedVaultClient::FakeServer::FakeServer() = default;
FakeTrustedVaultClient::FakeServer::~FakeServer() = default;

void FakeTrustedVaultClient::FakeServer::StoreKeysOnServer(
    const GaiaId& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys) {
  gaia_id_to_keys_[gaia_id] = keys;
}

void FakeTrustedVaultClient::FakeServer::MimicKeyRetrievalByUser(
    const GaiaId& gaia_id,
    FakeTrustedVaultClient* client,
    std::optional<trusted_vault::TrustedVaultUserActionTriggerForUMA> trigger) {
  DCHECK(client);
  DCHECK_NE(0U, gaia_id_to_keys_.count(gaia_id))
      << "StoreKeysOnServer() should have been called for " << gaia_id;

  client->StoreKeys(gaia_id, gaia_id_to_keys_[gaia_id],
                    /*last_key_version=*/
                    static_cast<int>(gaia_id_to_keys_[gaia_id].size()) - 1,
                    trigger);
}

std::vector<std::vector<uint8_t>>
FakeTrustedVaultClient::FakeServer::RequestRotatedKeysFromServer(
    const GaiaId& gaia_id,
    const std::vector<uint8_t>& key_known_by_client) {
  auto it = gaia_id_to_keys_.find(gaia_id);
  if (it == gaia_id_to_keys_.end()) {
    return {};
  }

  const std::vector<std::vector<uint8_t>>& latest_keys = it->second;
  if (!std::ranges::contains(latest_keys, key_known_by_client)) {
    // |key_known_by_client| is invalid or too old: cannot be used to follow
    // key rotation.
    return {};
  }

  return latest_keys;
}

void FakeTrustedVaultClient::FakeServer::AddRecoveryMethod(
    const GaiaId& gaia_id,
    const std::vector<uint8_t>& public_key,
    int method_type_hint) {
  gaia_id_to_recovery_methods_[gaia_id].emplace_back(public_key,
                                                     method_type_hint);
}

std::vector<FakeTrustedVaultClient::FakeServer::RecoveryMethod>
FakeTrustedVaultClient::FakeServer::GetRecoveryMethods(
    const GaiaId& gaia_id) const {
  auto it = gaia_id_to_recovery_methods_.find(gaia_id);
  if (it == gaia_id_to_recovery_methods_.end()) {
    return {};
  }
  return it->second;
}

FakeTrustedVaultClient::FakeTrustedVaultClient(bool auto_complete_requests)
    : auto_complete_requests_(auto_complete_requests) {}

FakeTrustedVaultClient::~FakeTrustedVaultClient() = default;

bool FakeTrustedVaultClient::CompleteAllPendingRequests() {
  if (pending_responses_.empty()) {
    return false;
  }
  // Response callbacks may add new requests, ensure that only those added
  // before this call are completed in the current task.
  size_t original_request_count = pending_responses_.size();
  for (size_t i = 0; i < original_request_count; ++i) {
    std::move(pending_responses_[i]).Run();
  }
  pending_responses_.erase(pending_responses_.begin(),
                           pending_responses_.begin() + original_request_count);
  return true;
}

void FakeTrustedVaultClient::SetIsRecoveryMethodRequired(
    bool is_recovery_method_required) {
  is_recovery_method_required_ = is_recovery_method_required;
  for (auto& observer : observer_list_) {
    // May be a false positive, but observers should handle this well.
    observer.OnTrustedVaultRecoverabilityChanged();
  }
}

void FakeTrustedVaultClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeTrustedVaultClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

std::vector<std::vector<uint8_t>> FakeTrustedVaultClient::GetStoredKeys(
    const GaiaId& gaia_id) const {
  auto it = gaia_id_to_cached_keys_.find(gaia_id);
  if (it == gaia_id_to_cached_keys_.end()) {
    return {};
  }
  return it->second.keys;
}

void FakeTrustedVaultClient::FetchKeys(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)>
        callback) {
  if (auto_complete_requests_) {
    // This posts a task to call CompleteAllPendingRequests(). It is okay to
    // call it now even though `pending_responses_` is not yet updated, because
    // it will be next task.
    PostCompleteAllPendingRequests();
  }

  const GaiaId& gaia_id = account_info.gaia;

  ++fetch_count_;

  CachedKeysPerUser& cached_keys = gaia_id_to_cached_keys_[gaia_id];

  // If there are no keys cached, the only way to bootstrap the client is by
  // going through a retrieval flow, see MimicKeyRetrievalByUser().
  if (cached_keys.keys.empty()) {
    pending_responses_.push_back(base::BindOnce(
        std::move(callback), std::vector<std::vector<uint8_t>>()));
    return;
  }

  // If the locally cached keys are not marked as stale, return them directly.
  if (!cached_keys.marked_as_stale) {
    pending_responses_.push_back(
        base::BindOnce(std::move(callback), cached_keys.keys));
    return;
  }

  // Fetch keys from the server and cache them upon completion.
  pending_responses_.push_back(base::BindOnce(
      &FakeTrustedVaultClient::CompleteStaleFetchKeys,
      weak_ptr_factory_.GetWeakPtr(), gaia_id, std::move(callback)));
}

void FakeTrustedVaultClient::StoreKeys(
    const GaiaId& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version,
    std::optional<trusted_vault::TrustedVaultUserActionTriggerForUMA> trigger) {
  CachedKeysPerUser& cached_keys = gaia_id_to_cached_keys_[gaia_id];
  const bool keys_changed = cached_keys.keys != keys;
  cached_keys.keys = keys;
  cached_keys.marked_as_stale = false;
  if (keys_changed) {
    for (Observer& observer : observer_list_) {
      observer.OnTrustedVaultKeysChanged(trigger);
    }
  }
}

void FakeTrustedVaultClient::MarkLocalKeysAsStale(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> callback) {
  const GaiaId& gaia_id = account_info.gaia;

  ++keys_marked_as_stale_count_;

  CachedKeysPerUser& cached_keys = gaia_id_to_cached_keys_[gaia_id];

  if (cached_keys.keys.empty() || cached_keys.marked_as_stale) {
    // Nothing changed so report `false`.
    std::move(callback).Run(false);
    return;
  }

  // The cache is stale and should be invalidated. Following calls to
  // FetchKeys() will read from the server.
  cached_keys.marked_as_stale = true;
  std::move(callback).Run(true);
}

void FakeTrustedVaultClient::GetIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> callback) {
  ++get_is_recoverablity_degraded_call_count_;
  const bool is_recoverability_degraded =
      is_recovery_method_required_ &&
      server_.GetRecoveryMethods(account_info.gaia).empty();
  pending_responses_.push_back(
      base::BindOnce(std::move(callback), is_recoverability_degraded));
  if (auto_complete_requests_) {
    PostCompleteAllPendingRequests();
  }
}

void FakeTrustedVaultClient::AddTrustedRecoveryMethod(
    const GaiaId& gaia_id,
    const std::vector<uint8_t>& public_key,
    int method_type_hint,
    base::OnceCallback<void(bool)> callback) {
  if (add_trusted_recovery_method_success_) {
    server_.AddRecoveryMethod(gaia_id, public_key, method_type_hint);
  }
  std::move(callback).Run(add_trusted_recovery_method_success_);
  for (auto& observer : observer_list_) {
    // May be a false positive, but observers should handle this well.
    observer.OnTrustedVaultRecoverabilityChanged();
  }
}

void FakeTrustedVaultClient::CanEnroll(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(can_enroll_);
}

void FakeTrustedVaultClient::Enroll(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<uint8_t>&, int)> callback) {
  if (!enroll_success_) {
    std::move(callback).Run(std::vector<uint8_t>(), -1);
    return;
  }
  const std::vector<uint8_t> new_shared_key =
      crypto::RandBytesAsVector(kSharedKeyLength);
  CachedKeysPerUser& cached_keys = gaia_id_to_cached_keys_[account_info.gaia];
  cached_keys.keys.push_back(new_shared_key);
  cached_keys.marked_as_stale = false;
  const int new_key_version = static_cast<int>(cached_keys.keys.size());
  server_.StoreKeysOnServer(account_info.gaia, cached_keys.keys);
  for (auto& observer : observer_list_) {
    observer.OnTrustedVaultKeysChanged(/*trigger=*/std::nullopt);
  }
  std::move(callback).Run(new_shared_key, new_key_version);
}

void FakeTrustedVaultClient::ClearLocalDataForAccount(
    const CoreAccountInfo& account_info) {
  auto it = gaia_id_to_cached_keys_.find(account_info.gaia);
  if (it != gaia_id_to_cached_keys_.end()) {
    gaia_id_to_cached_keys_.erase(it);
  }
}

void FakeTrustedVaultClient::CompleteStaleFetchKeys(
    const GaiaId& gaia_id,
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)>
        callback) {
  auto it = gaia_id_to_cached_keys_.find(gaia_id);
  if (it == gaia_id_to_cached_keys_.end() || it->second.keys.empty()) {
    std::move(callback).Run({});
    return;
  }

  CachedKeysPerUser& cached_keys = it->second;
  if (!cached_keys.marked_as_stale) {
    const std::vector<std::vector<uint8_t>> keys = cached_keys.keys;
    std::move(callback).Run(keys);
    return;
  }

  std::vector<std::vector<uint8_t>> rotated_keys =
      server_.RequestRotatedKeysFromServer(gaia_id, cached_keys.keys.back());
  if (!rotated_keys.empty()) {
    StoreKeys(gaia_id, rotated_keys,
              /*last_key_version=*/static_cast<int>(rotated_keys.size()) - 1,
              /*trigger=*/std::nullopt);
  } else {
    // Key rotation failed (local keys are obsolete); clear cached keys and
    // stale flag without notifying observers.
    cached_keys.keys.clear();
    cached_keys.marked_as_stale = false;
  }
  std::move(callback).Run(rotated_keys);
}

void FakeTrustedVaultClient::PostCompleteAllPendingRequests() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(
                         &FakeTrustedVaultClient::CompleteAllPendingRequests),
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace trusted_vault
