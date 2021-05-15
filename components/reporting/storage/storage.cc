// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/storage.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/encryption/primitives.h"
#include "components/reporting/encryption/verification.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/storage/storage_configuration.h"
#include "components/reporting/storage/storage_queue.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/task_runner_context.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl.h"

namespace reporting {

namespace {

// Parameters of individual queues.
// TODO(b/159352842): Deliver space and upload parameters from outside.

constexpr base::FilePath::CharType kImmediateQueueSubdir[] =
    FILE_PATH_LITERAL("Immediate");
constexpr base::FilePath::CharType kImmediateQueuePrefix[] =
    FILE_PATH_LITERAL("P_Immediate");

constexpr base::FilePath::CharType kFastBatchQueueSubdir[] =
    FILE_PATH_LITERAL("FastBatch");
constexpr base::FilePath::CharType kFastBatchQueuePrefix[] =
    FILE_PATH_LITERAL("P_FastBatch");
constexpr base::TimeDelta kFastBatchUploadPeriod =
    base::TimeDelta::FromSeconds(1);

constexpr base::FilePath::CharType kSlowBatchQueueSubdir[] =
    FILE_PATH_LITERAL("SlowBatch");
constexpr base::FilePath::CharType kSlowBatchQueuePrefix[] =
    FILE_PATH_LITERAL("P_SlowBatch");
constexpr base::TimeDelta kSlowBatchUploadPeriod =
    base::TimeDelta::FromSeconds(20);

constexpr base::FilePath::CharType kBackgroundQueueSubdir[] =
    FILE_PATH_LITERAL("Background");
constexpr base::FilePath::CharType kBackgroundQueuePrefix[] =
    FILE_PATH_LITERAL("P_Background");
constexpr base::TimeDelta kBackgroundQueueUploadPeriod =
    base::TimeDelta::FromMinutes(1);

constexpr base::FilePath::CharType kManualQueueSubdir[] =
    FILE_PATH_LITERAL("Manual");
constexpr base::FilePath::CharType kManualQueuePrefix[] =
    FILE_PATH_LITERAL("P_Manual");
constexpr base::TimeDelta kManualUploadPeriod = base::TimeDelta::Max();

constexpr base::FilePath::CharType kEncryptionKeyFilePrefix[] =
    FILE_PATH_LITERAL("EncryptionKey.");
const int32_t kEncryptionKeyMaxFileSize = 256;

// Returns vector of <priority, queue_options> for all expected queues in
// Storage. Queues are all located under the given root directory.
std::vector<std::pair<Priority, QueueOptions>> ExpectedQueues(
    const StorageOptions& options) {
  return {
      std::make_pair(IMMEDIATE, QueueOptions(options)
                                    .set_subdirectory(kImmediateQueueSubdir)
                                    .set_file_prefix(kImmediateQueuePrefix)),
      std::make_pair(FAST_BATCH,
                     QueueOptions(options)
                         .set_subdirectory(kFastBatchQueueSubdir)
                         .set_file_prefix(kFastBatchQueuePrefix)
                         .set_upload_period(kFastBatchUploadPeriod)),
      std::make_pair(SLOW_BATCH,
                     QueueOptions(options)
                         .set_subdirectory(kSlowBatchQueueSubdir)
                         .set_file_prefix(kSlowBatchQueuePrefix)
                         .set_upload_period(kSlowBatchUploadPeriod)),
      std::make_pair(BACKGROUND_BATCH,
                     QueueOptions(options)
                         .set_subdirectory(kBackgroundQueueSubdir)
                         .set_file_prefix(kBackgroundQueuePrefix)
                         .set_upload_period(kBackgroundQueueUploadPeriod)),
      std::make_pair(MANUAL_BATCH, QueueOptions(options)
                                       .set_subdirectory(kManualQueueSubdir)
                                       .set_file_prefix(kManualQueuePrefix)
                                       .set_upload_period(kManualUploadPeriod)),
  };
}

}  // namespace

// Uploader interface adaptor for individual queue.
class Storage::QueueUploaderInterface : public UploaderInterface {
 public:
  QueueUploaderInterface(Priority priority,
                         std::unique_ptr<UploaderInterface> storage_interface)
      : priority_(priority), storage_interface_(std::move(storage_interface)) {}

