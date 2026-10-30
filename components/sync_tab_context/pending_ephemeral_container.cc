// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/pending_ephemeral_container.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/sync/model/crypto/agile_symmetric_key_set.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync_tab_context/build_container_access_token.h"
#include "components/sync_tab_context/container_id.h"
#include "components/sync_tab_context/ephemeral_key_fetcher.h"
#include "components/sync_tab_context/tab_context_item_sync_bridge.h"
#include "components/sync_tab_context/upload_outcome.h"
#include "net/base/backoff_entry.h"

namespace sync_tab_context {

namespace {

// Maximum number of attempts (initial attempt + retries) to fetch an ephemeral
// key before permanently failing the pending container.
constexpr int kMaxFetchAttempts = 5;

constexpr net::BackoffEntry::Policy kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms (1 second).
    1000,

    // Factor by which the waiting time will be multiplied.
    2.0,

    // Fuzzing percentage.
    0.2,

    // Maximum amount of time we are willing to delay our request in ms (30s).
    30 * 1000,

    // Time to keep an entry from being discarded even when it has no
    // significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

std::unique_ptr<syncer::AgileSymmetricKeySet> CreateRandomKeySet() {
  std::unique_ptr<syncer::AgileSymmetricKeySet> key_set =
      syncer::AgileSymmetricKeySet::CreateEmpty();
  key_set->RotatePrimaryToNewlyGeneratedRandomKey();
  return key_set;
}

}  // namespace

struct PendingEphemeralContainer::QueuedItemUpload {
  QueuedItemUpload(std::string item_id,
                   sync_pb::EncryptedData encrypted_content,
                   UploadCallback callback)
      : item_id(std::move(item_id)),
        encrypted_content(std::move(encrypted_content)),
        callback(std::move(callback)) {}
  QueuedItemUpload(QueuedItemUpload&&) = default;
  QueuedItemUpload& operator=(QueuedItemUpload&&) = default;
  ~QueuedItemUpload() = default;

  std::string item_id;
  sync_pb::EncryptedData encrypted_content;
  UploadCallback callback;
};

// static
std::unique_ptr<syncer::AgileSymmetricKeySet>
PendingEphemeralContainer::DestroyAndTakeKey(
    std::unique_ptr<PendingEphemeralContainer> pending_container) {
  CHECK(pending_container);
  DCHECK_CALLED_ON_VALID_SEQUENCE(pending_container->sequence_checker_);
  return std::move(pending_container->key_set_);
}

PendingEphemeralContainer::PendingEphemeralContainer(
    const ContainerId& container_id,
    EphemeralKeyFetcher* ephemeral_key_fetcher,
    TabContextItemSyncBridge* item_bridge)
    : container_id_(container_id),
      ephemeral_key_fetcher_(CHECK_DEREF(ephemeral_key_fetcher)),
      item_bridge_(CHECK_DEREF(item_bridge)),
      key_set_(CreateRandomKeySet()),
      backoff_entry_(&kBackoffPolicy) {
  CHECK(container_id_.value().is_valid());
  CHECK(key_set_);
  CHECK_GT(key_set_->size(), 0u);
}

PendingEphemeralContainer::~PendingEphemeralContainer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::ignore =
      TakeQueuedOperationsAsScopedFlusher(/*access_token=*/std::nullopt);
}

void PendingEphemeralContainer::Start(CompletionCallback completion_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(completion_callback);
  CHECK(!completion_callback_);
  CHECK_EQ(backoff_entry_.failure_count(), 0);
  completion_callback_ = std::move(completion_callback);
  StartFetchAttempt();
}

const syncer::AgileSymmetricKeySet&
PendingEphemeralContainer::GetEncryptionKey() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return *key_set_;
}

void PendingEphemeralContainer::QueueUploadItem(
    const std::string& item_id,
    sync_pb::EncryptedData encrypted_content,
    UploadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(completion_callback_);
  CHECK(!item_id.empty());
  CHECK(callback);
  queued_item_uploads_.emplace_back(item_id, std::move(encrypted_content),
                                    std::move(callback));
}

