// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/tab_context_sync_service_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/crypto/agile_symmetric_key_set.h"
#include "components/sync/protocol/encrypted_tab_context_item_specifics.pb.h"
#include "components/sync_tab_context/build_container_access_token.h"
#include "components/sync_tab_context/ephemeral_key_fetcher.h"
#include "components/sync_tab_context/pending_ephemeral_container.h"
#include "components/sync_tab_context/tab_context_container_sync_bridge.h"
#include "components/sync_tab_context/tab_context_item_sync_bridge.h"
#include "components/sync_tab_context/tab_context_sync_client.h"
#include "components/sync_tab_context/upload_outcome.h"
#include "third_party/zlib/google/compression_utils.h"

namespace sync_tab_context {

namespace {

constexpr std::string_view kTestUploadFilePrefix = "file:";
constexpr char kTestUploadEntryId[] = "test_entry";
constexpr base::TimeDelta kTestUploadRetryDelay = base::Seconds(1);

// Executed on a background thread pool sequence to avoid blocking the UI
// thread when reading a file specified via `--sync-tab-context-test-upload`.
std::optional<std::string> ResolveTestUploadPayloadOnBackgroundThread() {
  const std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueUTF8(
          kSyncTabContextTestUploadSwitch);
  const std::optional<std::string_view> file_path_str =
      base::RemovePrefix(switch_value, kTestUploadFilePrefix);
  if (!file_path_str.has_value()) {
    return switch_value;
  }

  const base::FilePath file_path =
      base::FilePath::FromUTF8Unsafe(*file_path_str);
  std::string contents;
  if (!base::ReadFileToString(file_path, &contents)) {
    LOG(ERROR) << "Failed to read file for --"
               << kSyncTabContextTestUploadSwitch << ": " << file_path;
    return std::nullopt;
  }
  return contents;
}

// Executed on a background thread pool sequence to avoid blocking the UI
// thread during compression.
std::optional<std::string> CompressAndSerializePageContextOnBackgroundThread(
    std::string page_context) {
  std::string compressed_page_context;
  if (!compression::GzipCompress(base::as_byte_span(page_context),
                                 &compressed_page_context)) {
    return std::nullopt;
  }
  page_context.clear();
  page_context.shrink_to_fit();

  sync_pb::TabContextItemContent item_content;
  item_content.set_gzip_compressed_data(std::move(compressed_page_context));
  return item_content.SerializeAsString();
}

// Executed on a background thread pool sequence to avoid blocking the UI
// thread during compression and encryption.
std::optional<sync_pb::EncryptedData>
CompressAndEncryptPageContextOnBackgroundThread(
    sync_pb::AgileSymmetricKeySet key_set_proto,
    std::string page_context) {
  std::unique_ptr<syncer::AgileSymmetricKeySet> key_set =
      syncer::AgileSymmetricKeySet::FromProto(key_set_proto);
  if (!key_set) {
    return std::nullopt;
  }

  const std::optional<std::string> serialized_content =
      CompressAndSerializePageContextOnBackgroundThread(
          std::move(page_context));
  if (!serialized_content) {
    return std::nullopt;
  }

  return key_set->Encrypt(base::as_byte_span(*serialized_content));
}

}  // namespace

TabContextSyncServiceImpl::TabContextSyncServiceImpl(
    std::unique_ptr<TabContextSyncClient> client,
    syncer::OnceDataTypeStoreFactory store_factory,
    std::unique_ptr<EphemeralKeyFetcher> ephemeral_key_fetcher,
    base::RepeatingClosure dump_stack)
    : client_(std::move(client)),
      container_bridge_(std::make_unique<TabContextContainerSyncBridge>(
          std::move(store_factory),
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::ENCRYPTED_TAB_CONTEXT_CONTAINER,
              dump_stack))),
      item_bridge_(std::make_unique<TabContextItemSyncBridge>(
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::ENCRYPTED_TAB_CONTEXT_ITEM,
              dump_stack))),
      ephemeral_key_fetcher_(std::move(ephemeral_key_fetcher)),
      backend_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  CHECK(client_);
  CHECK(ephemeral_key_fetcher_);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kSyncTabContextTestUploadSwitch)) {
    backend_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&ResolveTestUploadPayloadOnBackgroundThread),
        base::BindOnce(&TabContextSyncServiceImpl::TryTestUpload,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

TabContextSyncServiceImpl::~TabContextSyncServiceImpl() = default;

std::optional<ContainerId> TabContextSyncServiceImpl::CreateContainer() {
  if (client_->GetPassphraseType() !=
      syncer::PassphraseType::kTrustedVaultPassphrase) {
    return std::nullopt;
  }
  if (!container_bridge_->IsTrackingMetadata()) {
    return std::nullopt;
  }

  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  std::unique_ptr<syncer::AgileSymmetricKeySet> key_set =
      syncer::AgileSymmetricKeySet::CreateEmpty();
  key_set->RotatePrimaryToNewlyGeneratedRandomKey();

  container_bridge_->CreateContainer(container_id, std::move(key_set),
                                     /*pre_minted_access_token=*/std::nullopt);
  return container_id;
}

std::optional<ContainerId>
TabContextSyncServiceImpl::CreateContainerPossiblyUnencryptedEphemeral() {
  const std::optional<syncer::PassphraseType> passphrase_type =
      client_->GetPassphraseType();
  if (!passphrase_type.has_value()) {
    return std::nullopt;
  }
  if (*passphrase_type == syncer::PassphraseType::kTrustedVaultPassphrase) {
    return CreateContainer();
  }
  if (!container_bridge_->IsTrackingMetadata()) {
    return std::nullopt;
  }

  const ContainerId container_id(base::Uuid::GenerateRandomV4());
  auto pending_container = std::make_unique<PendingEphemeralContainer>(
      container_id, ephemeral_key_fetcher_.get(), item_bridge_.get());
  // Populate `pending_ephemeral_containers_` before calling `Start()`, as
  // `Start()` may invoke `OnPendingEphemeralContainerCompleted()`
  // synchronously (e.g. in tests), which erases the entry from the map.
  PendingEphemeralContainer* pending_container_ptr = pending_container.get();
  pending_ephemeral_containers_[container_id] = std::move(pending_container);
  pending_container_ptr->Start(base::BindOnce(
      &TabContextSyncServiceImpl::OnPendingEphemeralContainerCompleted,
      weak_ptr_factory_.GetWeakPtr(), container_id));
  return container_id;
}

const syncer::AgileSymmetricKeySet*
TabContextSyncServiceImpl::GetEncryptionKeyForContainer(
    const ContainerId& container_id) const {
  if (const syncer::AgileSymmetricKeySet* key_set =
          container_bridge_->GetStoredEncryptionKeyForContainer(container_id)) {
    return key_set;
  }
  if (const PendingEphemeralContainer* pending_container =
          base::FindPtrOrNull(pending_ephemeral_containers_, container_id)) {
    return &pending_container->GetEncryptionKey();
  }
  return nullptr;
}

void TabContextSyncServiceImpl::OnPendingEphemeralContainerCompleted(
    const ContainerId& container_id,
    std::optional<std::string> pre_minted_access_token) {
  std::unique_ptr<PendingEphemeralContainer> pending_container =
      std::move(pending_ephemeral_containers_[container_id]);
  pending_ephemeral_containers_.erase(container_id);
  CHECK(pending_container);

  std::unique_ptr<syncer::AgileSymmetricKeySet> key_set =
      PendingEphemeralContainer::DestroyAndTakeKey(
          std::move(pending_container));
  if (!pre_minted_access_token.has_value() ||
      !container_bridge_->IsTrackingMetadata()) {
    return;
  }

  container_bridge_->CreateContainer(container_id, std::move(key_set),
                                     std::move(pre_minted_access_token));
}

void TabContextSyncServiceImpl::UploadPageContext(
    const ContainerId& container_id,
    const std::string& entry_id,
    std::string page_context,
    base::OnceCallback<void(UploadOutcome)> callback) {
  if (!container_id.value().is_valid() || entry_id.empty()) {
    std::move(callback).Run(UploadOutcome::kFailed);
    return;
  }

  const syncer::AgileSymmetricKeySet* key_set =
      GetEncryptionKeyForContainer(container_id);
  if (!key_set || key_set->size() == 0) {
    std::move(callback).Run(UploadOutcome::kFailed);
    return;
  }

  // Compress and encrypt on a sequenced background runner to avoid blocking the
  // UI thread while preserving FIFO ordering across uploads.
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CompressAndEncryptPageContextOnBackgroundThread,
                     key_set->ToProto(), std::move(page_context)),
      base::BindOnce(
          &TabContextSyncServiceImpl::OnPageContextCompressedAndEncrypted,
          weak_ptr_factory_.GetWeakPtr(), container_id, entry_id,
          std::move(callback)));
}