  // Factory method.
  static void AsyncProvideUploader(
      Priority priority,
      Storage* storage,
      UploaderInterfaceResultCb start_uploader_cb) {
    storage->async_start_upload_cb_.Run(
        priority,
        /*need_encryption_key=*/EncryptionModuleInterface::is_enabled() &&
            storage->encryption_module_->need_encryption_key(),
        base::BindOnce(&QueueUploaderInterface::WrapInstantiatedUploader,
                       priority, std::move(start_uploader_cb)));
  }

  void ProcessRecord(EncryptedRecord encrypted_record,
                     base::OnceCallback<void(bool)> processed_cb) override {
    // Update sequencing information: add Priority.
    SequencingInformation* const sequencing_info =
        encrypted_record.mutable_sequencing_information();
    sequencing_info->set_priority(priority_);
    storage_interface_->ProcessRecord(std::move(encrypted_record),
                                      std::move(processed_cb));
  }

  void ProcessGap(SequencingInformation start,
                  uint64_t count,
                  base::OnceCallback<void(bool)> processed_cb) override {
    // Update sequencing information: add Priority.
    start.set_priority(priority_);
    storage_interface_->ProcessGap(std::move(start), count,
                                   std::move(processed_cb));
  }

  void Completed(Status final_status) override {
    storage_interface_->Completed(final_status);
  }

 private:
  static void WrapInstantiatedUploader(
      Priority priority,
      UploaderInterfaceResultCb start_uploader_cb,
      StatusOr<std::unique_ptr<UploaderInterface>> uploader_result) {
    if (!uploader_result.ok()) {
      std::move(start_uploader_cb).Run(uploader_result.status());
      return;
    }
    std::move(start_uploader_cb)
        .Run(std::make_unique<QueueUploaderInterface>(
            priority, std::move(uploader_result.ValueOrDie())));
  }

  const Priority priority_;
  const std::unique_ptr<UploaderInterface> storage_interface_;
};

class Storage::KeyInStorage {
 public:
  explicit KeyInStorage(base::StringPiece signature_verification_public_key,
                        const base::FilePath& directory)
      : verifier_(signature_verification_public_key), directory_(directory) {}
  ~KeyInStorage() = default;

  // Uploads signed encryption key to a file with an |index| >=
  // |next_key_file_index_|. Returns status in case of any error. If succeeds,
  // removes all files with lower indexes (if any). Called every time encryption
  // key is updated.
  Status UploadKeyFile(const SignedEncryptionInfo& signed_encryption_key) {
    // Atomically reserve file index (none else will get the same index).
    uint64_t new_file_index = next_key_file_index_.fetch_add(1);
    // Write into file.
    RETURN_IF_ERROR(WriteKeyInfoFile(new_file_index, signed_encryption_key));

    // Enumerate data files and delete all files with lower index.
    RemoveKeyFilesWithLowerIndexes(new_file_index);
    return Status::StatusOK();
  }

  // Locates and downloads the latest valid enumeration keys file.
  // Atomically sets |next_key_file_index_| to the a value larger than any found
  // file. Returns key and key id pair, or error status (NOT_FOUND if no valid
  // file has been found). Called once during initialization only.
  StatusOr<std::pair<std::string, EncryptionModuleInterface::PublicKeyId>>
  DownloadKeyFile() {
    // Make sure the assigned directory exists.
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(directory_, &error)) {
      return Status(
          error::UNAVAILABLE,
          base::StrCat(
              {"Storage directory '", directory_.MaybeAsASCII(),
               "' does not exist, error=", base::File::ErrorToString(error)}));
    }

    // Enumerate possible key files, collect the ones that have valid name,
    // set next_key_file_index_ to a value that is definitely not used.
    base::flat_set<base::FilePath> all_key_files;
    base::flat_map<uint64_t, base::FilePath> found_key_files;
    EnumerateKeyFiles(&all_key_files, &found_key_files);

