// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/storage.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/adapters.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "components/reporting/compression/compression_module.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/encryption/primitives.h"
#include "components/reporting/encryption/verification.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/storage/storage_configuration.h"
#include "components/reporting/storage/storage_queue.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/file.h"
#include "components/reporting/util/reporting_errors.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/task_runner_context.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl.h"

namespace reporting {

namespace {
constexpr base::FilePath::CharType kEncryptionKeyFilePrefix[] =
    FILE_PATH_LITERAL("EncryptionKey.");
constexpr int32_t kEncryptionKeyMaxFileSize = 256;
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
      UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
      scoped_refptr<EncryptionModuleInterface> encryption_module,
      UploaderInterface::UploadReason reason,
      UploaderInterfaceResultCb start_uploader_cb) {
    async_start_upload_cb.Run(
        (/*need_encryption_key=*/EncryptionModuleInterface::is_enabled() &&
         encryption_module->need_encryption_key())
            ? UploaderInterface::UploadReason::KEY_DELIVERY
            : reason,
        base::BindOnce(&QueueUploaderInterface::WrapInstantiatedUploader,
                       priority, std::move(start_uploader_cb)));
  }

  void ProcessRecord(EncryptedRecord encrypted_record,
                     ScopedReservation scoped_reservation,
                     base::OnceCallback<void(bool)> processed_cb) override {
    // Update sequence information: add Priority.
    SequenceInformation* const sequence_info =
        encrypted_record.mutable_sequence_information();
    sequence_info->set_priority(priority_);
    storage_interface_->ProcessRecord(std::move(encrypted_record),
                                      std::move(scoped_reservation),
                                      std::move(processed_cb));
  }

  void ProcessGap(SequenceInformation start,
                  uint64_t count,
                  base::OnceCallback<void(bool)> processed_cb) override {
    // Update sequence information: add Priority.
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
    if (!uploader_result.has_value()) {
      std::move(start_uploader_cb)
          .Run(base::unexpected(std::move(uploader_result).error()));
      return;
    }
    std::move(start_uploader_cb)
        .Run(std::make_unique<QueueUploaderInterface>(
            priority, std::move(uploader_result.value())));
  }

  const Priority priority_;
  const std::unique_ptr<UploaderInterface> storage_interface_;
};

class Storage::KeyDelivery {
 public:
  using RequestCallback = base::OnceCallback<void(Status)>;

