// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/tab_context_sync_service_impl.h"

#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/crypto/agile_symmetric_key_set.h"
#include "components/sync/protocol/encrypted_tab_context_item_specifics.pb.h"
#include "components/sync/protocol/tab_context_container_access_token.pb.h"
#include "components/sync_tab_context/ephemeral_key_fetcher.h"
#include "components/sync_tab_context/tab_context_container_sync_bridge.h"
#include "components/sync_tab_context/tab_context_item_sync_bridge.h"
#include "components/sync_tab_context/upload_outcome.h"
#include "third_party/zlib/google/compression_utils.h"

namespace sync_tab_context {

namespace {

google::protobuf::Timestamp TimeToGoogleTimestampProto(base::Time time) {
  google::protobuf::Timestamp timestamp;
  const int64_t seconds = (time - base::Time::UnixEpoch()).InSeconds();
  const int64_t nanos =
      ((time - base::Time::UnixEpoch()) - base::Seconds(seconds))
          .InNanoseconds();
  timestamp.set_seconds(seconds);
  timestamp.set_nanos(static_cast<int32_t>(nanos));
  return timestamp;
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

// TODO(crbug.com/535450467): Consider improving type safety instead of dealing
// with strings.
std::string BuildContainerAccessToken(
    const syncer::AgileSymmetricKeySet& container_key_set,
    EphemeralKeyFetcher::Result result) {
  CHECK(result.ephemeral_key);

  const std::string container_key_bytes =
      container_key_set.ToProto().SerializeAsString();
  const std::optional<sync_pb::EncryptedData> encrypted_container_key =
      result.ephemeral_key->Encrypt(base::as_byte_span(container_key_bytes));
  if (!encrypted_container_key) {
    return std::string();
  }

  sync_pb::TabContextContainerAccessToken token_proto;
  token_proto.set_name(std::move(result.name));
  token_proto.set_encrypted_container_key(
      encrypted_container_key->SerializeAsString());
  if (!result.expire_time.is_null()) {
    *token_proto.mutable_expire_time() =
        TimeToGoogleTimestampProto(result.expire_time);
  }

  return token_proto.SerializeAsString();
}

}  // namespace

TabContextSyncServiceImpl::TabContextSyncServiceImpl(
    syncer::OnceDataTypeStoreFactory store_factory,
    std::unique_ptr<EphemeralKeyFetcher> ephemeral_key_fetcher,
    base::RepeatingClosure dump_stack)
    : container_bridge_(std::make_unique<TabContextContainerSyncBridge>(
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
          {base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  CHECK(ephemeral_key_fetcher_);
}

TabContextSyncServiceImpl::~TabContextSyncServiceImpl() = default;

std::optional<ContainerId> TabContextSyncServiceImpl::CreateContainer() {
  return container_bridge_->CreateContainer();
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
      container_bridge_->GetEncryptionKeyForContainer(container_id);
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

  item_bridge_->UploadItem(container_id, entry_id,
                           std::move(*encrypted_content), std::move(callback));
}

void TabContextSyncServiceImpl::GetContainerAccessToken(
    const ContainerId& container_id,
    base::OnceCallback<void(std::optional<std::string>)> cb) {
  if (!container_bridge_->GetEncryptionKeyForContainer(container_id)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), std::nullopt));
    return;
  }

  ephemeral_key_fetcher_->FetchEphemeralKey(base::BindOnce(
      &TabContextSyncServiceImpl::OnEphemeralKeyFetched,
      weak_ptr_factory_.GetWeakPtr(), container_id, std::move(cb)));
}

void TabContextSyncServiceImpl::OnEphemeralKeyFetched(
    const ContainerId& container_id,
    base::OnceCallback<void(std::optional<std::string>)> cb,
    std::optional<EphemeralKeyFetcher::Result> result) {
  if (!result || !result->ephemeral_key) {
    std::move(cb).Run(std::nullopt);
    return;
  }

  const syncer::AgileSymmetricKeySet* container_key_set =
      container_bridge_->GetEncryptionKeyForContainer(container_id);
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
  return container_bridge_ &&
         container_bridge_->IsTrackingMetadataForTesting();  // IN-TEST
}

}  // namespace sync_tab_context