void TabContextSyncServiceImpl::OnPageContextCompressedAndEncrypted(
    const ContainerId& container_id,
    const std::string& entry_id,
    base::OnceCallback<void(UploadOutcome)> callback,
    std::optional<sync_pb::EncryptedData> encrypted_content) {
  if (!encrypted_content) {
    std::move(callback).Run(UploadOutcome::kFailed);
    return;
  }

  if (PendingEphemeralContainer* pending_container =
          base::FindPtrOrNull(pending_ephemeral_containers_, container_id)) {
    pending_container->QueueUploadItem(entry_id, std::move(*encrypted_content),
                                       std::move(callback));
    return;
  }

  // Verify the container still exists (e.g. a pending ephemeral container's
  // initial key fetch could have failed while compression was in flight).
  if (!container_bridge_->GetStoredEncryptionKeyForContainer(container_id)) {
    std::move(callback).Run(UploadOutcome::kFailed);
    return;
  }

  item_bridge_->UploadItem(container_id, entry_id,
                           std::move(*encrypted_content), std::move(callback));
}

void TabContextSyncServiceImpl::GetContainerAccessToken(
    const ContainerId& container_id,
    base::OnceCallback<void(std::optional<std::string>)> cb) {
  if (PendingEphemeralContainer* pending_container =
          base::FindPtrOrNull(pending_ephemeral_containers_, container_id)) {
    pending_container->QueueGetContainerAccessToken(std::move(cb));
    return;
  }

  if (std::optional<std::string> stored_access_token =
          container_bridge_->GetStoredPreMintedAccessTokenForContainer(
              container_id)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(cb), std::move(stored_access_token)));
    return;
  }

  if (!container_bridge_->GetStoredEncryptionKeyForContainer(container_id)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), std::nullopt));
    return;
  }

  ephemeral_key_fetcher_->FetchEphemeralKey(base::BindOnce(
      &TabContextSyncServiceImpl::OnEphemeralKeyFetchedForAccessToken,
      weak_ptr_factory_.GetWeakPtr(), container_id, std::move(cb)));
}