  // Factory method, returns smart pointer with deletion on sequence.
  static std::unique_ptr<KeyDelivery, base::OnTaskRunnerDeleter> Create(
      UploaderInterface::AsyncStartUploaderCb async_start_upload_cb) {
    auto sequence_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::BEST_EFFORT, base::MayBlock()});
    return std::unique_ptr<KeyDelivery, base::OnTaskRunnerDeleter>(
        new KeyDelivery(async_start_upload_cb, sequence_task_runner),
        base::OnTaskRunnerDeleter(sequence_task_runner));
  }

  ~KeyDelivery() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    PostResponses(
        Status(error::UNAVAILABLE, "Key not delivered - Storage shuts down"));
  }

  void Request(RequestCallback callback) {
    sequenced_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&KeyDelivery::EuqueueRequestAndPossiblyStart,
                                  base::Unretained(this), std::move(callback)));
  }

  void OnCompletion(Status status) {
    sequenced_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&KeyDelivery::PostResponses,
                                  base::Unretained(this), status));
  }

 private:
  // Constructor called by factory only.
  explicit KeyDelivery(
      UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
      : sequenced_task_runner_(sequenced_task_runner),
        async_start_upload_cb_(async_start_upload_cb) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  void EuqueueRequestAndPossiblyStart(RequestCallback callback) {
    CHECK(callback);
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    const bool first_call = callbacks_.empty();
    callbacks_.push_back(std::move(callback));
    if (!first_call) {
      // Already started.
      return;
    }
    // The first request, starting the roundtrip.
    // Initiate upload with need_encryption_key flag and no records.
    UploaderInterface::UploaderInterfaceResultCb start_uploader_cb =
        base::BindOnce(&KeyDelivery::EncryptionKeyReceiverReady,
                       base::Unretained(this));
    async_start_upload_cb_.Run(
        UploaderInterface::UploadReason::KEY_DELIVERY,
        base::BindOnce(&KeyDelivery::WrapInstantiatedKeyUploader,
                       /*priority=*/MANUAL_BATCH,
                       std::move(start_uploader_cb)));
  }

  void PostResponses(Status status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    for (auto& callback : callbacks_) {
      std::move(callback).Run(status);
    }
    callbacks_.clear();
  }

  static void WrapInstantiatedKeyUploader(
      Priority priority,
      UploaderInterface::UploaderInterfaceResultCb start_uploader_cb,
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

  void EncryptionKeyReceiverReady(
      StatusOr<std::unique_ptr<UploaderInterface>> uploader_result) {
    if (!uploader_result.has_value()) {
      OnCompletion(uploader_result.error());
      return;
    }
    uploader_result.value()->Completed(Status::StatusOK());
  }

  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Upload provider callback.
  const UploaderInterface::AsyncStartUploaderCb async_start_upload_cb_;

  // List of all request callbacks.
  std::vector<RequestCallback> callbacks_ GUARDED_BY_CONTEXT(sequence_checker_);
};

class Storage::KeyInStorage {
 public:
  KeyInStorage(std::string_view signature_verification_public_key,
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
  DownloadKeyFile() {
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
    base::flat_set<base::FilePath> all_key_files;
    base::flat_map<uint64_t, base::FilePath> found_key_files;
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

  Status VerifySignature(const SignedEncryptionInfo& signed_encryption_key) {
    if (signed_encryption_key.public_asymmetric_key().size() != kKeySize) {
      return Status{error::FAILED_PRECONDITION, "Key size mismatch"};
    }
    std::string value_to_verify;
    const EncryptionModuleInterface::PublicKeyId public_key_id =
        signed_encryption_key.public_key_id();
    value_to_verify.assign(std::string_view(
        reinterpret_cast<const char*>(&public_key_id), sizeof(public_key_id)));
    value_to_verify.append(
        std::string_view(signed_encryption_key.public_asymmetric_key()));
    return verifier_.Verify(value_to_verify, signed_encryption_key.signature());
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
      base::UmaHistogramEnumeration(
          reporting::kUmaDataLossErrorReason,
          DataLossErrorReason::FAILED_TO_OPEN_KEY_FILE,
          DataLossErrorReason::MAX_VALUE);
      return Status(
          error::DATA_LOSS,
          base::StrCat({"Cannot open key file='", key_file_path.MaybeAsASCII(),
                        "' for append"}));
    }
    std::string serialized_key;
    if (!signed_encryption_key.SerializeToString(&serialized_key) ||
        serialized_key.empty()) {
      base::UmaHistogramEnumeration(
          reporting::kUmaDataLossErrorReason,
          DataLossErrorReason::FAILED_TO_SERIALIZE_KEY,
          DataLossErrorReason::MAX_VALUE);
      return Status(error::DATA_LOSS,
                    base::StrCat({"Failed to seralize key into file='",
                                  key_file_path.MaybeAsASCII(), "'"}));
    }
    const auto write_result = key_file.Write(
        /*offset=*/0, base::as_byte_span(serialized_key));
    if (!write_result.has_value() || write_result.value() < 0) {
      base::UmaHistogramEnumeration(
          reporting::kUmaDataLossErrorReason,
          DataLossErrorReason::FAILED_TO_WRITE_KEY_FILE,
          DataLossErrorReason::MAX_VALUE);
      return Status(
          error::DATA_LOSS,
          base::StrCat({"File write error=",
                        key_file.ErrorToString(key_file.GetLastFileError()),
                        " file=", key_file_path.MaybeAsASCII()}));
    }
    if (write_result.value() != serialized_key.size()) {
      base::UmaHistogramEnumeration(
          reporting::kUmaDataLossErrorReason,
          DataLossErrorReason::FAILED_TO_WRITE_KEY_FILE,
          DataLossErrorReason::MAX_VALUE);
      return Status(error::DATA_LOSS,
                    base::StrCat({"Failed to seralize key into file='",
                                  key_file_path.MaybeAsASCII(), "'"}));
    }
    return Status::StatusOK();
  }

  // Enumerates key files and deletes those with index lower than
  // |new_file_index|. Called during key upload.
  void RemoveKeyFilesWithLowerIndexes(uint64_t new_file_index) {
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
  void EnumerateKeyFiles(
      base::flat_set<base::FilePath>* all_key_files,
      base::flat_map<uint64_t, base::FilePath>* found_key_files) {
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
      const auto file_index =
          StorageQueue::GetFileSequenceIdFromPath(full_name);
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
        next_key_file_index_.store(
            static_cast<uint64_t>(file_index.value() + 1));
      }
    }
  }

  // Enumerates found key files and locates one with the highest index and
  // valid key. Returns pair of file name and loaded signed key proto.
  // Called once, during initialization.
  std::optional<std::pair<base::FilePath, SignedEncryptionInfo>>
  LocateValidKeyAndParse(
      const base::flat_map<uint64_t, base::FilePath>& found_key_files) {
    // Try to unserialize the key from each found file (latest first).
    for (const auto& [index, file_path] : base::Reversed(found_key_files)) {
      base::File key_file(file_path,
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
      if (!key_file.IsValid()) {
        continue;  // Could not open.
      }

      SignedEncryptionInfo signed_encryption_key;
      {
        std::array<uint8_t, kEncryptionKeyMaxFileSize> key_file_buffer;
        const auto read_result = key_file.Read(
            /*offset=*/0, key_file_buffer);
        if (!read_result.has_value() || read_result.value() < 0) {
          LOG(WARNING) << "File read error="
                       << key_file.ErrorToString(key_file.GetLastFileError())
                       << " " << file_path.MaybeAsASCII();
          continue;  // File read error.
        }
        if (read_result.value() == 0 ||
            read_result.value() >= kEncryptionKeyMaxFileSize) {
          continue;  // Unexpected file size.
        }
        google::protobuf::io::ArrayInputStream key_stream(  // Zero-copy stream.
            key_file_buffer.data(), read_result.value());
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
    scoped_refptr<CompressionModule> compression_module,
    base::OnceCallback<void(StatusOr<scoped_refptr<Storage>>)> completion_cb) {
  // Initialize Storage object, populating all the queues.
  class StorageInitContext
      : public TaskRunnerContext<StatusOr<scoped_refptr<Storage>>> {
   public:
    StorageInitContext(
        const StorageOptions::QueuesOptionsList& queues_options,
        scoped_refptr<Storage> storage,
        base::OnceCallback<void(StatusOr<scoped_refptr<Storage>>)> callback)
        : TaskRunnerContext<StatusOr<scoped_refptr<Storage>>>(
              std::move(callback),
              storage->sequenced_task_runner_),  // Same runner as the Storage!
          queues_options_(queues_options),
          storage_(std::move(storage)) {}

   private:
    // Context can only be deleted by calling Response method.
    ~StorageInitContext() override {
      DCHECK_CALLED_ON_VALID_SEQUENCE(storage_->sequence_checker_);
      CHECK_EQ(count_, 0u);
    }

    void OnStart() override {
      CheckOnValidSequence();

      // If encryption is not enabled, proceed with the queues.
      if (!EncryptionModuleInterface::is_enabled()) {
        InitAllQueues();
        return;
      }

      // Encryption is enabled. Locate the latest signed_encryption_key file
      // with matching key signature after deserialization.
      const auto download_key_result =
          storage_->key_in_storage_->DownloadKeyFile();
      if (!download_key_result.has_value()) {
        // Key not found or corrupt. Proceed with queues creation directly.
        // We will download the key on the first Enqueue.
        EncryptionSetUp(download_key_result.error());
        return;
      }

      // Key found, verified and downloaded.
      storage_->encryption_module_->UpdateAsymmetricKey(
          download_key_result.value().first, download_key_result.value().second,
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
        CHECK(storage_->encryption_module_->has_encryption_key());
      } else {
        LOG(WARNING)
            << "Encryption is enabled, but the key is not available yet, "
               "status="
            << status;
      }
      InitAllQueues();
    }

    void InitAllQueues() {
      CheckOnValidSequence();

      // Construct all queues.
      DCHECK_CALLED_ON_VALID_SEQUENCE(storage_->sequence_checker_);
      count_ = queues_options_.size();
      for (const auto& queue_options : queues_options_) {
        StorageQueue::Create(
            /*options=*/queue_options.second,
            // Note: the callback below belongs to the Queue and does not
            // outlive Storage, so it cannot refer to `storage_` itself!
            base::BindRepeating(&QueueUploaderInterface::AsyncProvideUploader,
                                /*priority=*/queue_options.first,
                                storage_->async_start_upload_cb_,
                                storage_->encryption_module_),
            storage_->encryption_module_, storage_->compression_module_,
            base::BindOnce(&StorageInitContext::ScheduleAddQueue,
                           base::Unretained(this),
                           /*priority=*/queue_options.first));
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
      DCHECK_CALLED_ON_VALID_SEQUENCE(storage_->sequence_checker_);
      if (storage_queue_result.has_value()) {
        auto add_result =
            storage_->queues_.emplace(priority, storage_queue_result.value());
        CHECK(add_result.second);
      } else {
        LOG(ERROR) << "Could not create queue, priority=" << priority
                   << ", status=" << storage_queue_result.error();
        if (final_status_.ok()) {
          final_status_ = storage_queue_result.error();
        }
      }
      CHECK_GT(count_, 0u);
      if (--count_ > 0u) {
        return;
      }
      if (!final_status_.ok()) {
        Response(base::unexpected(final_status_));
        return;
      }
      // Now all queues are ready, assign degradation vectors to them
      // in an ascending priorities order. The lowest priority queue has
      // an empty vector.
      std::vector<scoped_refptr<StorageQueue>> degradation_queues;
      CHECK_EQ(storage_->queues_.size(), queues_options_.size());
      for (const auto& queue_options : queues_options_) {
        const auto queue_or_error = storage_->GetQueue(queue_options.first);
        CHECK(queue_or_error.has_value()) << queue_or_error.error();
        queue_or_error.value()->AssignDegradationQueues(degradation_queues);
        // Add newly created queue to the list to be used by all the later ones.
        degradation_queues.emplace_back(queue_or_error.value());
      }

      Response(std::move(storage_));
    }

    const StorageOptions::QueuesOptionsList queues_options_;
    const scoped_refptr<Storage> storage_;
    size_t count_ GUARDED_BY_CONTEXT(storage_->sequence_checker_) = 0;
    Status final_status_;
  };

  // Create Storage object.
  // Cannot use base::MakeRefCounted<Storage>, because constructor is private.
  scoped_refptr<Storage> storage = base::WrapRefCounted(
      new Storage(options, encryption_module, compression_module,
                  std::move(async_start_upload_cb)));

  // Asynchronously run initialization.
  Start<StorageInitContext>(options.ProduceQueuesOptions(), std::move(storage),
                            std::move(completion_cb));
}

Storage::Storage(const StorageOptions& options,
                 scoped_refptr<EncryptionModuleInterface> encryption_module,
                 scoped_refptr<CompressionModule> compression_module,
                 UploaderInterface::AsyncStartUploaderCb async_start_upload_cb)
    : options_(options),
      encryption_module_(encryption_module),
      key_delivery_(KeyDelivery::Create(async_start_upload_cb)),
      compression_module_(compression_module),
      key_in_storage_(std::make_unique<KeyInStorage>(
          options.signature_verification_public_key(),
          options.directory())),
      async_start_upload_cb_(async_start_upload_cb),
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock()})) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

Storage::~Storage() = default;

void Storage::Write(Priority priority,
                    Record record,
                    base::OnceCallback<void(Status)> completion_cb) {
  AsyncGetQueueAndProceed(
      priority,
      base::BindOnce(
          [](scoped_refptr<Storage> self, Priority priority, Record record,
             scoped_refptr<StorageQueue> queue,
             base::OnceCallback<void(Status)> completion_cb) {
            if (EncryptionModuleInterface::is_enabled() &&
                !self->encryption_module_->has_encryption_key()) {
              // Key was not found at startup time. Note that if the key is
              // outdated, we still can't use it, and won't load it now. So
              // this processing can only happen after Storage is initialized
              // (until the first successful delivery of a key). After that we
              // will resume the write into the queue.
              KeyDelivery::RequestCallback action = base::BindOnce(
                  [](scoped_refptr<StorageQueue> queue, Record record,
                     base::OnceCallback<void(Status)> completion_cb,
                     Status status) {
                    if (!status.ok()) {
                      std::move(completion_cb).Run(status);
                      return;
                    }
                    queue->Write(std::move(record), std::move(completion_cb));
                  },
                  queue, std::move(record), std::move(completion_cb));
              self->key_delivery_->Request(std::move(action));
              return;
            }
            // Otherwise we can write into the queue right away.
            queue->Write(std::move(record), std::move(completion_cb));
          },
          base::WrapRefCounted(this), priority, std::move(record)),
      std::move(completion_cb));
}

void Storage::Confirm(SequenceInformation sequence_information,
                      bool force,
                      base::OnceCallback<void(Status)> completion_cb) {
  const Priority priority = sequence_information.priority();
  AsyncGetQueueAndProceed(
      priority,
      base::BindOnce(
          [](SequenceInformation sequence_information, bool force,
             scoped_refptr<StorageQueue> queue,
             base::OnceCallback<void(Status)> completion_cb) {
            queue->Confirm(std::move(sequence_information), force,
                           std::move(completion_cb));
          },
          std::move(sequence_information), force),
      std::move(completion_cb));
}

void Storage::Flush(Priority priority,
                    base::OnceCallback<void(Status)> completion_cb) {
  AsyncGetQueueAndProceed(
      priority,
      base::BindOnce([](scoped_refptr<StorageQueue> queue,
                        base::OnceCallback<void(Status)> completion_cb) {
        queue->Flush(std::move(completion_cb));
      }),
      std::move(completion_cb));
}

void Storage::UpdateEncryptionKey(SignedEncryptionInfo signed_encryption_key) {
  // Verify received key signature. Bail out if failed.
  const auto signature_verification_status =
      key_in_storage_->VerifySignature(signed_encryption_key);
  if (!signature_verification_status.ok()) {
    LOG(WARNING) << "Key failed verification, status="
                 << signature_verification_status;
    key_delivery_->OnCompletion(signature_verification_status);
    return;
  }

  // Assign the received key to encryption module.
  encryption_module_->UpdateAsymmetricKey(
      signed_encryption_key.public_asymmetric_key(),
      signed_encryption_key.public_key_id(),
      base::BindOnce(
          [](scoped_refptr<Storage> storage, Status status) {
            if (!status.ok()) {
              LOG(WARNING) << "Encryption key update failed, status=" << status;
              storage->key_delivery_->OnCompletion(status);
              return;
            }
            // Encryption key updated successfully.
            storage->key_delivery_->OnCompletion(Status::StatusOK());
          },
          base::WrapRefCounted(this)));

  // Serialize whole signed_encryption_key to a new file, discard the old
  // one(s). Do it on a thread which may block doing file operations.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(
          [](SignedEncryptionInfo signed_encryption_key,
             scoped_refptr<Storage> storage) {
            const Status status =
                storage->key_in_storage_->UploadKeyFile(signed_encryption_key);
            LOG_IF(ERROR, !status.ok())
                << "Failed to upload the new encription key.";
          },
          std::move(signed_encryption_key), base::WrapRefCounted(this)));
}

void Storage::AsyncGetQueueAndProceed(
    Priority priority,
    base::OnceCallback<void(scoped_refptr<StorageQueue>,
                            base::OnceCallback<void(Status)>)> queue_action,
    base::OnceCallback<void(Status)> completion_cb) {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<Storage> self, Priority priority,
             base::OnceCallback<void(scoped_refptr<StorageQueue>,
                                     base::OnceCallback<void(Status)>)>
                 queue_action,
             base::OnceCallback<void(Status)> completion_cb) {
            // Attempt to get queue by priority on the Storage task runner.
            auto queue_result = self->GetQueue(priority);
            if (!queue_result.has_value()) {
              // Queue not found, abort.
              std::move(completion_cb).Run(queue_result.error());
              return;
            }
            // Queue found, execute the action (it should relocate on
            // queue thread soon, to not block Storage task runner).
            std::move(queue_action)
                .Run(queue_result.value(), std::move(completion_cb));
          },
          base::WrapRefCounted(this), priority, std::move(queue_action),
          std::move(completion_cb)));
}

StatusOr<scoped_refptr<StorageQueue>> Storage::GetQueue(
    Priority priority) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = queues_.find(priority);
  if (it == queues_.end()) {
    return base::unexpected(Status(
        error::NOT_FOUND,
        base::StrCat({"Undefined priority=", base::NumberToString(priority)})));
  }
  return it->second;
}

void Storage::RegisterCompletionCallback(base::OnceClosure callback) {
  // Although this is an asynchronous action, note that Storage cannot be
  // destructed until the callback is registered - StorageQueue is held by added
  // reference here. Thus, the callback being registered is guaranteed
  // to be called when the Storage is being destructed.
  CHECK(callback);
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceClosure callback, scoped_refptr<Storage> self) {
            DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
            const base::RepeatingClosure queue_callback =
                base::BarrierClosure(self->queues_.size(), std::move(callback));
            for (auto& queue : self->queues_) {
              // Copy the callback as base::OnceClosure.
              queue.second->RegisterCompletionCallback(queue_callback);
            }
          },
          std::move(callback), base::WrapRefCounted(this)));
}
}  // namespace reporting
