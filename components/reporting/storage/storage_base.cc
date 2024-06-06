// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/storage_base.h"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/encryption/primitives.h"
#include "components/reporting/encryption/verification.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage_configuration.h"
#include "components/reporting/storage/storage_queue.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/file.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace reporting {

BASE_FEATURE(kLegacyStorageEnabledFeature,
             "LegacyStorageEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

QueueUploaderInterface::QueueUploaderInterface(
    Priority priority,
    std::unique_ptr<UploaderInterface> storage_uploader_interface)
    : priority_(priority),
      storage_uploader_interface_(std::move(storage_uploader_interface)) {}

QueueUploaderInterface::~QueueUploaderInterface() = default;

// Factory method.
void QueueUploaderInterface::AsyncProvideUploader(
    Priority priority,
    UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
    scoped_refptr<EncryptionModuleInterface> encryption_module,
    UploaderInterface::UploadReason reason,
    UploaderInterfaceResultCb start_uploader_cb) {
  const auto upload_reason =
      (/*need_encryption_key=*/encryption_module->is_enabled() &&
       encryption_module->need_encryption_key())
          ? UploaderInterface::UploadReason::KEY_DELIVERY
          : reason;
  async_start_upload_cb.Run(
      upload_reason,
      base::BindOnce(&QueueUploaderInterface::WrapInstantiatedUploader,
                     priority, std::move(start_uploader_cb)));
}

void QueueUploaderInterface::ProcessRecord(
    EncryptedRecord encrypted_record,
    ScopedReservation scoped_reservation,
    base::OnceCallback<void(bool)> processed_cb) {
  // Update sequence information: add Priority.
  SequenceInformation* const sequence_info =
      encrypted_record.mutable_sequence_information();
  sequence_info->set_priority(priority_);
  storage_uploader_interface_->ProcessRecord(std::move(encrypted_record),
                                             std::move(scoped_reservation),
                                             std::move(processed_cb));
}

void QueueUploaderInterface::ProcessGap(
    SequenceInformation start,
    uint64_t count,
    base::OnceCallback<void(bool)> processed_cb) {
  // Update sequence information: add Priority.
  start.set_priority(priority_);
  storage_uploader_interface_->ProcessGap(std::move(start), count,
                                          std::move(processed_cb));
}

void QueueUploaderInterface::Completed(Status final_status) {
  storage_uploader_interface_->Completed(final_status);
}

void QueueUploaderInterface::WrapInstantiatedUploader(
    Priority priority,
    UploaderInterfaceResultCb start_uploader_cb,
    StatusOr<std::unique_ptr<UploaderInterface>> uploader_result) {
  if (!uploader_result.has_value()) {
    std::move(start_uploader_cb)
        .Run(base::unexpected(std::move(uploader_result).error()));
    return;
  }
  std::move(start_uploader_cb)
      .Run(std::make_unique<QueueUploaderInterface>(
          priority, std::move(uploader_result.value())));
}

// static
scoped_refptr<QueuesContainer> QueuesContainer::Create(
    bool storage_degradation_enabled) {
  // Cannot use MakeRefCounted, because constructor is declared private.
  return base::WrapRefCounted(new QueuesContainer(
      storage_degradation_enabled,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock()})));
}

QueuesContainer::QueuesContainer(
    bool storage_degradation_enabled,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : base::RefCountedDeleteOnSequence<QueuesContainer>(sequenced_task_runner),
      sequenced_task_runner_(sequenced_task_runner),
      storage_degradation_enabled_(storage_degradation_enabled) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

QueuesContainer::~QueuesContainer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

Status QueuesContainer::AddQueue(Priority priority,
                                 scoped_refptr<StorageQueue> queue) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto [_, emplaced] = queues_.emplace(
      std::make_tuple(priority, queue->generation_guid()), queue);
  if (!emplaced) {
    return Status(
        error::ALREADY_EXISTS,
        base::StrCat({"Queue with generation GUID=", queue->generation_guid(),
                      " is already being created."}));
  }
  return Status::StatusOK();
}

StatusOr<scoped_refptr<StorageQueue>> QueuesContainer::GetQueue(
    Priority priority,
    GenerationGuid generation_guid) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = queues_.find(std::make_tuple(priority, generation_guid));
  if (it == queues_.end()) {
    return base::unexpected(Status(
        error::NOT_FOUND,
        base::StrCat(
            {"No queue found with priority=", base::NumberToString(priority),
             " and generation_guid= ", generation_guid})));
  }
  return it->second;
}

