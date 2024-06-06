// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/storage_module.h"

#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_split.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/types/expected.h"
#include "components/reporting/compression/compression_module.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage.h"
#include "components/reporting/storage/storage_configuration.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

namespace {
Status kStorageUnavailableStatus() {
  return {error::UNAVAILABLE, "Storage unavailable"};
}
}  // namespace

// Tracker class is used in SequenceBound, and as such its state is guarded by
// sequence.
class StorageModule::UploadProgressTracker {
 public:
  UploadProgressTracker() = default;
  ~UploadProgressTracker() = default;

  // Invokes the callback if it is the first upload or if progress is detected.
  // It also updates the progress for future calls.
  void Record(const SequenceInformation& seq_info) {
    auto state_it = state_.find(std::make_tuple(seq_info.priority(),
                                                seq_info.generation_id(),
                                                seq_info.generation_guid()));
    if (state_it != state_.end() &&
        seq_info.sequencing_id() <= state_it->second) {
      // No progress detected.
      return;
    }
    state_.insert_or_assign(
        std::make_tuple(seq_info.priority(), seq_info.generation_id(),
                        seq_info.generation_guid()),
        seq_info.sequencing_id());
  }

 private:
  struct Hash {
    size_t operator()(
        const std::tuple<Priority,
                         int64_t /*generation id*/,
                         std::string /*genration_giud*/>& v) const noexcept {
      const auto& [priority, generation_id, genration_giud] = v;
      static constexpr std::hash<Priority> priority_hasher;
      static constexpr std::hash<int64_t> generation_hasher;
      static constexpr std::hash<std::string> genration_giud_hasher;
      return priority_hasher(priority) ^ generation_hasher(generation_id) ^
             genration_giud_hasher(genration_giud);
    }
  };
  std::unordered_map<std::tuple<Priority,
                                int64_t /*generation_id*/,
                                std::string /*genration_giud*/>,
                     int64_t /*sequencing_id*/,
                     Hash>
      state_;
};

StorageModule::StorageModule(const StorageOptions& options)
    : upload_progress_tracker_(base::ThreadPool::CreateSequencedTaskRunner({})),
      options_(options) {}

StorageModule::~StorageModule() = default;

void StorageModule::AddRecord(Priority priority,
                              Record record,
                              EnqueueCallback callback) {
  if (!storage_) {
    std::move(callback).Run(kStorageUnavailableStatus());
    return;
  }
  storage_->Write(priority, std::move(record), std::move(callback));
}

void StorageModule::Flush(Priority priority, FlushCallback callback) {
  if (!storage_) {
    std::move(callback).Run(kStorageUnavailableStatus());
    return;
  }
  storage_->Flush(priority, std::move(callback));
}

void StorageModule::ReportSuccess(SequenceInformation sequence_information,
                                  bool force,
                                  base::OnceCallback<void(Status)> done_cb) {
  if (!storage_) {
    LOG(ERROR) << kStorageUnavailableStatus().error_message();
    std::move(done_cb).Run(kStorageUnavailableStatus());
    return;
  }
  // See whether the device makes any progress, and if so, update the timestamp.
  upload_progress_tracker_.AsyncCall(&UploadProgressTracker::Record)
      .WithArgs(sequence_information);
  // Hand over to the Storage.
  storage_->Confirm(std::move(sequence_information), force, std::move(done_cb));
}

void StorageModule::UpdateEncryptionKey(
    SignedEncryptionInfo signed_encryption_key) {
  if (!storage_) {
    LOG(ERROR) << kStorageUnavailableStatus().error_message();
    return;
  }
  storage_->UpdateEncryptionKey(std::move(signed_encryption_key));
}

void StorageModule::SetLegacyEnabledPriorities(
    std::string_view legacy_storage_enabled) {
  const std::vector<std::string_view> splits =
      base::SplitStringPieceUsingSubstr(legacy_storage_enabled, ",",
                                        base::TRIM_WHITESPACE,
                                        base::SPLIT_WANT_NONEMPTY);
  // Initialize all flags as 'false' (multi-generational, non-legacy).
  std::array<bool, Priority_ARRAYSIZE> legacy_enabled_for_priority;
  for (auto& value : legacy_enabled_for_priority) {
    value = false;
  }
  // Flip specified priorities' flags as 'true' (single-generation, legacy).
  for (const auto& split : splits) {
    Priority priority;
    if (!Priority_Parse(std::string(split), &priority)) {
      LOG(ERROR) << "Invalid legacy-enabled priority specified: `" << split
                 << "`";
      continue;
    }
    CHECK_LT(priority, Priority_ARRAYSIZE);
    legacy_enabled_for_priority[priority] = true;
  }
  // Atomically deliver all priorities' flags to `options_` (shared with
  // `storage_`). For flags that do not change `set_multi_generational` is
  // effectively a no-op.
  for (const auto& priority : StorageOptions::GetPrioritiesOrder()) {
    options_.set_multi_generational(priority,
                                    !legacy_enabled_for_priority[priority]);
  }
}

// static
void StorageModule::Create(
    const StorageOptions& options,
    const std::string_view legacy_storage_enabled,
    const scoped_refptr<QueuesContainer> queues_container,
    const scoped_refptr<EncryptionModuleInterface> encryption_module,
    const scoped_refptr<CompressionModule> compression_module,
    const UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
    base::OnceCallback<void(StatusOr<scoped_refptr<StorageModule>>)> callback) {
  // Call constructor.
  scoped_refptr<StorageModule> instance =
      // Cannot use `base::MakeRefCounted`, since constructor is protected.
      base::WrapRefCounted(new StorageModule(options));

  // Enable/disable multi-generation action for all priorities.
  instance->SetLegacyEnabledPriorities(legacy_storage_enabled);

  // Initialize `instance`.
  instance->InitStorage(options, queues_container, encryption_module,
                        compression_module, async_start_upload_cb,
                        std::move(callback));
}

// static
void StorageModule::InitStorage(
    const StorageOptions& options,
    const scoped_refptr<QueuesContainer> queues_container,
    const scoped_refptr<EncryptionModuleInterface> encryption_module,
    const scoped_refptr<CompressionModule> compression_module,
    const UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
    base::OnceCallback<void(StatusOr<scoped_refptr<StorageModule>>)> callback) {
  // Partially bound callback which sets `storage_` or returns an
  // error status via `callback`. Run on the current default task runner.
  auto set_storage_cb =
      base::BindOnce(&StorageModule::SetStorage, base::WrapRefCounted(this),
                     std::move(callback));

  // Instantiate Storage.
  Storage::Create(options, queues_container, encryption_module,
                  compression_module, async_start_upload_cb,
                  std::move(set_storage_cb));
}

void StorageModule::SetStorage(
    base::OnceCallback<void(StatusOr<scoped_refptr<StorageModule>>)> callback,
    StatusOr<scoped_refptr<Storage>> storage) {
  if (!storage.has_value()) {
    std::move(callback).Run(base::unexpected(std::move(storage).error()));
    return;
  }
  storage_ = storage.value();
  std::move(callback).Run(base::WrapRefCounted(this));
}

void StorageModule::InjectStorageUnavailableErrorForTesting() {
  storage_.reset();
}

}  // namespace reporting