void PendingEphemeralContainer::QueueGetContainerAccessToken(
    AccessTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(completion_callback_);
  CHECK(callback);
  queued_get_token_callbacks_.push_back(std::move(callback));
}

void PendingEphemeralContainer::StartFetchAttempt() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ephemeral_key_fetcher_->FetchEphemeralKey(
      base::BindOnce(&PendingEphemeralContainer::OnEphemeralKeyFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PendingEphemeralContainer::OnEphemeralKeyFetched(
    std::optional<EphemeralKeyFetcher::Result> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<std::string> access_token;
  if (result.has_value() && result->ephemeral_key) {
    access_token = BuildContainerAccessToken(*key_set_, std::move(*result));
  }

  if (!access_token.has_value()) {
    backoff_entry_.InformOfRequest(/*succeeded=*/false);
    if (backoff_entry_.failure_count() >= kMaxFetchAttempts) {
      FailAndNotifyAll();
      return;
    }
    retry_timer_.Start(
        FROM_HERE, backoff_entry_.GetTimeUntilRelease(),
        base::BindOnce(&PendingEphemeralContainer::StartFetchAttempt,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  backoff_entry_.InformOfRequest(/*succeeded=*/true);

  // Run `completion_callback_` before flushing queued item uploads and access
  // token callbacks (which run when `flusher` goes out of scope). This ensures
  // the owner (`TabContextSyncServiceImpl`) removes `this` from its pending map
  // and persists the container in `TabContextContainerSyncBridge` before items
  // are committed to `item_bridge_` or caller callbacks re-entrantly query the
  // service.
  base::ScopedClosureRunner flusher =
      TakeQueuedOperationsAsScopedFlusher(*access_token);
  std::move(completion_callback_).Run(std::move(*access_token));
  // `this` may be destroyed here.
}

void PendingEphemeralContainer::FailAndNotifyAll() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // As in `OnEphemeralKeyFetched()`, run `completion_callback_` before flushing
  // queued callbacks (when `flusher` goes out of scope) so the owner removes
  // `this` from its pending map before caller callbacks run.
  base::ScopedClosureRunner flusher =
      TakeQueuedOperationsAsScopedFlusher(/*access_token=*/std::nullopt);
  std::move(completion_callback_).Run(std::nullopt);
  // `this` may be destroyed here.
}

base::ScopedClosureRunner
PendingEphemeralContainer::TakeQueuedOperationsAsScopedFlusher(
    std::optional<std::string> access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Move `queued_item_uploads_` and `queued_get_token_callbacks_` into the
  // returned closure before `completion_callback_` is invoked, both to avoid
  // failure notifications from `~PendingEphemeralContainer()` if
  // `completion_callback_` destroys `this`, and so queued operations are
  // flushed on scope exit without code executing after `this` may have been
  // destroyed.
  base::ScopedClosureRunner flusher(base::BindOnce(
      [](const ContainerId& container_id,
         base::raw_ref<TabContextItemSyncBridge> item_bridge,
         std::vector<QueuedItemUpload> queued_item_uploads,
         std::vector<AccessTokenCallback> queued_get_token_callbacks,
         std::optional<std::string> access_token) {
        for (QueuedItemUpload& upload : queued_item_uploads) {
          if (access_token.has_value()) {
            item_bridge->UploadItem(container_id, upload.item_id,
                                    std::move(upload.encrypted_content),
                                    std::move(upload.callback));
          } else {
            std::move(upload.callback).Run(UploadOutcome::kFailed);
          }
        }
        for (AccessTokenCallback& callback : queued_get_token_callbacks) {
          std::move(callback).Run(access_token);
        }
      },
      container_id_, item_bridge_, std::move(queued_item_uploads_),
      std::move(queued_get_token_callbacks_), std::move(access_token)));
  queued_item_uploads_.clear();
  queued_get_token_callbacks_.clear();
  return flusher;
}

}  // namespace sync_tab_context