size_t QueuesContainer::RunActionOnAllQueues(
    Priority priority,
    base::RepeatingCallback<void(scoped_refptr<StorageQueue>)> action) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Flush each queue
  size_t count = 0;
  for (const auto& [priority_generation_tuple, queue] : queues_) {
    auto queue_priority = std::get<Priority>(priority_generation_tuple);
    const auto generation_guid =
        std::get<GenerationGuid>(priority_generation_tuple);
    if (queue_priority == priority) {
      count++;  // Count the number of queues to flush
      action.Run(queue);
    }
  }
  return count;
}

GenerationGuid QueuesContainer::GetOrCreateGenerationGuid(
    const DMtoken& dm_token,
    Priority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StatusOr<GenerationGuid> generation_guid_result =
      GetGenerationGuid(dm_token, priority);
  if (!generation_guid_result.has_value()) {
    // Create a generation guid for this dm token and priority
    generation_guid_result = CreateGenerationGuidForDMToken(dm_token, priority);
    // Creation should never fail.
    CHECK(generation_guid_result.has_value()) << generation_guid_result.error();
  }
  return generation_guid_result.value();
}

StatusOr<GenerationGuid> QueuesContainer::GetGenerationGuid(
    const DMtoken& dm_token,
    Priority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (dmtoken_to_generation_guid_map_.find(std::make_tuple(
          dm_token, priority)) == dmtoken_to_generation_guid_map_.end()) {
    return base::unexpected(Status(
        error::NOT_FOUND,
        base::StrCat({"No generation guid exists for DM token: ", dm_token})));
  }
  return dmtoken_to_generation_guid_map_[std::make_tuple(dm_token, priority)];
}

StatusOr<GenerationGuid> QueuesContainer::CreateGenerationGuidForDMToken(
    const DMtoken& dm_token,
    Priority priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (auto generation_guid = GetGenerationGuid(dm_token, priority);
      generation_guid.has_value()) {
    return base::unexpected(Status(
        error::FAILED_PRECONDITION,
        base::StrCat({"Generation guid for dm_token ", dm_token,
                      " already exists! guid=", generation_guid.value()})));
  }

  GenerationGuid generation_guid =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  dmtoken_to_generation_guid_map_[std::make_tuple(dm_token, priority)] =
      generation_guid;
  return generation_guid;
}

namespace {

// Comparator class for ordering degradation candidates queue.
class QueueComparator {
 public:
  QueueComparator() : priority_to_order_(MapPriorityToOrder()) {}

  bool operator()(
      const std::pair<Priority, scoped_refptr<StorageQueue>>& a,
      const std::pair<Priority, scoped_refptr<StorageQueue>>& b) const {
    // Compare priorities.
    CHECK_LT(a.first, Priority_ARRAYSIZE);
    CHECK_LT(b.first, Priority_ARRAYSIZE);
    const auto pa = priority_to_order_[a.first];
    const auto pb = priority_to_order_[b.first];
    if (pa < pb) {
      return true;  // Lower priority.
    }
    if (pa > pb) {
      return false;  // Higner priority.
    }
    // Equal priority. Compare time stamps: earlier ones first.
    return a.second->time_stamp() < b.second->time_stamp();
  }

 private:
  static std::array<size_t, Priority_ARRAYSIZE> MapPriorityToOrder() {
    size_t index = Priority_MIN;
    std::array<size_t, Priority_ARRAYSIZE> priority_to_order;
    for (const auto& priority : StorageOptions::GetPrioritiesOrder()) {
      priority_to_order[priority] = index;
      ++index;
    }
    return priority_to_order;
  }

  // Reverse mapping Priority->index in ascending priorities order.
  const std::array<size_t, Priority_ARRAYSIZE> priority_to_order_;
};
}  // namespace

