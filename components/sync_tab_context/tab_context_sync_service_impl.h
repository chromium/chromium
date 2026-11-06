// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_SYNC_SERVICE_IMPL_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_SYNC_SERVICE_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync_tab_context/container_id.h"
#include "components/sync_tab_context/ephemeral_key_fetcher.h"
#include "components/sync_tab_context/tab_context_sync_service.h"
#include "components/sync_tab_context/upload_outcome.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace syncer {
class AgileSymmetricKeySet;
class DataTypeControllerDelegate;
}  // namespace syncer

namespace sync_tab_context {

class PendingEphemeralContainer;
class TabContextSyncClient;
class TabContextItemSyncBridge;
class TabContextContainerSyncBridge;

class TabContextSyncServiceImpl : public TabContextSyncService {
 public:
  TabContextSyncServiceImpl(
      std::unique_ptr<TabContextSyncClient> client,
      syncer::OnceDataTypeStoreFactory store_factory,
      std::unique_ptr<EphemeralKeyFetcher> ephemeral_key_fetcher,
      base::RepeatingClosure dump_stack);
  TabContextSyncServiceImpl(const TabContextSyncServiceImpl&) = delete;
  TabContextSyncServiceImpl& operator=(const TabContextSyncServiceImpl&) =
      delete;
  ~TabContextSyncServiceImpl() override;

  // TabContextSyncService implementation.
  std::optional<ContainerId> CreateContainer() override;
  std::optional<ContainerId> CreateContainerPossiblyUnencryptedEphemeral()
      override;
  void UploadPageContext(
      const ContainerId& container_id,
      const std::string& entry_id,
      std::string page_context,
      base::OnceCallback<void(UploadOutcome)> callback) override;
  void GetContainerAccessToken(
      const ContainerId& container_id,
      base::OnceCallback<void(std::optional<std::string>)> cb) override;
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSyncControllerDelegateForContainer() override;
  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetSyncControllerDelegateForItem() override;
  bool IsActiveForTesting() const override;

 private:
  // Returns the encryption key set for `container_id` from either
  // `container_bridge_` or `pending_ephemeral_containers_`, or `nullptr` if not
  // found. The returned pointer is only guaranteed to remain valid while
  // neither `container_bridge_` nor `pending_ephemeral_containers_` is mutated
  // (i.e. it must not be retained across tasks).
  const syncer::AgileSymmetricKeySet* GetEncryptionKeyForContainer(
      const ContainerId& container_id) const;

  // Called when a `PendingEphemeralContainer` finishes fetching its initial
  // ephemeral key (or permanently fails).
  void OnPendingEphemeralContainerCompleted(
      const ContainerId& container_id,
      std::optional<std::string> pre_minted_access_token);

  // Called asynchronously when `backend_task_runner_` finishes compressing and
  // encrypting the page context.
  void OnPageContextCompressedAndEncrypted(
      const ContainerId& container_id,
      const std::string& entry_id,
      base::OnceCallback<void(UploadOutcome)> callback,
      std::optional<sync_pb::EncryptedData> encrypted_content);

  // Called asynchronously when `ephemeral_key_fetcher_` finishes fetching an
  // ephemeral key for a `GetContainerAccessToken()` request. Encrypts the
  // container's key set and runs `cb`.
  void OnEphemeralKeyFetchedForAccessToken(
      const ContainerId& container_id,
      base::OnceCallback<void(std::optional<std::string>)> cb,
      std::optional<EphemeralKeyFetcher::Result> result);

  // Attempts to create a container and upload `payload` for testing when
  // `--sync-tab-context-test-upload` is specified, retrying periodically if
  // the service is not ready yet.
  void TryTestUpload(std::optional<std::string> payload);

  const std::unique_ptr<TabContextSyncClient> client_;
  const std::unique_ptr<TabContextContainerSyncBridge> container_bridge_;
  const std::unique_ptr<TabContextItemSyncBridge> item_bridge_;
  const std::unique_ptr<EphemeralKeyFetcher> ephemeral_key_fetcher_;
  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  absl::flat_hash_map<ContainerId,
                      std::unique_ptr<PendingEphemeralContainer>,
                      ContainerIdHash>
      pending_ephemeral_containers_;

  base::WeakPtrFactory<TabContextSyncServiceImpl> weak_ptr_factory_{this};
};

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_SYNC_SERVICE_IMPL_H_