    // Try to unserialize the key from each found file (latest first).
    auto signed_encryption_key_result = LocateValidKeyAndParse(found_key_files);

    // If not found, return error.
    if (!signed_encryption_key_result.has_value()) {
      return Status(error::NOT_FOUND, "No valid encryption key found");
    }

    // Found and validated, delete all other files.
    for (const auto& full_name : all_key_files) {
      if (full_name == signed_encryption_key_result.value().first) {
        continue;  // This file is used.
      }
      base::DeleteFile(full_name);  // Ignore errors, if any.
    }

    // Return the key.
    return std::make_pair(
        signed_encryption_key_result.value().second.public_asymmetric_key(),
        signed_encryption_key_result.value().second.public_key_id());
  }

  Status VerifySignature(const SignedEncryptionInfo& signed_encryption_key) {
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
    return verifier_.Verify(
        std::string(value_to_verify, sizeof(value_to_verify)),
        signed_encryption_key.signature());
  }

 private:
  // Writes key into file. Called during key upload.
  Status WriteKeyInfoFile(uint64_t new_file_index,
                          const SignedEncryptionInfo& signed_encryption_key) {
    base::FilePath key_file_path =
        directory_.Append(kEncryptionKeyFilePrefix)
            .AddExtensionASCII(base::NumberToString(new_file_index));
    base::File key_file(key_file_path,
                        base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
    if (!key_file.IsValid()) {
      return Status(
          error::DATA_LOSS,
          base::StrCat({"Cannot open key file='", key_file_path.MaybeAsASCII(),
                        "' for append"}));
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
  void RemoveKeyFilesWithLowerIndexes(uint64_t new_file_index) {
    base::flat_set<base::FilePath> key_files_to_remove;
    base::FileEnumerator dir_enum(
        directory_,
        /*recursive=*/false, base::FileEnumerator::FILES,
        base::StrCat({kEncryptionKeyFilePrefix, FILE_PATH_LITERAL("*")}));
    base::FilePath full_name;
    while (full_name = dir_enum.Next(), !full_name.empty()) {
      const auto result = key_files_to_remove.emplace(full_name);
      if (!result.second) {
        // Duplicate file name. Should not happen.
        continue;
      }
      const auto extension = full_name.Extension();
      if (extension.empty()) {
        // Should not happen, will remove this file.
        continue;
      }
      uint64_t file_index = 0;
      if (!base::StringToUint64(extension.substr(1), &file_index)) {
        // Bad extension - not a number. Should not happen, will remove this
        // file.
        continue;
      }
      if (file_index < new_file_index) {
        // Lower index file, will remove it.
        continue;
      }
      // Keep this file - drop it from erase list.
      key_files_to_remove.erase(result.first);
    }
    // Delete all files assigned for deletion.
    for (const auto& full_name : key_files_to_remove) {
      base::DeleteFile(full_name);  // Ignore errors, if any.
    }
  }

  // Enumerates possible key files, collects the ones that have valid name,
  // sets next_key_file_index_ to a value that is definitely not used.
  // Called once, during initialization.
  void EnumerateKeyFiles(
      base::flat_set<base::FilePath>* all_key_files,
      base::flat_map<uint64_t, base::FilePath>* found_key_files) {
    base::FileEnumerator dir_enum(
        directory_,
        /*recursive=*/false, base::FileEnumerator::FILES,
        base::StrCat({kEncryptionKeyFilePrefix, FILE_PATH_LITERAL("*")}));
    base::FilePath full_name;
    while (full_name = dir_enum.Next(), !full_name.empty()) {
      if (!all_key_files->emplace(full_name).second) {
        // Duplicate file name. Should not happen.
        continue;
      }
      const auto extension = full_name.Extension();
      if (extension.empty()) {
        // Should not happen.
        continue;
      }
      uint64_t file_index = 0;
      bool success = base::StringToUint64(extension.substr(1), &file_index);
      if (!success) {
        // Bad extension - not a number. Should not happen (file is corrupt).
        continue;
      }
      if (!found_key_files->emplace(file_index, full_name).second) {
        // Duplicate extension (e.g., 01 and 001). Should not happen (file is
        // corrupt).
        continue;
      }
      // Set 'next_key_file_index_' to a number which is definitely not used.
      if (next_key_file_index_.load() <= file_index) {
        next_key_file_index_.store(file_index + 1);
      }
    }
  }

  // Enumerates found key files and locates one with the highest index and
  // valid key. Returns pair of file name and loaded signed key proto.
  // Called once, during initialization.
  absl::optional<std::pair<base::FilePath, SignedEncryptionInfo>>
  LocateValidKeyAndParse(
      const base::flat_map<uint64_t, base::FilePath>& found_key_files) {
    // Try to unserialize the key from each found file (latest first).
    for (auto key_file_it = found_key_files.rbegin();
         key_file_it != found_key_files.rend(); ++key_file_it) {
      base::File key_file(key_file_it->second,
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
      if (!key_file.IsValid()) {
        continue;  // Could not open.
      }

      SignedEncryptionInfo signed_encryption_key;
      {
        const auto key_file_buffer =
            std::make_unique<char[]>(kEncryptionKeyMaxFileSize);
        const int32_t read_result = key_file.Read(
            /*offset=*/0, key_file_buffer.get(), kEncryptionKeyMaxFileSize);
        if (read_result < 0) {
          LOG(WARNING) << "File read error="
                       << key_file.ErrorToString(key_file.GetLastFileError())
                       << " " << key_file_it->second.MaybeAsASCII();
          continue;  // File read error.
        }
        if (read_result == 0 || read_result >= kEncryptionKeyMaxFileSize) {
          continue;  // Unexpected file size.
        }
        google::protobuf::io::ArrayInputStream key_stream(  // Zero-copy stream.
            key_file_buffer.get(), read_result);
        if (!signed_encryption_key.ParseFromZeroCopyStream(&key_stream)) {
          LOG(WARNING) << "Failed to parse key file, full_name='"
                       << key_file_it->second.MaybeAsASCII() << "'";
          continue;
        }
      }

      // Parsed successfully. Verify signature of the whole "id"+"key" string.
      const auto signature_verification_status =
          VerifySignature(signed_encryption_key);
      if (!signature_verification_status.ok()) {
        LOG(WARNING) << "Loaded key failed verification, status="
                     << signature_verification_status << ", full_name='"
                     << key_file_it->second.MaybeAsASCII() << "'";
        continue;
      }

      // Validated successfully. Return file name and signed key proto.
      return std::make_pair(key_file_it->second, signed_encryption_key);
    }

    // Not found, return error.
    return absl::nullopt;
  }

  // Index of the file to serialize the signed key to.
  // Initialized to the next available number or 0, if none present.
  // Every time a new key is received, it is stored in a file with the next
  // index; however, any file found with the matching signature can be used
  // to successfully encrypt records and for the server to then decrypt them.
  std::atomic<uint64_t> next_key_file_index_{0};

  SignatureVerifier verifier_;

  const base::FilePath directory_;
};

void Storage::Create(
    const StorageOptions& options,
    UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
    scoped_refptr<EncryptionModuleInterface> encryption_module,
    base::OnceCallback<void(StatusOr<scoped_refptr<Storage>>)> completion_cb) {
  // Initialize Storage object, populating all the queues.
  class StorageInitContext
      : public TaskRunnerContext<StatusOr<scoped_refptr<Storage>>> {
   public:
    StorageInitContext(
        const std::vector<std::pair<Priority, QueueOptions>>& queues_options,
        scoped_refptr<Storage> storage,
        base::OnceCallback<void(StatusOr<scoped_refptr<Storage>>)> callback)
        : TaskRunnerContext<StatusOr<scoped_refptr<Storage>>>(
              std::move(callback),
              base::ThreadPool::CreateSequencedTaskRunner(
                  {base::TaskPriority::BEST_EFFORT, base::MayBlock()})),
          queues_options_(queues_options),
          storage_(std::move(storage)) {}

   private:
    // Context can only be deleted by calling Response method.
    ~StorageInitContext() override { DCHECK_EQ(count_, 0); }

    void OnStart() override {
      CheckOnValidSequence();

      // Locate the latest signed_encryption_key file with matching key
      // signature after deserialization.
      const auto download_key_result =
          storage_->key_in_storage_->DownloadKeyFile();
      if (!download_key_result.ok()) {
        // Key not found or corrupt. Proceed with queues creation directly.
        EncryptionSetUp(download_key_result.status());
        return;
      }

      // Key found, verified and downloaded.
      storage_->encryption_module_->UpdateAsymmetricKey(
          download_key_result.ValueOrDie().first,
          download_key_result.ValueOrDie().second,
          base::BindOnce(&StorageInitContext::ScheduleEncryptionSetUp,
                         base::Unretained(this)));
    }

    void ScheduleEncryptionSetUp(Status status) {
      Schedule(&StorageInitContext::EncryptionSetUp, base::Unretained(this),
               status);
    }

    void EncryptionSetUp(Status status) {
      CheckOnValidSequence();

      if (status.ok()) {
        // Encryption key has been found and set up. Must be available now.
        DCHECK(storage_->encryption_module_->has_encryption_key());
      } else {
        if (EncryptionModuleInterface::is_enabled()) {
          // Initiate upload with need_encryption_key flag and no records.
          UploaderInterface::UploaderInterfaceResultCb start_uploader_cb =
              base::BindOnce(&StorageInitContext::EncryptionKeyReceiverReady);
          storage_->async_start_upload_cb_.Run(
              /*priority=*/MANUAL_BATCH,  // Any priority would do.
              /*need_encryption_key=*/true,
              base::BindOnce(&StorageInitContext::WrapInstantiatedKeyUploader,
                             /*priority=*/MANUAL_BATCH,
                             std::move(start_uploader_cb)));
          // Continue initialization without waiting for it to respond.
          // Until the response arrives, we will reject Enqueues.
        }
      }

      // Construct all queues.
      count_ = queues_options_.size();
      for (const auto& queue_options : queues_options_) {
        StorageQueue::Create(
            /*options=*/queue_options.second,
            // Note: the callback below belongs to the Queue and does not
            // outlive Storage.
            base::BindRepeating(&QueueUploaderInterface::AsyncProvideUploader,
                                /*priority=*/queue_options.first,
                                base::Unretained(storage_.get())),
            storage_->encryption_module_,
            base::BindOnce(&StorageInitContext::ScheduleAddQueue,
                           base::Unretained(this),
                           /*priority=*/queue_options.first));
      }
    }

    static void WrapInstantiatedKeyUploader(
        Priority priority,
        UploaderInterface::UploaderInterfaceResultCb start_uploader_cb,
        StatusOr<std::unique_ptr<UploaderInterface>> uploader_result) {
      if (!uploader_result.ok()) {
        std::move(start_uploader_cb).Run(uploader_result.status());
        return;
      }
      std::move(start_uploader_cb)
          .Run(std::make_unique<QueueUploaderInterface>(
              priority, std::move(uploader_result.ValueOrDie())));
    }

    static void EncryptionKeyReceiverReady(
        StatusOr<std::unique_ptr<UploaderInterface>> uploader_result) {
      if (uploader_result.ok()) {
        uploader_result.ValueOrDie()->Completed(Status::StatusOK());
      }
    }

    void ScheduleAddQueue(
        Priority priority,
        StatusOr<scoped_refptr<StorageQueue>> storage_queue_result) {
      Schedule(&StorageInitContext::AddQueue, base::Unretained(this), priority,
               std::move(storage_queue_result));
    }

    void AddQueue(Priority priority,
                  StatusOr<scoped_refptr<StorageQueue>> storage_queue_result) {
      CheckOnValidSequence();
      if (storage_queue_result.ok()) {
        auto add_result = storage_->queues_.emplace(
            priority, storage_queue_result.ValueOrDie());
        DCHECK(add_result.second);
      } else {
        LOG(ERROR) << "Could not create queue, priority=" << priority
                   << ", status=" << storage_queue_result.status();
        if (final_status_.ok()) {
          final_status_ = storage_queue_result.status();
        }
      }
      DCHECK_GT(count_, 0);
      if (--count_ > 0) {
        return;
      }
      if (!final_status_.ok()) {
        Response(final_status_);
        return;
      }
      Response(std::move(storage_));
    }

    const std::vector<std::pair<Priority, QueueOptions>> queues_options_;
    scoped_refptr<Storage> storage_;
    int32_t count_ = 0;
    Status final_status_;
  };

  // Create Storage object.
  // Cannot use base::MakeRefCounted<Storage>, because constructor is private.
  scoped_refptr<Storage> storage = base::WrapRefCounted(new Storage(
      options, encryption_module, std::move(async_start_upload_cb)));

  // Asynchronously run initialization.
  Start<StorageInitContext>(ExpectedQueues(storage->options_),
                            std::move(storage), std::move(completion_cb));
}

Storage::Storage(const StorageOptions& options,
                 scoped_refptr<EncryptionModuleInterface> encryption_module,
                 UploaderInterface::AsyncStartUploaderCb async_start_upload_cb)
    : options_(options),
      encryption_module_(encryption_module),
      key_in_storage_(std::make_unique<KeyInStorage>(
          options.signature_verification_public_key(),
          options.directory())),
      async_start_upload_cb_(std::move(async_start_upload_cb)) {}

Storage::~Storage() = default;

void Storage::Write(Priority priority,
                    Record record,
                    base::OnceCallback<void(Status)> completion_cb) {
  // Note: queues_ never change after initialization is finished, so there is
  // no need to protect or serialize access to it.
  ASSIGN_OR_ONCE_CALLBACK_AND_RETURN(scoped_refptr<StorageQueue> queue,
                                     completion_cb, GetQueue(priority));
  queue->Write(std::move(record), std::move(completion_cb));
}

void Storage::Confirm(Priority priority,
                      absl::optional<int64_t> seq_number,
                      bool force,
                      base::OnceCallback<void(Status)> completion_cb) {
  // Note: queues_ never change after initialization is finished, so there is
  // no need to protect or serialize access to it.
  ASSIGN_OR_ONCE_CALLBACK_AND_RETURN(scoped_refptr<StorageQueue> queue,
                                     completion_cb, GetQueue(priority));
  queue->Confirm(seq_number, force, std::move(completion_cb));
}

Status Storage::Flush(Priority priority) {
  // Note: queues_ never change after initialization is finished, so there is
  // no need to protect or serialize access to it.
  ASSIGN_OR_RETURN(scoped_refptr<StorageQueue> queue, GetQueue(priority));
  queue->Flush();
  return Status::StatusOK();
}

void Storage::UpdateEncryptionKey(SignedEncryptionInfo signed_encryption_key) {
  // Verify received key signature. Bail out if failed.
  const auto signature_verification_status =
      key_in_storage_->VerifySignature(signed_encryption_key);
  if (!signature_verification_status.ok()) {
    LOG(WARNING) << "Key failed verification, status="
                 << signature_verification_status;
    return;
  }

  // Assign the received key to encryption module.
  encryption_module_->UpdateAsymmetricKey(
      signed_encryption_key.public_asymmetric_key(),
      signed_encryption_key.public_key_id(), base::BindOnce([](Status status) {
        if (!status.ok()) {
          LOG(WARNING) << "Encryption key update failed, status=" << status;
          return;
        }
        // Encryption key updated successfully.
      }));

  // Serialize whole signed_encryption_key to a new file, discard the old
  // one(s). Do it on a thread which may block doing file operations.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(
          [](SignedEncryptionInfo signed_encryption_key,
             KeyInStorage* key_in_storage) {
            const Status status =
                key_in_storage->UploadKeyFile(signed_encryption_key);
            LOG_IF(ERROR, !status.ok())
                << "Failed to upload the new encription key.";
          },
          std::move(signed_encryption_key), key_in_storage_.get()));
}

StatusOr<scoped_refptr<StorageQueue>> Storage::GetQueue(Priority priority) {
  auto it = queues_.find(priority);
  if (it == queues_.end()) {
    return Status(
        error::NOT_FOUND,
        base::StrCat({"Undefined priority=", base::NumberToString(priority)}));
  }
  return it->second;
}

}  // namespace reporting
