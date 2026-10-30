// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_PENDING_EPHEMERAL_CONTAINER_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_PENDING_EPHEMERAL_CONTAINER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync_tab_context/container_id.h"
#include "components/sync_tab_context/ephemeral_key_fetcher.h"
#include "components/sync_tab_context/upload_outcome.h"
#include "net/base/backoff_entry.h"

namespace syncer {
class AgileSymmetricKeySet;
}  // namespace syncer

namespace sync_tab_context {

class TabContextItemSyncBridge;

// Represents a short-lived ephemeral container while its initial server-minted
// ephemeral key fetch is in flight.
//
// An instance of `PendingEphemeralContainer` is created when a container is
// requested without an active Trusted Vault passphrase. It immediately
// generates a random container `AgileSymmetricKeySet` so that callers can start
// compressing and encrypting page context right away, while asynchronously
// fetching an ephemeral key from `EphemeralKeyFetcher` (retrying with
// exponential backoff on transient failures) to build a
// `TabContextContainerAccessToken`.
//
// Any encrypted item uploads (`QueueUploadItem()`) or access token requests
// (`QueueGetContainerAccessToken()`) that arrive while the fetch is in flight
// are queued. Once the fetch finishes (either succeeding and building an access
// token, or exhausting all retry attempts), `CompletionCallback` is invoked
// synchronously before any queued item uploads or token callbacks are flushed,
// allowing the owner (`TabContextSyncServiceImpl`) to persist the container in
// `TabContextContainerSyncBridge` and destroy this `PendingEphemeralContainer`.
class PendingEphemeralContainer {
 public:
  // Invoked for a queued `QueueGetContainerAccessToken()` request once the
  // ephemeral key fetch finishes. Receives the serialized
  // `TabContextContainerAccessToken` on success, or `std::nullopt` if the fetch
  // permanently fails or `this` is destroyed before completion.
  using AccessTokenCallback =
      base::OnceCallback<void(std::optional<std::string>)>;

  // Invoked for a queued `QueueUploadItem()` request once the item upload
  // completes. Receives the final `UploadOutcome` from `item_bridge_` if the
  // ephemeral key fetch succeeds and the item is forwarded, or
  // `UploadOutcome::kFailed` if the fetch permanently fails or `this` is
  // destroyed before completion.
  using UploadCallback = base::OnceCallback<void(UploadOutcome)>;

  // Invoked when the ephemeral key fetch finishes. On success,
  // `container_access_token` is populated and non-empty. On permanent failure,
  // `container_access_token` is `std::nullopt`.
  // The callback is allowed to destroy `this`.
  using CompletionCallback = base::OnceCallback<void(
      std::optional<std::string> container_access_token)>;

  // Destroys `pending_container` and returns ownership of its encryption key
  // set. `pending_container` must not be null.
  static std::unique_ptr<syncer::AgileSymmetricKeySet> DestroyAndTakeKey(
      std::unique_ptr<PendingEphemeralContainer> pending_container);

  // Generates a random container encryption key immediately. `Start()` must be
  // called after construction to begin fetching the ephemeral key.
  // `ephemeral_key_fetcher` and `item_bridge` must not be null and must outlive
  // `this`; `item_bridge` must also outlive any `CompletionCallback` invocation
  // (since queued item uploads are flushed immediately after
  // `CompletionCallback` returns).
  PendingEphemeralContainer(const ContainerId& container_id,
                            EphemeralKeyFetcher* ephemeral_key_fetcher,
                            TabContextItemSyncBridge* item_bridge);
  PendingEphemeralContainer(const PendingEphemeralContainer&) = delete;
  PendingEphemeralContainer& operator=(const PendingEphemeralContainer&) =
      delete;
  ~PendingEphemeralContainer();

  // Starts fetching an ephemeral key via `ephemeral_key_fetcher_` and registers
  // `completion_callback` to be invoked when the fetch finishes. Must be called
  // at most once, and `completion_callback` must be non-null.
  void Start(CompletionCallback completion_callback);

  // Returns the generated encryption key set for this pending container.
  // Guaranteed to be non-empty.
  const syncer::AgileSymmetricKeySet& GetEncryptionKey() const;

  // Queues an encrypted item upload to be forwarded to `item_bridge_` (or
  // failed with `UploadOutcome::kFailed` if the fetch permanently fails)
  // immediately after `Start()`'s `completion_callback` returns, or failed with
  // `UploadOutcome::kFailed` if `this` is destroyed before completion.
  void QueueUploadItem(const std::string& item_id,
                       sync_pb::EncryptedData encrypted_content,
                       UploadCallback callback);

  // Queues `callback` to be invoked with the container access token (or
  // `std::nullopt` on permanent fetch failure) immediately after `Start()`'s
  // `completion_callback` returns, or with `std::nullopt` if `this` is
  // destroyed before completion.
  void QueueGetContainerAccessToken(AccessTokenCallback callback);

 private:
  struct QueuedItemUpload;

  void StartFetchAttempt();
  void OnEphemeralKeyFetched(std::optional<EphemeralKeyFetcher::Result> result);
  void FailAndNotifyAll();
  [[nodiscard]] base::ScopedClosureRunner TakeQueuedOperationsAsScopedFlusher(
      std::optional<std::string> access_token);

  SEQUENCE_CHECKER(sequence_checker_);

  const ContainerId container_id_;
  const raw_ref<EphemeralKeyFetcher> ephemeral_key_fetcher_;
  const raw_ref<TabContextItemSyncBridge> item_bridge_;

  std::unique_ptr<syncer::AgileSymmetricKeySet> key_set_;
  CompletionCallback completion_callback_;
  net::BackoffEntry backoff_entry_;
  base::OneShotTimer retry_timer_;

  std::vector<QueuedItemUpload> queued_item_uploads_;
  std::vector<AccessTokenCallback> queued_get_token_callbacks_;

  base::WeakPtrFactory<PendingEphemeralContainer> weak_ptr_factory_{this};
};

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_PENDING_EPHEMERAL_CONTAINER_H_