// static
void QueuesContainer::GetDegradationCandidates(
    base::WeakPtr<QueuesContainer> container,
    Priority priority,
    const scoped_refptr<StorageQueue> queue,
    base::OnceCallback<void(std::queue<scoped_refptr<StorageQueue>>)>
        result_cb) {
  if (!container) {
    std::move(result_cb).Run({});
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(container->sequence_checker_);
  if (!container->storage_degradation_enabled_) {
    std::move(result_cb).Run({});
    return;
  }

  // Degradation enabled, populate the result from lowest to highest
  // priority up to (but not including) the one referenced by `queue`.
  // Note: base::NoDestruct<QueueComparator> does not compile.
  static const QueueComparator comparator;

  // Collect queues with lower or same priorities as `queue` except the
  // `queue` itself.
  std::vector<std::pair<Priority, scoped_refptr<StorageQueue>>>
      candidate_queues;
  const auto writing_queue_pair = std::make_pair(priority, queue);
  for (const auto& [priority_generation_tuple, candidate_queue] :
       container->queues_) {
    auto queue_priority = std::get<Priority>(priority_generation_tuple);
    auto queue_pair = std::make_pair(queue_priority, candidate_queue);
    if (comparator(queue_pair, writing_queue_pair)) {
      CHECK_NE(candidate_queue.get(), queue.get());
      candidate_queues.emplace_back(queue_pair);
    }
  }

  // Sort them by priority and time stamp.
  std::sort(candidate_queues.begin(), candidate_queues.end(), comparator);

  std::queue<scoped_refptr<StorageQueue>> result;
  for (auto& [_, candidate_queue] : candidate_queues) {
    result.emplace(std::move(candidate_queue));
  }
  std::move(result_cb).Run(std::move(result));
}

// static
void QueuesContainer::DisableQueue(base::WeakPtr<QueuesContainer> container,
                                   Priority priority,
                                   GenerationGuid generation_guid,
                                   base::OnceClosure done_cb) {
  if (!container) {
    std::move(done_cb).Run();
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(container->sequence_checker_);
  const auto count = std::erase_if(container->dmtoken_to_generation_guid_map_,
                                   [&generation_guid](const auto& key_value) {
                                     const auto& [_, guid] = key_value;
                                     return guid == generation_guid;
                                   });
  CHECK_EQ(count, 1u) << Priority_Name(priority) << "/" << generation_guid;
  // The specified queue has been removed from DM_Token map, resume the
  // queue-owned action.
  std::move(done_cb).Run();
}

// static
void QueuesContainer::DisconnectQueue(base::WeakPtr<QueuesContainer> container,
                                      Priority priority,
                                      GenerationGuid generation_guid,
                                      base::OnceClosure done_cb) {
  if (!container) {
    std::move(done_cb).Run();
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(container->sequence_checker_);
  const auto count =
      container->queues_.erase(std::make_tuple(priority, generation_guid));
  CHECK_EQ(count, 1u) << Priority_Name(priority) << "/" << generation_guid;
  // The specified queue has been removed, resume the queue-owned
  // action to remove the queue files from the disk.
  std::move(done_cb).Run();
}

void QueuesContainer::RegisterCompletionCallback(base::OnceClosure callback) {
  CHECK(callback);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::RepeatingClosure queue_callback =
      base::BarrierClosure(queues_.size(), std::move(callback));
  for (auto& queue : queues_) {
    // Copy the callback as base::OnceClosure.
    queue.second->RegisterCompletionCallback(queue_callback);
  }
}

base::WeakPtr<QueuesContainer> QueuesContainer::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

scoped_refptr<base::SequencedTaskRunner>
QueuesContainer::sequenced_task_runner() const {
  return sequenced_task_runner_;
}

KeyInStorage::KeyInStorage(std::string_view signature_verification_public_key,
                           const base::FilePath& directory)
    : verifier_(signature_verification_public_key), directory_(directory) {}

KeyInStorage::~KeyInStorage() = default;

// Uploads signed encryption key to a file with an |index| >=
// |next_key_file_index_|. Returns status in case of any error. If succeeds,
// removes all files with lower indexes (if any). Called every time encryption
// key is updated.
Status KeyInStorage::UploadKeyFile(
    const SignedEncryptionInfo& signed_encryption_key) {
  // Atomically reserve file index (none else will get the same index).
  uint64_t new_file_index = next_key_file_index_.fetch_add(1);
  // Write into file.
  RETURN_IF_ERROR_STATUS(
      WriteKeyInfoFile(new_file_index, signed_encryption_key));

  // Enumerate data files and delete all files with lower index.
  RemoveKeyFilesWithLowerIndexes(new_file_index);
  return Status::StatusOK();
}

// Locates and downloads the latest valid enumeration keys file.
// Atomically sets |next_key_file_index_| to the a value larger than any found
// file. Returns key and key id pair, or error status (NOT_FOUND if no valid
// file has been found). Called once during initialization only.
StatusOr<std::pair<std::string, EncryptionModuleInterface::PublicKeyId>>
KeyInStorage::DownloadKeyFile() {
  // Make sure the assigned directory exists.
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(directory_, &error)) {
    return base::unexpected(Status(
        error::UNAVAILABLE,
        base::StrCat(
            {"Storage directory '", directory_.MaybeAsASCII(),
             "' does not exist, error=", base::File::ErrorToString(error)})));
  }

  // Enumerate possible key files, collect the ones that have valid name,
  // set next_key_file_index_ to a value that is definitely not used.
  std::unordered_set<base::FilePath> all_key_files;
  std::map<uint64_t, base::FilePath, std::greater<>> found_key_files;
  EnumerateKeyFiles(&all_key_files, &found_key_files);

  // Try to unserialize the key from each found file (latest first).
  auto signed_encryption_key_result = LocateValidKeyAndParse(found_key_files);

  // If not found, return error.
  if (!signed_encryption_key_result.has_value()) {
    return base::unexpected(
        Status(error::NOT_FOUND, "No valid encryption key found"));
  }

  // Found and validated, delete all other files.
  for (const auto& full_name : all_key_files) {
    if (full_name == signed_encryption_key_result.value().first) {
      continue;  // This file is used.
    }
    DeleteFileWarnIfFailed(full_name);  // Ignore errors, if any.
  }

  // Return the key.
  return std::make_pair(
      signed_encryption_key_result.value().second.public_asymmetric_key(),
      signed_encryption_key_result.value().second.public_key_id());
}

Status KeyInStorage::VerifySignature(
    const SignedEncryptionInfo& signed_encryption_key) {
  if (signed_encryption_key.public_asymmetric_key().size() != kKeySize) {
    return Status{error::FAILED_PRECONDITION, "Key size mismatch"};
  }
  char value_to_verify[sizeof(EncryptionModuleInterface::PublicKeyId) +
                       kKeySize];
  const EncryptionModuleInterface::PublicKeyId public_key_id =
      signed_encryption_key.public_key_id();
  memcpy(value_to_verify, &public_key_id,
         sizeof(EncryptionModuleInterface::PublicKeyId));
  memcpy(value_to_verify + sizeof(EncryptionModuleInterface::PublicKeyId),
         signed_encryption_key.public_asymmetric_key().data(), kKeySize);
  return verifier_.Verify(std::string(value_to_verify, sizeof(value_to_verify)),
                          signed_encryption_key.signature());
}

// Writes key into file. Called during key upload.
Status KeyInStorage::WriteKeyInfoFile(
    uint64_t new_file_index,
    const SignedEncryptionInfo& signed_encryption_key) {
  base::FilePath key_file_path =
      directory_.Append(kEncryptionKeyFilePrefix)
          .AddExtensionASCII(base::NumberToString(new_file_index));
  base::File key_file(key_file_path,
                      base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
  if (!key_file.IsValid()) {
    return Status(error::DATA_LOSS,
                  base::StrCat({"Cannot open key file='",
                                key_file_path.MaybeAsASCII(), "' for append"}));
  }
  std::string serialized_key;
  if (!signed_encryption_key.SerializeToString(&serialized_key) ||
      serialized_key.empty()) {
    return Status(error::DATA_LOSS,
                  base::StrCat({"Failed to seralize key into file='",
                                key_file_path.MaybeAsASCII(), "'"}));
  }
  const int32_t write_result = key_file.Write(
      /*offset=*/0, serialized_key.data(), serialized_key.size());
  if (write_result < 0) {
    return Status(
        error::DATA_LOSS,
        base::StrCat({"File write error=",
                      key_file.ErrorToString(key_file.GetLastFileError()),
                      " file=", key_file_path.MaybeAsASCII()}));
  }
  if (static_cast<size_t>(write_result) != serialized_key.size()) {
    return Status(error::DATA_LOSS,
                  base::StrCat({"Failed to seralize key into file='",
                                key_file_path.MaybeAsASCII(), "'"}));
  }
  return Status::StatusOK();
}

// Enumerates key files and deletes those with index lower than
// |new_file_index|. Called during key upload.
void KeyInStorage::RemoveKeyFilesWithLowerIndexes(uint64_t new_file_index) {
  base::FileEnumerator dir_enum(
      directory_,
      /*recursive=*/false, base::FileEnumerator::FILES,
      base::StrCat({kEncryptionKeyFilePrefix, FILE_PATH_LITERAL("*")}));
  DeleteFilesWarnIfFailed(
      dir_enum,
      base::BindRepeating(
          [](uint64_t new_file_index, const base::FilePath& full_name) {
            const auto file_index =
                StorageQueue::GetFileSequenceIdFromPath(full_name);
            if (!file_index
                     .has_value() ||  // Should not happen, will remove file.
                file_index.value() <
                    static_cast<int64_t>(
                        new_file_index)) {  // Lower index file, will remove
                                            // it.
              return true;
            }
            return false;
          },
          new_file_index));
}

// Enumerates possible key files, collects the ones that have valid name,
// sets next_key_file_index_ to a value that is definitely not used.
// Called once, during initialization.
void KeyInStorage::EnumerateKeyFiles(
    std::unordered_set<base::FilePath>* all_key_files,
    std::map<uint64_t, base::FilePath, std::greater<>>* found_key_files) {
  base::FileEnumerator dir_enum(
      directory_,
      /*recursive=*/false, base::FileEnumerator::FILES,
      base::StrCat({kEncryptionKeyFilePrefix, FILE_PATH_LITERAL("*")}));
  for (auto full_name = dir_enum.Next(); !full_name.empty();
       full_name = dir_enum.Next()) {
    if (!all_key_files->emplace(full_name).second) {
      // Duplicate file name. Should not happen.
      continue;
    }
    const auto file_index = StorageQueue::GetFileSequenceIdFromPath(full_name);
    if (!file_index.has_value()) {  // Shouldn't happen, something went wrong.
      continue;
    }
    if (!found_key_files
             ->emplace(static_cast<uint64_t>(file_index.value()), full_name)
             .second) {
      // Duplicate extension (e.g., 01 and 001). Should not happen (file is
      // corrupt).
      continue;
    }
    // Set 'next_key_file_index_' to a number which is definitely not used.
    if (static_cast<int64_t>(next_key_file_index_.load()) <=
        file_index.value()) {
      next_key_file_index_.store(static_cast<uint64_t>(file_index.value() + 1));
    }
  }
}

// Enumerates found key files and locates one with the highest index and
// valid key. Returns pair of file name and loaded signed key proto.
// Called once, during initialization.
std::optional<std::pair<base::FilePath, SignedEncryptionInfo>>
KeyInStorage::LocateValidKeyAndParse(
    const std::map<uint64_t, base::FilePath, std::greater<>>& found_key_files) {
  // Try to unserialize the key from each found file (latest first, since the
  // map is reverse-ordered).
  for (const auto& [index, file_path] : found_key_files) {
    base::File key_file(file_path,
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!key_file.IsValid()) {
      continue;  // Could not open.
    }

    SignedEncryptionInfo signed_encryption_key;
    {
      char key_file_buffer[kEncryptionKeyMaxFileSize];
      const int32_t read_result = key_file.Read(
          /*offset=*/0, key_file_buffer, kEncryptionKeyMaxFileSize);
      if (read_result < 0) {
        LOG(WARNING) << "File read error="
                     << key_file.ErrorToString(key_file.GetLastFileError())
                     << " " << file_path.MaybeAsASCII();
        continue;  // File read error.
      }
      if (read_result == 0 || read_result >= kEncryptionKeyMaxFileSize) {
        continue;  // Unexpected file size.
      }
      google::protobuf::io::ArrayInputStream key_stream(  // Zero-copy stream.
          key_file_buffer, read_result);
      if (!signed_encryption_key.ParseFromZeroCopyStream(&key_stream)) {
        LOG(WARNING) << "Failed to parse key file, full_name='"
                     << file_path.MaybeAsASCII() << "'";
        continue;
      }
    }

    // Parsed successfully. Verify signature of the whole "id"+"key" string.
    const auto signature_verification_status =
        VerifySignature(signed_encryption_key);
    if (!signature_verification_status.ok()) {
      LOG(WARNING) << "Loaded key failed verification, status="
                   << signature_verification_status << ", full_name='"
                   << file_path.MaybeAsASCII() << "'";
      continue;
    }

    // Validated successfully. Return file name and signed key proto.
    return std::make_pair(file_path, signed_encryption_key);
  }

  // Not found, return error.
  return std::nullopt;
}
}  // namespace reporting