void TabContextSyncServiceImpl::OnEphemeralKeyFetchedForAccessToken(
    const ContainerId& container_id,
    base::OnceCallback<void(std::optional<std::string>)> cb,
    std::optional<EphemeralKeyFetcher::Result> result) {
  if (!result || !result->ephemeral_key) {
    std::move(cb).Run(std::nullopt);
    return;
  }

  const syncer::AgileSymmetricKeySet* container_key_set =
      container_bridge_->GetStoredEncryptionKeyForContainer(container_id);
  if (!container_key_set) {
    // The container could have been removed during key fetching.
    std::move(cb).Run(std::nullopt);
    return;
  }

  std::move(cb).Run(
      BuildContainerAccessToken(*container_key_set, std::move(*result)));
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
TabContextSyncServiceImpl::GetSyncControllerDelegateForContainer() {
  return container_bridge_->change_processor()->GetControllerDelegate();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
TabContextSyncServiceImpl::GetSyncControllerDelegateForItem() {
  return item_bridge_->change_processor()->GetControllerDelegate();
}

bool TabContextSyncServiceImpl::IsActiveForTesting() const {
  return container_bridge_ && container_bridge_->IsTrackingMetadata();
}

void TabContextSyncServiceImpl::TryTestUpload(
    std::optional<std::string> payload) {
  if (!payload.has_value()) {
    return;
  }

  if (!client_->GetPassphraseType().has_value() ||
      !container_bridge_->IsTrackingMetadata() ||
      !item_bridge_->change_processor()->IsTrackingMetadata()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TabContextSyncServiceImpl::TryTestUpload,
                       weak_ptr_factory_.GetWeakPtr(), std::move(payload)),
        kTestUploadRetryDelay);
    return;
  }

  const std::optional<ContainerId> container_id =
      CreateContainerPossiblyUnencryptedEphemeral();
  if (!container_id.has_value()) {
    LOG(ERROR) << "Failed to create container for --"
               << kSyncTabContextTestUploadSwitch;
    return;
  }

  UploadPageContext(*container_id, kTestUploadEntryId, std::move(*payload),
                    base::BindOnce([](UploadOutcome outcome) {
                      if (outcome != UploadOutcome::kSucceeded) {
                        LOG(ERROR) << "Failed to upload item for --"
                                   << kSyncTabContextTestUploadSwitch;
                      }
                    }));
}

}  // namespace sync_tab_context
