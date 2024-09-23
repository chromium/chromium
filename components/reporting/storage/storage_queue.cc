// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/reporting/storage/storage_queue.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/reporting/compression/compression_module.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_managed_buffer.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/storage/storage_configuration.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/file.h"
#include "components/reporting/util/refcounted_closure_list.h"
#include "components/reporting/util/reporting_errors.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/task_runner_context.h"
#include "crypto/random.h"
#include "crypto/sha2.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace reporting {

// Temporary: enable/disable Storage degradation.
// When enabled: If there is no disk space available at the time of writing a
// new record then the storage queue will try to shed files sequentially from
// lowest to highest priority.
BASE_FEATURE(kReportingStorageDegradationFeature,
             "ReportingStorageDegradation",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

// Metadata file name prefix.
const base::FilePath::CharType METADATA_NAME[] = FILE_PATH_LITERAL("META");

// The size in bytes that all files and records are rounded to (for privacy:
// make it harder to differ between kinds of records).
constexpr size_t FRAME_SIZE = 16u;

// Helper functions for FRAME_SIZE alignment support.
size_t RoundUpToFrameSize(size_t size) {
  return (size + FRAME_SIZE - 1) / FRAME_SIZE * FRAME_SIZE;
}

// Internal structure of the record header. Must fit in FRAME_SIZE.
struct RecordHeader {
  int64_t record_sequencing_id;
  uint32_t record_size;  // Size of the blob, not including RecordHeader
  uint32_t record_hash;  // Hash of the blob, not including RecordHeader
  // Data starts right after the header.

  // Sum of the sizes of individual members.
  static constexpr size_t kSize =
      sizeof(record_sequencing_id) + sizeof(record_size) + sizeof(record_hash);

  // Serialize to string. This does not guarantee same results across
  // devices, but on the same device the result should always be consistent
  // even if compiler behavior changes.
  [[nodiscard]] std::string SerializeToString() const {
    std::string serialized;
    serialized.reserve(sizeof(record_sequencing_id) + sizeof(record_size) +
                       sizeof(record_hash));
    serialized.append(reinterpret_cast<const char*>(&record_sequencing_id),
                      sizeof(record_sequencing_id));
    serialized.append(reinterpret_cast<const char*>(&record_size),
                      sizeof(record_size));
    serialized.append(reinterpret_cast<const char*>(&record_hash),
                      sizeof(record_hash));
    return serialized;
  }

  // Construct from a serialized string. This does not guarantee same results
  // across devices, but on the same device the result should always be
  // consistent even compiler behavior changes.
  [[nodiscard]] static StatusOr<RecordHeader> FromString(std::string_view s) {
    if (s.size() < kSize) {
      return base::unexpected(Status(error::INTERNAL, "header is corrupt"));
    }

    RecordHeader header;
    const char* p = s.data();
    header.record_sequencing_id = *reinterpret_cast<const int64_t*>(p);
    if (header.record_sequencing_id < 0) {
      return base::unexpected(Status(error::INTERNAL, "header is corrupt"));
    }
    p += sizeof(header.record_sequencing_id);
    header.record_size = *reinterpret_cast<const int32_t*>(p);
    p += sizeof(header.record_size);
    header.record_hash = *reinterpret_cast<const int32_t*>(p);

    return header;
  }
};
}  // namespace

// static
void StorageQueue::Create(
    const QueueOptions& options,
    UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
    scoped_refptr<EncryptionModuleInterface> encryption_module,
    scoped_refptr<CompressionModule> compression_module,
    base::OnceCallback<void(StatusOr<scoped_refptr<StorageQueue>>)>
        completion_cb) {
  // Initialize StorageQueue object loading the data.
  class StorageQueueInitContext
      : public TaskRunnerContext<StatusOr<scoped_refptr<StorageQueue>>> {
   public:
    StorageQueueInitContext(
        scoped_refptr<StorageQueue> storage_queue,
        base::OnceCallback<void(StatusOr<scoped_refptr<StorageQueue>>)>
            callback)
        : TaskRunnerContext<StatusOr<scoped_refptr<StorageQueue>>>(
              std::move(callback),
              storage_queue->sequenced_task_runner_),
          storage_queue_(std::move(storage_queue)) {
      CHECK(storage_queue_);
    }

   private:
    // Context can only be deleted by calling Response method.
    ~StorageQueueInitContext() override = default;

    void OnStart() override {
      auto init_status = storage_queue_->Init();
      if (!init_status.ok()) {
        Response(base::unexpected(std::move(init_status)));
        return;
      }
      Response(std::move(storage_queue_));
    }

    scoped_refptr<StorageQueue> storage_queue_;
  };

  auto sequenced_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT, base::MayBlock()});

  // Create StorageQueue object.
  // Cannot use base::MakeRefCounted<StorageQueue>, because constructor is
  // private.
  scoped_refptr<StorageQueue> storage_queue = base::WrapRefCounted(
      new StorageQueue(std::move(sequenced_task_runner), options,
                       std::move(async_start_upload_cb), encryption_module,
                       compression_module));

  // Asynchronously run initialization.
  Start<StorageQueueInitContext>(std::move(storage_queue),
                                 std::move(completion_cb));
}

StorageQueue::StorageQueue(
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
    const QueueOptions& options,
    UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
    scoped_refptr<EncryptionModuleInterface> encryption_module,
    scoped_refptr<CompressionModule> compression_module)
    : base::RefCountedDeleteOnSequence<StorageQueue>(sequenced_task_runner),
      sequenced_task_runner_(sequenced_task_runner),
      completion_closure_list_(
          base::MakeRefCounted<RefCountedClosureList>(sequenced_task_runner)),
      low_priority_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock()})),
      options_(options),
      async_start_upload_cb_(async_start_upload_cb),
      encryption_module_(encryption_module),
      compression_module_(compression_module) {
  DETACH_FROM_SEQUENCE(storage_queue_sequence_checker_);
}

StorageQueue::~StorageQueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);

  // Stop timers.
  upload_timer_.AbandonAndStop();
  check_back_timer_.AbandonAndStop();
  // Make sure no pending writes is present.
  CHECK(write_contexts_queue_.empty());

  // Release all files.
  ReleaseAllFileInstances();
}

void StorageQueue::AssignDegradationQueues(
    const std::vector<scoped_refptr<StorageQueue>>& degradation_queues) {
  CHECK(degradation_queues_.empty()) << "Can only be assigned once";
  degradation_queues_ = degradation_queues;
}

Status StorageQueue::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  // Make sure the assigned directory exists.
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(options_.directory(), &error)) {
    base::UmaHistogramEnumeration(
        reporting::kUmaUnavailableErrorReason,
        UnavailableErrorReason::FAILED_TO_CREATE_STORAGE_QUEUE_DIRECTORY,
        UnavailableErrorReason::MAX_VALUE);
    return Status(
        error::UNAVAILABLE,
        base::StrCat(
            {"Storage queue directory '", options_.directory().MaybeAsASCII(),
             "' does not exist, error=", base::File::ErrorToString(error)}));
  }
  CHECK_LE(generation_id_, 0);  // Not yet set within valid range [1, max_int64]
  base::flat_set<base::FilePath> used_files_set;
  // Enumerate data files and scan the last one to determine what sequence
  // ids do we have (first and last).
  RETURN_IF_ERROR_STATUS(EnumerateDataFiles(&used_files_set));
  RETURN_IF_ERROR_STATUS(ScanLastFile());
  if (next_sequencing_id_ > 0) {
    // Enumerate metadata files to determine what sequencing ids have
    // last record digest. They might have metadata for sequencing ids
    // beyond what data files had, because metadata is written ahead of the
    // data, but must have metadata for the last data, because metadata is only
    // removed once data is written. So we are picking the metadata matching the
    // last sequencing id and load both digest and generation id from there.
    const Status status = RestoreMetadata(&used_files_set);
    // If there is no match and we cannot recover generation id, clear up
    // everything we've found before and start a new generation from scratch.
    // In the future we could possibly consider preserving the previous
    // generation data, but will need to resolve multiple issues:
    // 1) we would need to send the old generation before starting to send
    //    the new one, which could trigger a loss of data in the new generation.
    // 2) we could end up with 3 or more generations, if the loss of metadata
    //    repeats. Which of them should be sent first (which one is expected
    //    by the server)?
    // 3) different generations might include the same sequencing ids;
    //    how do we resolve file naming then? Should we add generation id
    //    to the file name too?
    // Because of all this, for now we just drop the old generation data
    // and start the new one from scratch.
    if (!status.ok()) {
      LOG(ERROR) << "Failed to restore metadata, status=" << status;
      // If generation id is also unknown, reset all parameters as they were
      // at the beginning of Init(). Some of them might have been changed
      // earlier.
      if (generation_id_ <= 0) {
        LOG(ERROR) << "Unable to retrieve generation id, performing full reset";
        next_sequencing_id_ = 0;
        first_sequencing_id_ = 0;
        first_unconfirmed_sequencing_id_ = std::nullopt;
        last_record_digest_ = std::nullopt;
        ReleaseAllFileInstances();
        used_files_set.clear();
      }
    }
  }
  // In case of unavailability default to a new generation id being a random
  // number [1, max_int64].
  if (generation_id_ <= 0) {
    generation_id_ =
        1 + base::RandGenerator(std::numeric_limits<int64_t>::max());
  }
  // Delete all files except used ones.
  DeleteUnusedFiles(used_files_set);
  // Initiate periodic uploading, if needed (IMMEDIATE, SECURITY and MANUAL
  // priorities do not need it - they are created with 0, 0 and infinite period
  // respectively).
  //
  if (!options_.upload_period().is_zero() &&
      !options_.upload_period().is_max()) {
    upload_timer_.Start(FROM_HERE, options_.upload_period(),
                        base::BindRepeating(&StorageQueue::PeriodicUpload,
                                            weakptr_factory_.GetWeakPtr()));
  }
  // In case some events are found in the queue, initiate an upload.
  // This is especially imporant for non-periodic queues, but won't harm
  // others either.
  if (first_sequencing_id_ < next_sequencing_id_) {
    Start<ReadContext>(UploaderInterface::UploadReason::INIT_RESUME,
                       base::DoNothing(), this);
  }
  return Status::StatusOK();
}

std::optional<std::string> StorageQueue::GetLastRecordDigest() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  // Attach last record digest, if present.
  return last_record_digest_;
}

Status StorageQueue::SetOrConfirmGenerationId(const base::FilePath& full_name) {
  // Data file should have generation id as an extension too.
  // TODO(b/195786943): Encapsulate file naming assumptions in objects.
  const auto generation_extension =
      full_name.RemoveFinalExtension().FinalExtension();
  if (generation_extension.empty()) {
    base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                  DataLossErrorReason::MISSING_GENERATION_ID,
                                  DataLossErrorReason::MAX_VALUE);
    return Status(error::DATA_LOSS,
                  base::StrCat({"Data file generation id not found in path: '",
                                full_name.MaybeAsASCII()}));
  }

  int64_t file_generation_id = 0;
  const bool success =
      base::StringToInt64(generation_extension.substr(1), &file_generation_id);
  if (!success || file_generation_id <= 0) {
    base::UmaHistogramEnumeration(
        reporting::kUmaDataLossErrorReason,
        DataLossErrorReason::FAILED_TO_PASE_GENERATION_ID,
        DataLossErrorReason::MAX_VALUE);
    return Status(error::DATA_LOSS,
                  base::StrCat({"Data file generation id corrupt: '",
                                full_name.MaybeAsASCII()}));
  }

  // Found valid generation [1, int64_max] in the data file name.
  if (generation_id_ > 0) {
    // Generation was already set, data file must match.
    if (file_generation_id != generation_id_) {
      base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                    DataLossErrorReason::INVALID_GENERATION_ID,
                                    DataLossErrorReason::MAX_VALUE);
      return Status(error::DATA_LOSS,
                    base::StrCat({"Data file generation id does not match: '",
                                  full_name.MaybeAsASCII(), "', expected=",
                                  base::NumberToString(generation_id_)}));
    }
  } else {
    // No generation set in the queue. Use the one from this file and expect
    // all other files to match.
    generation_id_ = file_generation_id;
  }
  return Status::StatusOK();
}

StatusOr<int64_t> StorageQueue::GetFileSequenceIdFromPath(
    const base::FilePath& file_name) {
  const auto extension = file_name.FinalExtension();
  if (extension.empty() || extension == FILE_PATH_LITERAL(".")) {
    return base::unexpected(
        Status(error::INTERNAL, base::StrCat({"File has no extension: '",
                                              file_name.MaybeAsASCII(), "'"})));
  }
  int64_t file_sequence_id = 0;
  const bool success =
      base::StringToInt64(extension.substr(1), &file_sequence_id);
  if (!success) {
    return base::unexpected(Status(
        error::INTERNAL, base::StrCat({"File extension does not parse: '",
                                       file_name.MaybeAsASCII(), "'"})));
  }

  return file_sequence_id;
}

StatusOr<int64_t> StorageQueue::AddDataFile(
    const base::FilePath& full_name,
    const base::FileEnumerator::FileInfo& file_info) {
  ASSIGN_OR_RETURN(int64_t file_sequence_id,
                   GetFileSequenceIdFromPath(full_name));

  auto file_or_status = SingleFile::Create(
      full_name, file_info.GetSize(), options_.memory_resource(),
      options_.disk_space_resource(), completion_closure_list_);
  if (!file_or_status.has_value()) {
    return base::unexpected(std::move(file_or_status).error());
  }
  if (!files_.emplace(file_sequence_id, file_or_status.value()).second) {
    return base::unexpected(Status(
        error::ALREADY_EXISTS, base::StrCat({"Sequencing id duplicated: '",
                                             full_name.MaybeAsASCII(), "'"})));
  }
  return file_sequence_id;
}

Status StorageQueue::EnumerateDataFiles(
    base::flat_set<base::FilePath>* used_files_set) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  // We need to set first_sequencing_id_ to 0 if this is the initialization
  // of an empty StorageQueue, and to the lowest sequencing id among all
  // existing files, if it was already used.
  std::optional<int64_t> first_sequencing_id;
  base::FileEnumerator dir_enum(
      options_.directory(),
      /*recursive=*/false, base::FileEnumerator::FILES,
      base::StrCat({options_.file_prefix(), FILE_PATH_LITERAL(".*")}));

  bool found_files_in_directory = false;

  for (auto full_name = dir_enum.Next(); !full_name.empty();
       full_name = dir_enum.Next()) {
    found_files_in_directory = true;
    // Try to parse a generation id from `full_name` and either set
    // `generation_id_` or confirm that the generation id matches
    // `generation_id_`
    if (auto status = SetOrConfirmGenerationId(full_name); !status.ok()) {
      LOG(WARNING) << "Failed to add file " << full_name.MaybeAsASCII()
                   << ", status=" << status;
      continue;
    }
    // Add file to `files_` if the sequence id in the file path is valid
    const auto file_sequencing_id_result =
        AddDataFile(full_name, dir_enum.GetInfo());
    if (!file_sequencing_id_result.has_value()) {
      LOG(WARNING) << "Failed to add file " << full_name.MaybeAsASCII()
                   << ", status=" << file_sequencing_id_result.error();
      continue;
    }
    used_files_set->emplace(full_name);  // File is in use.
    if (!first_sequencing_id.has_value() ||
        first_sequencing_id.value() > file_sequencing_id_result.value()) {
      first_sequencing_id = file_sequencing_id_result.value();
    }
  }

  // If there were files in the queue directory, but we haven't found a
  // generation id in any of the file paths, then the data is corrupt and we
  // shouldn't proceed.
  if (found_files_in_directory && generation_id_ <= 0) {
    base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                  DataLossErrorReason::INVALID_GENERATION_ID,
                                  DataLossErrorReason::MAX_VALUE);
    base::UmaHistogramEnumeration(
        reporting::kUmaDataLossErrorReason,
        DataLossErrorReason::ALL_FILE_PATHS_MISSING_GENERATION_ID,
        DataLossErrorReason::MAX_VALUE);
    return Status(
        error::DATA_LOSS,
        base::StrCat({"All file paths missing generation id in directory",
                      options_.directory().MaybeAsASCII()}));
  }
  // first_sequencing_id.has_value() is true only if we found some files.
  // Otherwise it is false, the StorageQueue is being initialized for the
  // first time, and we need to set first_sequencing_id_ to 0.
  first_sequencing_id_ =
      first_sequencing_id.has_value() ? first_sequencing_id.value() : 0;
  return Status::StatusOK();
}

Status StorageQueue::ScanLastFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  next_sequencing_id_ = 0;
  if (files_.empty()) {
    return Status::StatusOK();
  }
  next_sequencing_id_ = files_.rbegin()->first;
  // Scan the file. Open it and leave open, because it might soon be needed
  // again (for the next or repeated Upload), and we won't waste time closing
  // and reopening it. If the file remains open for too long, it will auto-close
  // by timer.
  scoped_refptr<SingleFile> last_file = files_.rbegin()->second.get();
  auto open_status = last_file->Open(/*read_only=*/false);
  if (!open_status.ok()) {
    LOG(ERROR) << "Error opening file " << last_file->name()
               << ", status=" << open_status;
    base::UmaHistogramEnumeration(
        reporting::kUmaDataLossErrorReason,
        DataLossErrorReason::FAILED_TO_OPEN_STORAGE_QUEUE_FILE,
        DataLossErrorReason::MAX_VALUE);
    return Status(error::DATA_LOSS, base::StrCat({"Error opening file: '",
                                                  last_file->name(), "'"}));
  }
  const size_t max_buffer_size =
      RoundUpToFrameSize(options_.max_record_size()) +
      RoundUpToFrameSize(RecordHeader::kSize);
  uint32_t pos = 0;
  for (;;) {
    // Read the header
    auto read_result =
        last_file->Read(pos, RecordHeader::kSize, max_buffer_size,
                        /*expect_readonly=*/false);
    if (!read_result.has_value() &&
        read_result.error().error_code() == error::OUT_OF_RANGE) {
      // End of file detected.
      break;
    }
    if (!read_result.has_value()) {
      // Error detected.
      LOG(ERROR) << "Error reading file " << last_file->name()
                 << ", status=" << read_result.error();
      break;
    }
    pos += read_result.value().size();
    // Copy out the header, since the buffer might be overwritten later on.
    const auto header_status = RecordHeader::FromString(read_result.value());
    if (!header_status.has_value()) {
      // Error detected.
      LOG(ERROR) << "Incomplete record header in file " << last_file->name();
      break;
    }
    const auto header = std::move(header_status.value());
    // Read the data (rounded to frame size).
    const size_t data_size = RoundUpToFrameSize(header.record_size);
    read_result = last_file->Read(pos, data_size, max_buffer_size,
                                  /*expect_readonly=*/false);
    if (!read_result.has_value()) {
      // Error detected.
      LOG(ERROR) << "Error reading file " << last_file->name()
                 << ", status=" << read_result.error();
      break;
    }
    pos += read_result.value().size();
    if (read_result.value().size() < data_size) {
      // Error detected.
      LOG(ERROR) << "Incomplete record in file " << last_file->name();
      break;
    }
    // Verify sequencing id.
    if (header.record_sequencing_id != next_sequencing_id_) {
      LOG(ERROR) << "sequencing id mismatch, expected=" << next_sequencing_id_
                 << ", actual=" << header.record_sequencing_id << ", file "
                 << last_file->name();
      break;
    }
    // Verify record hash.
    uint32_t actual_record_hash =
        base::PersistentHash(read_result.value().substr(0, header.record_size));
    if (header.record_hash != actual_record_hash) {
      LOG(ERROR) << "Hash mismatch, seq=" << header.record_sequencing_id
                 << " actual_hash=" << std::hex << actual_record_hash
                 << " expected_hash=" << std::hex << header.record_hash;
      break;
    }
    // Everything looks all right. Advance the sequencing id.
    ++next_sequencing_id_;
  }
  return Status::StatusOK();
}

StatusOr<scoped_refptr<StorageQueue::SingleFile>> StorageQueue::AssignLastFile(
    size_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  if (files_.empty()) {
    // Create the very first file (empty).
    ASSIGN_OR_RETURN(
        scoped_refptr<SingleFile> file,
        SingleFile::Create(
            options_.directory()
                .Append(options_.file_prefix())
                .AddExtensionASCII(base::NumberToString(generation_id_))
                .AddExtensionASCII(base::NumberToString(next_sequencing_id_)),
            /*size=*/0, options_.memory_resource(),
            options_.disk_space_resource(), completion_closure_list_));
    next_sequencing_id_ = 0;
    auto insert_result = files_.emplace(next_sequencing_id_, file);
    CHECK(insert_result.second);
  }
  if (size > options_.max_record_size()) {
    return base::unexpected(
        Status(error::OUT_OF_RANGE, "Too much data to be recorded at once"));
  }
  scoped_refptr<SingleFile> last_file = files_.rbegin()->second;
  if (last_file->size() > 0 &&  // Cannot have a file with no records.
      last_file->size() + size + RecordHeader::kSize + FRAME_SIZE >
          options_.max_single_file_size()) {
    // The last file will become too large, asynchronously close it and add
    // new.
    last_file->Close();
    ASSIGN_OR_RETURN(last_file, OpenNewWriteableFile());
  }
  return last_file;
}

StatusOr<scoped_refptr<StorageQueue::SingleFile>>
StorageQueue::OpenNewWriteableFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  ASSIGN_OR_RETURN(
      scoped_refptr<SingleFile> new_file,
      SingleFile::Create(
          options_.directory()
              .Append(options_.file_prefix())
              .AddExtensionASCII(base::NumberToString(generation_id_))
              .AddExtensionASCII(base::NumberToString(next_sequencing_id_)),
          /*size=*/0, options_.memory_resource(),
          options_.disk_space_resource(), completion_closure_list_));
  RETURN_IF_ERROR_STATUS(base::unexpected(new_file->Open(/*read_only=*/false)));
  auto insert_result = files_.emplace(next_sequencing_id_, new_file);
  if (!insert_result.second) {
    return base::unexpected(
        Status(error::ALREADY_EXISTS,
               base::StrCat({"Sequencing id already assigned: '",
                             base::NumberToString(next_sequencing_id_), "'"})));
  }
  return new_file;
}

Status StorageQueue::WriteHeaderAndBlock(
    std::string_view data,
    std::string_view current_record_digest,
    scoped_refptr<StorageQueue::SingleFile> file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);

  // Test only: Simulate failure if requested
  if (test_injection_handler_) {
    RETURN_IF_ERROR_STATUS(test_injection_handler_.Run(
        test::StorageQueueOperationKind::kWriteBlock, next_sequencing_id_));
  }

  // Prepare header.
  RecordHeader header;
  // Pad to the whole frame, if necessary.
  const size_t total_size =
      RoundUpToFrameSize(RecordHeader::kSize + data.size());
  // Assign sequencing id.
  header.record_sequencing_id = next_sequencing_id_++;
  header.record_hash = base::PersistentHash(data);
  header.record_size = data.size();
  // Store last record digest.
  last_record_digest_.emplace(current_record_digest);
  // Write to the last file, update sequencing id.
  auto open_status = file->Open(/*read_only=*/false);
  if (!open_status.ok()) {
    return Status(error::ALREADY_EXISTS,
                  base::StrCat({"Cannot open file=", file->name(),
                                " status=", open_status.ToString()}));
  }
  // The space for this append is being reserved in
  // StorageQueue::ReserveNewRecordDiskSpace.
  if (active_write_reservation_size_ < total_size) {
    return Status(
        error::RESOURCE_EXHAUSTED,
        base::StrCat({"Not enough disk space available to write into file=",
                      file->name()}));
  }
  active_write_reservation_size_ -= total_size;

  auto write_status = file->Append(header.SerializeToString());
  if (!write_status.has_value()) {
    return Status(error::RESOURCE_EXHAUSTED,
                  base::StrCat({"Cannot write file=", file->name(),
                                " status=", write_status.error().ToString()}));
  }
  if (data.size() > 0) {
    write_status = file->Append(data);
    if (!write_status.has_value()) {
      return Status(
          error::RESOURCE_EXHAUSTED,
          base::StrCat({"Cannot write file=", file->name(),
                        " status=", write_status.error().ToString()}));
    }
  }
  if (total_size > RecordHeader::kSize + data.size()) {
    // Fill in with random bytes.
    const size_t pad_size = total_size - (RecordHeader::kSize + data.size());
    uint8_t junk_bytes[FRAME_SIZE];
    auto padding = base::span(junk_bytes).first(pad_size);
    crypto::RandBytes(padding);
    write_status = file->Append(base::as_string_view(padding));
    if (!write_status.has_value()) {
      return Status(error::RESOURCE_EXHAUSTED,
                    base::StrCat({"Cannot pad file=", file->name(), " status=",
                                  write_status.error().ToString()}));
    }
  }
  return Status::StatusOK();
}

Status StorageQueue::WriteMetadata(std::string_view current_record_digest) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);

  // Test only: Simulate failure if requested
  if (test_injection_handler_) {
    RETURN_IF_ERROR_STATUS(test_injection_handler_.Run(
        test::StorageQueueOperationKind::kWriteMetadata, next_sequencing_id_));
  }

  // Synchronously write the metafile.
  ASSIGN_OR_RETURN(
      scoped_refptr<SingleFile> meta_file,
      SingleFile::Create(
          options_.directory()
              .Append(METADATA_NAME)
              .AddExtensionASCII(base::NumberToString(next_sequencing_id_)),
          /*size=*/0, options_.memory_resource(),
          options_.disk_space_resource(), completion_closure_list_));
  RETURN_IF_ERROR_STATUS(meta_file->Open(/*read_only=*/false));

  // The space for this following Append is being reserved in
  // StorageQueue::ReserveNewRecordDiskSpace.
  if (active_write_reservation_size_ <
      sizeof(generation_id_) + current_record_digest.size()) {
    return Status(
        error::RESOURCE_EXHAUSTED,
        base::StrCat({"Not enough disk space available to write into file=",
                      meta_file->name()}));
  }
  active_write_reservation_size_ -=
      sizeof(generation_id_) + current_record_digest.size();

  // Metadata file format is:
  // - generation id (8 bytes)
  // - last record digest (crypto::kSHA256Length bytes)
  // Write generation id.
  auto append_result = meta_file->Append(std::string_view(
      reinterpret_cast<const char*>(&generation_id_), sizeof(generation_id_)));
  if (!append_result.has_value()) {
    return Status(error::RESOURCE_EXHAUSTED,
                  base::StrCat({"Cannot write metafile=", meta_file->name(),
                                " status=", append_result.error().ToString()}));
  }
  // Write last record digest.
  append_result = meta_file->Append(current_record_digest);
  if (!append_result.has_value()) {
    return Status(error::RESOURCE_EXHAUSTED,
                  base::StrCat({"Cannot write metafile=", meta_file->name(),
                                " status=", append_result.error().ToString()}));
  }
  if (append_result.value() != current_record_digest.size()) {
    base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                  DataLossErrorReason::FAILED_TO_WRITE_METADATA,
                                  DataLossErrorReason::MAX_VALUE);
    return Status(error::DATA_LOSS, base::StrCat({"Failure writing metafile=",
                                                  meta_file->name()}));
  }
  meta_file->Close();
  // Asynchronously delete all earlier metafiles. Do not wait for this to
  // happen.
  low_priority_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StorageQueue::DeleteOutdatedMetadata, this,
                                next_sequencing_id_));
  return Status::StatusOK();
}

Status StorageQueue::ReadMetadata(
    const base::FilePath& meta_file_path,
    size_t size,
    int64_t sequencing_id,
    base::flat_set<base::FilePath>* used_files_set) {
  ASSIGN_OR_RETURN(
      scoped_refptr<SingleFile> meta_file,
      SingleFile::Create(meta_file_path, size, options_.memory_resource(),
                         options_.disk_space_resource(),
                         completion_closure_list_));
  RETURN_IF_ERROR_STATUS(meta_file->Open(/*read_only=*/true));
  // Metadata file format is:
  // - generation id (8 bytes)
  // - last record digest (crypto::kSHA256Length bytes)
  // Read generation id.
  constexpr size_t max_buffer_size =
      sizeof(generation_id_) + crypto::kSHA256Length;
  auto read_result =
      meta_file->Read(/*pos=*/0, sizeof(generation_id_), max_buffer_size);
  if (!read_result.has_value() ||
      read_result.value().size() != sizeof(generation_id_)) {
    base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                  DataLossErrorReason::FAILED_TO_READ_METADATA,
                                  DataLossErrorReason::MAX_VALUE);
    return Status(error::DATA_LOSS,
                  base::StrCat({"Cannot read metafile=", meta_file->name(),
                                " status=", read_result.error().ToString()}));
  }
  const int64_t generation_id =
      *reinterpret_cast<const int64_t*>(read_result.value().data());
  if (generation_id <= 0) {
    // Generation is not in [1, max_int64] range - file corrupt or empty.
    base::UmaHistogramEnumeration(
        reporting::kUmaDataLossErrorReason,
        DataLossErrorReason::METADATA_GENERATION_ID_OUT_OF_RANGE,
        DataLossErrorReason::MAX_VALUE);
    return Status(error::DATA_LOSS,
                  base::StrCat({"Corrupt or empty metafile=", meta_file->name(),
                                " - invalid generation ",
                                base::NumberToString(generation_id)}));
  }
  if (generation_id_ > 0 && generation_id != generation_id_) {
    // Generation has already been set, and meta file does not match it - file
    // corrupt or empty.
    base::UmaHistogramEnumeration(
        reporting::kUmaDataLossErrorReason,
        DataLossErrorReason::METADATA_GENERATION_MISMATCH,
        DataLossErrorReason::MAX_VALUE);
    return Status(
        error::DATA_LOSS,
        base::StrCat({"Corrupt or empty metafile=", meta_file->name(),
                      " - generation mismatch ",
                      base::NumberToString(generation_id),
                      ", expected=", base::NumberToString(generation_id_)}));
  }
  // Read last record digest.
  read_result = meta_file->Read(/*pos=*/sizeof(generation_id),
                                crypto::kSHA256Length, max_buffer_size);
  if (!read_result.has_value() ||
      read_result.value().size() != crypto::kSHA256Length) {
    base::UmaHistogramEnumeration(
        reporting::kUmaDataLossErrorReason,
        DataLossErrorReason::METADATA_LAST_RECORD_DIGEST_IS_CORRUPT,
        DataLossErrorReason::MAX_VALUE);
    return Status(error::DATA_LOSS,
                  base::StrCat({"Cannot read metafile=", meta_file->name(),
                                " status=", read_result.error().ToString()}));
  }
  // Everything read successfully, set the queue up.
  if (generation_id_ <= 0) {
    generation_id_ = generation_id;
  }
  if (sequencing_id == next_sequencing_id_ - 1) {
    // Record last digest only if the metadata matches
    // the latest sequencing id.
    last_record_digest_.emplace(read_result.value());
  }
  meta_file->Close();
  // Store used metadata file.
  used_files_set->emplace(meta_file_path);
  return Status::StatusOK();
}

Status StorageQueue::RestoreMetadata(
    base::flat_set<base::FilePath>* used_files_set) {
  // Enumerate all meta-files into a map sequencing_id->file_path.
  std::map<int64_t, std::pair<base::FilePath, size_t>> meta_files;
  base::FileEnumerator dir_enum(
      options_.directory(),
      /*recursive=*/false, base::FileEnumerator::FILES,
      base::StrCat({METADATA_NAME, FILE_PATH_LITERAL(".*")}));
  for (auto full_name = dir_enum.Next(); !full_name.empty();
       full_name = dir_enum.Next()) {
    const auto file_sequence_id =
        GetFileSequenceIdFromPath(dir_enum.GetInfo().GetName());
    if (!file_sequence_id.has_value()) {
      continue;
    }

    // Record file name and size. Ignore the result.
    meta_files.emplace(file_sequence_id.value(),
                       std::make_pair(full_name, dir_enum.GetInfo().GetSize()));
  }
  // See whether we have a match for next_sequencing_id_ - 1.
  CHECK_GT(next_sequencing_id_, 0u);
  auto it = meta_files.find(next_sequencing_id_ - 1);
  if (it != meta_files.end()) {
    // Match found. Attempt to load the metadata.
    const auto status =
        ReadMetadata(/*meta_file_path=*/it->second.first,
                     /*size=*/it->second.second,
                     /*sequencing_id=*/next_sequencing_id_ - 1, used_files_set);
    if (status.ok()) {
      return status;
    }
    // Failed to load, remove it from the candidates.
    meta_files.erase(it);
  }
  // No match or failed to load. Let's locate any valid metadata file (from
  // latest to earilest) and use generation from there (last record digest is
  // useless in that case).
  for (const auto& [sequencing_id, path_and_size] :
       base::Reversed(meta_files)) {
    const auto& [path, size] = path_and_size;
    const auto status = ReadMetadata(path, size, sequencing_id, used_files_set);
    if (status.ok()) {
      return status;
    }
  }
  // No valid metadata found. Cannot recover from that.
  base::UmaHistogramEnumeration(
      reporting::kUmaDataLossErrorReason,
      DataLossErrorReason::FAILED_TO_RESTORE_LAST_RECORD_DIGEST,
      DataLossErrorReason::MAX_VALUE);
  return Status(error::DATA_LOSS,
                base::StrCat({"Cannot recover last record digest at ",
                              base::NumberToString(next_sequencing_id_ - 1)}));
}  // namespace reporting

void StorageQueue::DeleteUnusedFiles(
    const base::flat_set<base::FilePath>& used_files_set) const {
  // Note, that these files were not reserved against disk allowance and do not
  // need to be discarded.
  // If the deletion of a file fails, the file will be naturally handled next
  // time.
  base::FileEnumerator dir_enum(options_.directory(),
                                /*recursive=*/true,
                                base::FileEnumerator::FILES);
  DeleteFilesWarnIfFailed(
      dir_enum, base::BindRepeating(
                    [](const base::flat_set<base::FilePath>* used_files_set,
                       const base::FilePath& full_name) {
                      return !used_files_set->contains(full_name);
                    },
                    &used_files_set));
}

void StorageQueue::DeleteOutdatedMetadata(int64_t sequencing_id_to_keep) const {
  // Delete file on disk. Note: disk space has already been released when the
  // metafile was destructed, and so we don't need to do that here.
  // If the deletion of a file fails, the file will be naturally handled next
  // time.
  base::FileEnumerator dir_enum(
      options_.directory(),
      /*recursive=*/false, base::FileEnumerator::FILES,
      base::StrCat({METADATA_NAME, FILE_PATH_LITERAL(".*")}));
  DeleteFilesWarnIfFailed(
      dir_enum,
      base::BindRepeating(
          [](int64_t sequence_id_to_keep, const base::FilePath& full_name) {
            const auto sequence_id = GetFileSequenceIdFromPath(full_name);
            if (!sequence_id.has_value()) {
              return false;
            }
            if (sequence_id.value() >= sequence_id_to_keep) {
              return false;
            }
            return true;
          },
          sequencing_id_to_keep));
}

// Context for uploading data from the queue in proper sequence.
// Runs on a storage_queue->sequenced_task_runner_
// Makes necessary calls to the provided |UploaderInterface|:
// repeatedly to ProcessRecord/ProcessGap, and Completed at the end.
// Sets references to potentially used files aside, and increments
// active_read_operations_ to make sure confirmation will not trigger
// files deletion. Decrements it upon completion (when this counter
// is zero, RemoveConfirmedData can delete the unused files).
// Returns result through `completion_cb`.
class StorageQueue::ReadContext : public TaskRunnerContext<Status> {
 public:
  ReadContext(UploaderInterface::UploadReason reason,
              base::OnceCallback<void(Status)> completion_cb,
              scoped_refptr<StorageQueue> storage_queue)
      : TaskRunnerContext<Status>(
            base::BindOnce(&ReadContext::UploadingCompleted,
                           base::Unretained(this)),
            storage_queue->sequenced_task_runner_),
        reason_(reason),
        completion_cb_(std::move(completion_cb)),
        async_start_upload_cb_(storage_queue->async_start_upload_cb_),
        must_invoke_upload_(
            EncryptionModuleInterface::is_enabled() &&
            storage_queue->encryption_module_->need_encryption_key()),
        storage_queue_(storage_queue->weakptr_factory_.GetWeakPtr()) {
    CHECK(storage_queue.get());
    CHECK(async_start_upload_cb_);
    CHECK_LT(
        static_cast<uint32_t>(reason),
        static_cast<uint32_t>(UploaderInterface::UploadReason::MAX_REASON));
  }

 private:
  // Context can only be deleted by calling Response method.
  ~ReadContext() override = default;

  void OnStart() override {
    if (!storage_queue_) {
      Response(Status(error::UNAVAILABLE, "StorageQueue shut down"));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::STORAGE_QUEUE_SHUTDOWN,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);
    if (!must_invoke_upload_) {
      PrepareDataFiles();
      return;
    }

    InstantiateUploader(
        base::BindOnce(&ReadContext::PrepareDataFiles, base::Unretained(this)));
  }

  void PrepareDataFiles() {
    if (!storage_queue_) {
      Response(Status(error::UNAVAILABLE, "StorageQueue shut down"));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::STORAGE_QUEUE_SHUTDOWN,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);

    // Fill in initial sequencing information to track progress:
    // use minimum of first_sequencing_id_ and first_unconfirmed_sequencing_id_
    // if the latter has been recorded.
    sequence_info_.set_generation_id(storage_queue_->generation_id_);
    if (storage_queue_->first_unconfirmed_sequencing_id_.has_value()) {
      sequence_info_.set_sequencing_id(
          std::min(storage_queue_->first_unconfirmed_sequencing_id_.value(),
                   storage_queue_->first_sequencing_id_));
    } else {
      sequence_info_.set_sequencing_id(storage_queue_->first_sequencing_id_);
    }

    // If there are no files in the queue, do nothing and return success right
    // away. This can happen in case of key delivery request.
    if (storage_queue_->files_.empty()) {
      Response(Status::StatusOK());
      return;
    }

    // If the last file is not empty (has at least one record),
    // close it and create the new one, so that its records are
    // also included in the reading.
    const Status last_status = storage_queue_->SwitchLastFileIfNotEmpty();
    if (!last_status.ok()) {
      Response(last_status);
      return;
    }

    // If expected sequencing id is at or beyond the last (empty) file,
    // we have succeeded - there are no records to upload.
    if (sequence_info_.sequencing_id() >=
        storage_queue_->files_.rbegin()->first) {
      Response(Status::StatusOK());
      return;
    }

    // Collect and set aside the files in the set that might have data
    // for the Upload.
    files_ =
        storage_queue_->CollectFilesForUpload(sequence_info_.sequencing_id());
    if (files_.empty()) {
      Response(Status(error::OUT_OF_RANGE,
                      "Sequencing id not found in StorageQueue."));
      return;
    }

    // Register with storage_queue, to make sure selected files are not removed.
    ++(storage_queue_->active_read_operations_);

    if (uploader_) {
      // Uploader already created.
      BeginUploading();
      return;
    }

    InstantiateUploader(
        base::BindOnce(&ReadContext::BeginUploading, base::Unretained(this)));
  }

  void BeginUploading() {
    if (!storage_queue_) {
      Response(Status(error::UNAVAILABLE, "StorageQueue shut down"));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::STORAGE_QUEUE_SHUTDOWN,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);

    // The first <seq.file> pair is the current file now, and we are at its
    // start or ahead of it.
    current_file_ = files_.begin();
    current_pos_ = 0;

    // If the first record we need to upload is unavailable, produce Gap record
    // instead.
    if (sequence_info_.sequencing_id() < current_file_->first) {
      CallGapUpload(/*count=*/current_file_->first -
                    sequence_info_.sequencing_id());
      // Resume at ScheduleNextRecord.
      return;
    }

    StartUploading();
  }

  void StartUploading() {
    if (!storage_queue_) {
      Response(Status(error::UNAVAILABLE, "StorageQueue shut down"));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::STORAGE_QUEUE_SHUTDOWN,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);
    // Read from it until the specified sequencing id is found.
    for (int64_t sequencing_id = current_file_->first;
         sequencing_id < sequence_info_.sequencing_id(); ++sequencing_id) {
      auto blob = EnsureBlob(sequencing_id);
      if (!blob.has_value() &&
          blob.error().error_code() == error::OUT_OF_RANGE) {
        // Reached end of file, switch to the next one (if present).
        ++current_file_;
        if (current_file_ == files_.end()) {
          Response(Status::StatusOK());
          return;
        }
        current_pos_ = 0;
        blob = EnsureBlob(sequence_info_.sequencing_id());
      }
      if (!blob.has_value()) {
        // File found to be corrupt. Produce Gap record till the start of next
        // file, if present.
        ++current_file_;
        current_pos_ = 0;
        uint64_t count = static_cast<uint64_t>(
            (current_file_ == files_.end())
                ? 1
                : current_file_->first - sequence_info_.sequencing_id());
        CallGapUpload(count);
        // Resume at ScheduleNextRecord.
        return;
      }
    }

    // Read and upload sequence_info_.sequencing_id().
    CallRecordOrGap(sequence_info_.sequencing_id());
    // Resume at ScheduleNextRecord.
  }

  void UploadingCompleted(Status status) {
    // Release all files.
    files_.clear();
    current_file_ = files_.end();
    // If uploader was created, notify it about completion.
    if (uploader_) {
      uploader_->Completed(status);
    }
    // If retry delay is specified, check back after the delay.
    // If the status was error, or if any events are still there,
    // retry the upload.
    if (storage_queue_ &&
        !storage_queue_->options_.upload_retry_delay().is_zero()) {
      storage_queue_->check_back_timer_.Start(
          FROM_HERE, storage_queue_->options_.upload_retry_delay(),
          base::BindPostTask(
              storage_queue_->sequenced_task_runner_,
              base::BindRepeating(
                  &StorageQueue::CheckBackUpload,
                  storage_queue_->weakptr_factory_.GetWeakPtr(), status,
                  /*next_sequencing_id=*/sequence_info_.sequencing_id())));
    }
  }

  void OnCompletion(const Status& status) override {
    if (!storage_queue_) {
      std::move(completion_cb_)
          .Run(Status(error::UNAVAILABLE, "StorageQueue shut down"));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::STORAGE_QUEUE_SHUTDOWN,
          UnavailableErrorReason::MAX_VALUE);
      files_.clear();
      current_file_ = files_.end();
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);
    // Unregister with storage_queue.
    if (!files_.empty()) {
      const auto count = --(storage_queue_->active_read_operations_);
      CHECK_GE(count, 0);
      files_.clear();
      current_file_ = files_.end();
    }
    // Respond with the result.
    std::move(completion_cb_).Run(status);
  }

  // Prepares the |blob| for uploading.
  void CallCurrentRecord(std::string_view blob) {
    if (!storage_queue_) {
      Response(Status(error::UNAVAILABLE, "StorageQueue shut down"));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::STORAGE_QUEUE_SHUTDOWN,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);
    google::protobuf::io::ArrayInputStream blob_stream(  // Zero-copy stream.
        blob.data(), blob.size());
    EncryptedRecord encrypted_record;
    ScopedReservation scoped_reservation(
        blob.size(), storage_queue_->options().memory_resource());
    if (!scoped_reservation.reserved()) {
      Response(
          Status(error::RESOURCE_EXHAUSTED, "Insufficient memory for upload"));
      return;
    }
    if (!encrypted_record.ParseFromZeroCopyStream(&blob_stream)) {
      LOG(ERROR) << "Failed to parse record, seq="
                 << sequence_info_.sequencing_id();
      CallGapUpload(/*count=*/1);  // Do not reserve space for Gap record.
      // Resume at ScheduleNextRecord.
      return;
    }
    CallRecordUpload(std::move(encrypted_record),
                     std::move(scoped_reservation));
  }

  // Completes sequence information and makes a call to UploaderInterface
  // instance provided by user, which can place processing of the record on any
  // thread(s). Once it returns, it will schedule NextRecord to execute on the
  // sequential thread runner of this StorageQueue. If |encrypted_record| is
  // empty (has no |encrypted_wrapped_record| and/or |encryption_info|), it
  // indicates a gap notification.
  void CallRecordUpload(EncryptedRecord encrypted_record,
                        ScopedReservation scoped_reservation) {
    if (!storage_queue_) {
      Response(Status(error::UNAVAILABLE, "StorageQueue shut down"));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::STORAGE_QUEUE_SHUTDOWN,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);
    if (encrypted_record.has_sequence_information()) {
      LOG(ERROR) << "Sequence information already present, seq="
                 << sequence_info_.sequencing_id();
      CallGapUpload(/*count=*/1);
      // Resume at ScheduleNextRecord.
      return;
    }
    // Fill in sequence information.
    // Priority is attached by the Storage layer.
    *encrypted_record.mutable_sequence_information() = sequence_info_;
    uploader_->ProcessRecord(std::move(encrypted_record),
                             std::move(scoped_reservation),
                             base::BindOnce(&ReadContext::ScheduleNextRecord,
                                            base::Unretained(this)));
    // Move sequencing id forward (ScheduleNextRecord will see this).
    sequence_info_.set_sequencing_id(sequence_info_.sequencing_id() + 1);
  }

  void CallGapUpload(uint64_t count) {
    if (!storage_queue_) {
      Response(Status(error::UNAVAILABLE, "StorageQueue shut down"));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::STORAGE_QUEUE_SHUTDOWN,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);
    if (count == 0u) {
      // No records skipped.
      NextRecord(/*more_records=*/true);
      return;
    }
    uploader_->ProcessGap(sequence_info_, count,
                          base::BindOnce(&ReadContext::ScheduleNextRecord,
                                         base::Unretained(this)));
    // Move sequence id forward (ScheduleNextRecord will see this).
    sequence_info_.set_sequencing_id(sequence_info_.sequencing_id() + count);
  }

  // Schedules NextRecord to execute on the StorageQueue sequential task runner.
  void ScheduleNextRecord(bool more_records) {
    Schedule(&ReadContext::NextRecord, base::Unretained(this), more_records);
  }

  // If more records are expected, retrieves the next record (if present) and
  // sends for processing, or calls Response with error status. Otherwise, call
  // Response(OK).
  void NextRecord(bool more_records) {
    if (!storage_queue_) {
      Response(Status(error::UNAVAILABLE, "StorageQueue shut down"));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::STORAGE_QUEUE_SHUTDOWN,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);
    if (!more_records) {
      Response(Status::StatusOK());  // Requested to stop reading.
      return;
    }
    // If reached end of the last file, finish reading.
    if (current_file_ == files_.end()) {
      Response(Status::StatusOK());
      return;
    }
    // sequence_info_.sequencing_id() blob is ready.
    CallRecordOrGap(sequence_info_.sequencing_id());
    // Resume at ScheduleNextRecord.
  }

  // Loads blob from the current file - reads header first, and then the body.
  // (SingleFile::Read call makes sure all the data is in the buffer).
  // After reading, verifies that data matches the hash stored in the header.
  // If everything checks out, returns the reference to the data in the buffer:
  // the buffer remains intact until the next call to SingleFile::Read.
  // If anything goes wrong (file is shorter than expected, or record hash does
  // not match), returns error.
  StatusOr<std::string_view> EnsureBlob(int64_t sequencing_id) {
    if (!storage_queue_) {
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::STORAGE_QUEUE_SHUTDOWN,
          UnavailableErrorReason::MAX_VALUE);
      return base::unexpected(
          Status(error::UNAVAILABLE, "StorageQueue shut down"));
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);

    // Test only: simulate error, if requested.
    if (storage_queue_->test_injection_handler_) {
      RETURN_IF_ERROR_STATUS(
          base::unexpected(storage_queue_->test_injection_handler_.Run(
              test::StorageQueueOperationKind::kReadBlock, sequencing_id)));
    }

    // Read from the current file at the current offset.
    RETURN_IF_ERROR_STATUS(
        base::unexpected(current_file_->second->Open(/*read_only=*/true)));
    const size_t max_buffer_size =
        RoundUpToFrameSize(storage_queue_->options_.max_record_size()) +
        RoundUpToFrameSize(RecordHeader::kSize);
    auto read_result = current_file_->second->Read(
        current_pos_, RecordHeader::kSize, max_buffer_size);
    ASSIGN_OR_RETURN(auto header_data, read_result);
    if (header_data.empty()) {
      // No more blobs.
      return base::unexpected(
          Status(error::OUT_OF_RANGE, "Reached end of data"));
    }
    current_pos_ += header_data.size();
    // Copy the header out (its memory can be overwritten when reading rest of
    // the data).
    const auto header_status = RecordHeader::FromString(header_data);
    if (!header_status.has_value()) {
      // Error detected.
      return base::unexpected(Status(
          error::INTERNAL,
          base::StrCat({"File corrupt: ", current_file_->second->name()})));
    }
    const auto header = std::move(header_status.value());
    if (header.record_sequencing_id != sequencing_id) {
      return base::unexpected(Status(
          error::INTERNAL,
          base::StrCat(
              {"File corrupt: ", current_file_->second->name(),
               " seq=", base::NumberToString(header.record_sequencing_id),
               " expected=", base::NumberToString(sequencing_id)})));
    }
    // Read the record blob (align size to FRAME_SIZE).
    const size_t data_size = RoundUpToFrameSize(header.record_size);
    // From this point on, header in memory is no longer used and can be
    // overwritten when reading rest of the data.
    read_result =
        current_file_->second->Read(current_pos_, data_size, max_buffer_size);
    RETURN_IF_ERROR(read_result);
    current_pos_ += read_result.value().size();
    if (read_result.value().size() != data_size) {
      // File corrupt, blob incomplete.
      return base::unexpected(Status(
          error::INTERNAL,
          base::StrCat(
              {"File corrupt: ", current_file_->second->name(),
               " size=", base::NumberToString(read_result.value().size()),
               " expected=", base::NumberToString(data_size)})));
    }
    // Verify record hash.
    uint32_t actual_record_hash =
        base::PersistentHash(read_result.value().substr(0, header.record_size));
    if (header.record_hash != actual_record_hash) {
      return base::unexpected(Status(
          error::INTERNAL,
          base::StrCat(
              {"File corrupt: ", current_file_->second->name(), " seq=",
               base::NumberToString(header.record_sequencing_id), " hash=",
               base::HexEncode(
                   reinterpret_cast<const uint8_t*>(&header.record_hash),
                   sizeof(header.record_hash)),
               " expected=",
               base::HexEncode(
                   reinterpret_cast<const uint8_t*>(&actual_record_hash),
                   sizeof(actual_record_hash))})));
    }
    return read_result.value().substr(0, header.record_size);
  }

  void CallRecordOrGap(int64_t sequencing_id) {
    if (!storage_queue_) {
      Response(Status(error::UNAVAILABLE, "StorageQueue shut down"));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::STORAGE_QUEUE_SHUTDOWN,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);
    auto blob = EnsureBlob(sequence_info_.sequencing_id());
    if (!blob.has_value() && blob.error().error_code() == error::OUT_OF_RANGE) {
      // Reached end of file, switch to the next one (if present).
      ++current_file_;
      if (current_file_ == files_.end()) {
        Response(Status::StatusOK());
        return;
      }
      current_pos_ = 0;
      blob = EnsureBlob(sequence_info_.sequencing_id());
    }
    if (!blob.has_value()) {
      // File found to be corrupt. Produce Gap record till the start of next
      // file, if present.
      ++current_file_;
      current_pos_ = 0;
      uint64_t count = static_cast<uint64_t>(
          (current_file_ == files_.end())
              ? 1
              : current_file_->first - sequence_info_.sequencing_id());
      CallGapUpload(count);
      // Resume at ScheduleNextRecord.
      return;
    }
    CallCurrentRecord(blob.value());
    // Resume at ScheduleNextRecord.
  }

  void InstantiateUploader(base::OnceCallback<void()> continuation) {
    if (!storage_queue_) {
      Response(Status(error::UNAVAILABLE, "StorageQueue shut down"));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::STORAGE_QUEUE_SHUTDOWN,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            [](base::OnceCallback<void()> continuation, ReadContext* self) {
              self->async_start_upload_cb_.Run(
                  self->reason_,
                  base::BindOnce(&ReadContext::ScheduleOnUploaderInstantiated,
                                 base::Unretained(self),
                                 std::move(continuation)));
            },
            std::move(continuation), base::Unretained(this)));
  }

  void ScheduleOnUploaderInstantiated(
      base::OnceCallback<void()> continuation,
      StatusOr<std::unique_ptr<UploaderInterface>> uploader_result) {
    Schedule(base::BindOnce(&ReadContext::OnUploaderInstantiated,
                            base::Unretained(this), std::move(continuation),
                            std::move(uploader_result)));
  }

  void OnUploaderInstantiated(
      base::OnceCallback<void()> continuation,
      StatusOr<std::unique_ptr<UploaderInterface>> uploader_result) {
    if (!storage_queue_) {
      Response(Status(error::UNAVAILABLE, "StorageQueue shut down"));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::STORAGE_QUEUE_SHUTDOWN,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);
    if (!uploader_result.has_value()) {
      Response(Status(error::FAILED_PRECONDITION,
                      base::StrCat({"Failed to provide the Uploader, status=",
                                    uploader_result.error().ToString()})));
      return;
    }
    CHECK(!uploader_)
        << "Uploader instantiated more than once for single upload";
    uploader_ = std::move(uploader_result.value());

    std::move(continuation).Run();
  }

  // Upload reason. Passed to uploader instantiation and may affect
  // the uploader object.
  const UploaderInterface::UploadReason reason_;

  // Completion callback.
  base::OnceCallback<void(Status)> completion_cb_;

  // Files that will be read (in order of sequencing ids).
  std::map<int64_t, scoped_refptr<SingleFile>> files_;
  SequenceInformation sequence_info_;
  uint32_t current_pos_;
  std::map<int64_t, scoped_refptr<SingleFile>>::iterator current_file_;
  const UploaderInterface::AsyncStartUploaderCb async_start_upload_cb_;
  const bool must_invoke_upload_;
  std::unique_ptr<UploaderInterface> uploader_;
  base::WeakPtr<StorageQueue> storage_queue_;
};

class StorageQueue::WriteContext : public TaskRunnerContext<Status> {
 public:
  WriteContext(Record record,
               base::OnceCallback<void(Status)> write_callback,
               scoped_refptr<StorageQueue> storage_queue)
      : TaskRunnerContext<Status>(std::move(write_callback),
                                  storage_queue->sequenced_task_runner_),
        storage_queue_(storage_queue),
        record_(std::move(record)) {
    CHECK(storage_queue_.get());
  }

 private:
  // Context can only be deleted by calling Response method.
  ~WriteContext() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);

    // If still in queue, remove it (something went wrong).
    if (in_contexts_queue_ != storage_queue_->write_contexts_queue_.end()) {
      CHECK_EQ(storage_queue_->write_contexts_queue_.front().get(), this);
      storage_queue_->write_contexts_queue_.erase(in_contexts_queue_);
    }

    // If there is the context at the front of the queue and its buffer is
    // filled in, schedule respective |Write| to happen now.
    if (!storage_queue_->write_contexts_queue_.empty() &&
        !storage_queue_->write_contexts_queue_.front()->buffer_.empty()) {
      storage_queue_->write_contexts_queue_.front()->Schedule(
          &WriteContext::ResumeWriteRecord,
          storage_queue_->write_contexts_queue_.front());
    }

    // If uploads are not immediate, we are done.
    if (!storage_queue_->options_.upload_period().is_zero()) {
      return;
    }

    // Otherwise initiate Upload right after writing
    // finished and respond back when reading Upload is done.
    // Note: new uploader created synchronously before scheduling Upload.
    Start<ReadContext>(UploaderInterface::UploadReason::IMMEDIATE_FLUSH,
                       base::DoNothing(), storage_queue_);
  }

  void OnStart() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);

    // Set iterator to `end` in case early exit is required.
    in_contexts_queue_ = storage_queue_->write_contexts_queue_.end();

    // Make sure the record is valid.
    if (!record_.has_destination() ||
        record_.destination() == Destination::UNDEFINED_DESTINATION) {
      Response(Status(error::FAILED_PRECONDITION,
                      "Malformed record: missing destination"));
      return;
    }

    // Prepare a copy of the original record, if `upload_settings` is present.
    if (record_.needs_local_unencrypted_copy()) {
      record_copy_ = record_;
      record_.clear_needs_local_unencrypted_copy();
    }

    // If `record_` requires to uphold reserved space, check whether disk space
    // is sufficient. Note that this is only an approximate check, since other
    // writes that have no reservation specified will not observe it anyway.
    // As such, it relies on the Record's ByteSizeLong(), not accounting for
    // compression and overhead.
    if (record_.reserved_space() > 0u) {
      const uint64_t space_used =
          storage_queue_->options().disk_space_resource()->GetUsed();
      const uint64_t space_total =
          storage_queue_->options().disk_space_resource()->GetTotal();
      if (space_used + record_.ByteSizeLong() + record_.reserved_space() >
          space_total) {
        // Do not apply degradation, if insufficient - just reject with error.
        Response(Status(
            error::RESOURCE_EXHAUSTED,
            base::StrCat({"Write would not leave enough reserved space=",
                          base::NumberToString(record_.reserved_space()),
                          ", available=",
                          base::NumberToString(space_total - space_used)})));
        return;
      }

      // Remove `reserved_space` field from the `record_` itself - no longer
      // needed.
      record_.clear_reserved_space();
    }

    // Wrap the record.
    WrappedRecord wrapped_record;
    *wrapped_record.mutable_record() = std::move(record_);

    // Calculate new record digest and store it in the record
    // (for self-verification by the server). Do not store it in the queue yet,
    // because the record might fail to write.
    {
      std::string serialized_record;
      wrapped_record.record().SerializeToString(&serialized_record);
      current_record_digest_ = crypto::SHA256HashString(serialized_record);
      CHECK_EQ(current_record_digest_.size(), crypto::kSHA256Length);
      *wrapped_record.mutable_record_digest() = current_record_digest_;
    }

    // Attach last record digest.
    if (storage_queue_->write_contexts_queue_.empty()) {
      // Queue is empty, copy |storage_queue_|->|last_record_digest_|
      // into the record, if it exists.
      const auto last_record_digest = storage_queue_->GetLastRecordDigest();
      if (last_record_digest.has_value()) {
        *wrapped_record.mutable_last_record_digest() =
            last_record_digest.value();
      }
    } else {
      // Copy previous record digest in the queue into the record.
      *wrapped_record.mutable_last_record_digest() =
          (*storage_queue_->write_contexts_queue_.rbegin())
              ->current_record_digest_;
    }

    // Add context to the end of the queue.
    in_contexts_queue_ = storage_queue_->write_contexts_queue_.insert(
        storage_queue_->write_contexts_queue_.end(),
        weak_ptr_factory_.GetWeakPtr());

    // Start processing wrapped record.
    PrepareProcessWrappedRecord(std::move(wrapped_record));
  }

  void PrepareProcessWrappedRecord(WrappedRecord wrapped_record) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);

    // Reserve space. Pause processing, if necessary.
    const size_t serialized_size = wrapped_record.ByteSizeLong();
    ScopedReservation scoped_reservation(
        serialized_size, storage_queue_->options().memory_resource());
    // Inject "memory unavailable" failure, if requested.
    if (storage_queue_->test_injection_handler_ &&
        !storage_queue_->test_injection_handler_
             .Run(test::StorageQueueOperationKind::kWrappedRecordLowMemory,
                  storage_queue_->next_sequencing_id_)
             .ok()) {
      scoped_reservation.Reduce(0);
    }
    if (!scoped_reservation.reserved()) {
      if (remaining_attempts_ > 0u) {
        // Attempt to wait for sufficient memory availability
        // and retry.
        --remaining_attempts_;
        storage_queue_->options().memory_resource()->RegisterCallback(
            serialized_size,
            base::BindOnce(&WriteContext::PrepareProcessWrappedRecord,
                           base::Unretained(this), std::move(wrapped_record)));
        return;
      }
      // Max number of attempts exceeded, return error.
      Schedule(&WriteContext::Response, base::Unretained(this),
               Status(error::RESOURCE_EXHAUSTED,
                      "Not enough memory for the write buffer"));
      return;
    }

    // Memory reserved, serialize and compress wrapped record on a thread pool.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&WriteContext::ProcessWrappedRecord,
                       base::Unretained(this), std::move(wrapped_record),
                       std::move(scoped_reservation)));
  }

  void ProcessWrappedRecord(WrappedRecord wrapped_record,
                            ScopedReservation scoped_reservation) {
    // UTC time of 2122-01-01T00:00:00Z since Unix epoch 1970-01-01T00:00:00Z in
    // microseconds
    static constexpr int64_t kTime2122 = 4'796'668'800'000'000;
    // Log an error if the timestamp is larger than 2122-01-01T00:00:00Z. This
    // is the latest spot in the code before a record is compressed or
    // encrypted.
    // TODO(b/254270304): Remove this log after M111 is released and no error is
    // reported for 3 months.
    LOG_IF(ERROR, wrapped_record.record().timestamp_us() > kTime2122)
        << "Unusually large timestamp (in milliseconds): "
        << wrapped_record.record().timestamp_us();

    // Serialize wrapped record into a string.
    std::string buffer;
    if (!wrapped_record.SerializeToString(&buffer)) {
      base::UmaHistogramEnumeration(
          reporting::kUmaDataLossErrorReason,
          DataLossErrorReason::FAILED_TO_SERIALIZE_WRAPPED_RECORD,
          DataLossErrorReason::MAX_VALUE);
      Schedule(&WriteContext::Response, base::Unretained(this),
               Status(error::DATA_LOSS, "Cannot serialize record"));
      return;
    }
    // Release wrapped record memory, so `scoped_reservation` may act.
    wrapped_record.Clear();
    CompressWrappedRecord(std::move(buffer), std::move(scoped_reservation));
  }

  void CompressWrappedRecord(std::string serialized_record,
                             ScopedReservation scoped_reservation) {
    // Compress the string. If memory is insufficient, compression is skipped.
    storage_queue_->compression_module_->CompressRecord(
        std::move(serialized_record),
        storage_queue_->options().memory_resource(),
        base::BindOnce(&WriteContext::OnCompressedRecordReady,
                       base::Unretained(this), std::move(scoped_reservation)));
  }

  void OnCompressedRecordReady(
      ScopedReservation scoped_reservation,
      std::string compressed_record_result,
      std::optional<CompressionInformation> compression_information) {
    // Reduce amount of memory reserved to the resulting size after compression.
    scoped_reservation.Reduce(compressed_record_result.size());

    // Encrypt the result. The callback is partially bounded to include
    // compression information.
    storage_queue_->encryption_module_->EncryptRecord(
        compressed_record_result,
        base::BindPostTask(storage_queue_->sequenced_task_runner_,
                           base::BindOnce(&WriteContext::OnEncryptedRecordReady,
                                          base::Unretained(this),
                                          std::move(compression_information))));
  }

  void OnEncryptedRecordReady(
      std::optional<CompressionInformation> compression_information,
      StatusOr<EncryptedRecord> encrypted_record_result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);
    if (!encrypted_record_result.has_value()) {
      // Failed to serialize or encrypt.
      Response(encrypted_record_result.error());
      return;
    }
    auto encrypted_record = std::move(encrypted_record_result.value());

    // Add compression information to the encrypted record if it exists.
    if (compression_information.has_value()) {
      *encrypted_record.mutable_compression_information() =
          compression_information.value();
    }

    // Add original Record copy, if required.
    if (record_copy_.has_value()) {
      *encrypted_record.mutable_record_copy() = std::move(record_copy_.value());
    }

    // Proceed and serialize record.
    SerializeEncryptedRecord(std::move(compression_information),
                             std::move(encrypted_record));
  }

  void SerializeEncryptedRecord(
      std::optional<CompressionInformation> compression_information,
      EncryptedRecord encrypted_record) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);
    // Serialize encrypted record.
    const size_t serialized_size = encrypted_record.ByteSizeLong();
    ScopedReservation scoped_reservation(
        serialized_size, storage_queue_->options().memory_resource());
    // Inject "memory unavailable" failure, if requested.
    if (storage_queue_->test_injection_handler_ &&
        !storage_queue_->test_injection_handler_
             .Run(test::StorageQueueOperationKind::kEncryptedRecordLowMemory,
                  storage_queue_->next_sequencing_id_)
             .ok()) {
      scoped_reservation.Reduce(0);
    }
    if (!scoped_reservation.reserved()) {
      if (remaining_attempts_ > 0u) {
        // Attempt to wait for sufficient memory availability
        // and retry.
        --remaining_attempts_;
        storage_queue_->options().memory_resource()->RegisterCallback(
            serialized_size,
            base::BindOnce(&WriteContext::SerializeEncryptedRecord,
                           base::Unretained(this),
                           std::move(compression_information),
                           std::move(encrypted_record)));
        return;
      }
      Schedule(&WriteContext::Response, base::Unretained(this),
               Status(error::RESOURCE_EXHAUSTED,
                      "Not enough memory for encrypted record"));
      return;
    }
    std::string buffer;
    if (!encrypted_record.SerializeToString(&buffer)) {
      base::UmaHistogramEnumeration(
          reporting::kUmaDataLossErrorReason,
          DataLossErrorReason::FAILED_TO_SERIALIZE_ENCRYPTED_RECORD,
          DataLossErrorReason::MAX_VALUE);
      Schedule(&WriteContext::Response, base::Unretained(this),
               Status(error::DATA_LOSS, "Cannot serialize EncryptedRecord"));
      return;
    }
    // Release encrypted record memory, so scoped reservation may act.
    encrypted_record.Clear();

    // Write into storage on sequential task runner.
    Schedule(&WriteContext::WriteRecord, base::Unretained(this),
             std::move(buffer));
  }

  void WriteRecord(std::string buffer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);
    buffer_.swap(buffer);

    ResumeWriteRecord();
  }

  void ResumeWriteRecord() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);
    // The size of the reservation is unknown until calculated.
    CHECK_EQ(storage_queue_->active_write_reservation_size_, 0u);

    // If we are not at the head of the queue, delay write and expect to be
    // reactivated later.
    CHECK(in_contexts_queue_ != storage_queue_->write_contexts_queue_.end());
    if (storage_queue_->write_contexts_queue_.front().get() != this) {
      return;
    }

    CHECK(!buffer_.empty());
    // active_write_reservation_size_ includes both expected size of META file
    // and increase in size of DATA file.
    storage_queue_->active_write_reservation_size_ =
        sizeof(generation_id_) + current_record_digest_.size() +
        RoundUpToFrameSize(sizeof(RecordHeader) + buffer_.size());

    Status reserve_result = storage_queue_->ReserveNewRecordDiskSpace(
        storage_queue_->active_write_reservation_size_);
    if (!reserve_result.ok()) {
      if (!base::FeatureList::IsEnabled(kReportingStorageDegradationFeature)) {
        storage_queue_->active_write_reservation_size_ = 0;
        Response(reserve_result);
        return;
      }
      StartRecordsShedding(storage_queue_->active_write_reservation_size_);
      storage_queue_->active_write_reservation_size_ = 0;
      return;
    }

    // We are at the head of the queue, remove ourselves.
    storage_queue_->write_contexts_queue_.pop_front();
    in_contexts_queue_ = storage_queue_->write_contexts_queue_.end();

    StatusOr<scoped_refptr<SingleFile>> assign_result =
        storage_queue_->AssignLastFile(buffer_.size());
    if (!assign_result.has_value()) {
      Response(assign_result.error());
      return;
    }
    scoped_refptr<SingleFile> last_file = assign_result.value();

    // Writing metadata ahead of the data write.
    Status write_result = storage_queue_->WriteMetadata(current_record_digest_);
    if (!write_result.ok()) {
      Response(write_result);
      return;
    }

    // Write header and block. Store current_record_digest_ with the queue,
    // increment next_sequencing_id_
    write_result = storage_queue_->WriteHeaderAndBlock(
        buffer_, current_record_digest_, std::move(last_file));
    if (!write_result.ok()) {
      Response(write_result);
      return;
    }

    Response(Status::StatusOK());
  }

  void StartRecordsShedding(const size_t space_to_recover) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);

    // Prepare callbacks for shedding success and failure.
    // Both will run on the current queue.
    auto resume_writing_cb =
        base::BindPostTask(storage_queue_->sequenced_task_runner_,
                           base::BindOnce(&WriteContext::ResumeWriteRecord,
                                          base::Unretained(this)));
    auto writing_failure_cb = base::BindPostTask(
        storage_queue_->sequenced_task_runner_,
        base::BindOnce(&WriteContext::DiskSpaceReservationFailure,
                       base::Unretained(this), space_to_recover));

    if (storage_queue_->degradation_queues_.empty()) {
      // No lower priority queues available for degradation.
      // Try to shed files in the current queue (if allowed).
      storage_queue_->RecordsSheddingHelper(space_to_recover,
                                            std::move(resume_writing_cb),
                                            std::move(writing_failure_cb));
      return;
    }

    // Try shedding in the lowest priority queue, passing the rest of the vector
    // for the next attempts (schedule shedding on the lowest priority queue's
    // task runner).
    base::span<scoped_refptr<StorageQueue>> degradation_queues_span{
        storage_queue_->degradation_queues_};
    storage_queue_->degradation_queues_.front()
        ->sequenced_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&StorageQueue::RecordsShedding,
                           storage_queue_->degradation_queues_.front(),
                           degradation_queues_span.last(
                               degradation_queues_span.size() - 1),
                           storage_queue_, space_to_recover,
                           std::move(resume_writing_cb),
                           std::move(writing_failure_cb)));
  }

  void DiskSpaceReservationFailure(uint64_t space_to_recover) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);

    // We are at the head of the queue, remove ourselves.
    storage_queue_->write_contexts_queue_.pop_front();
    in_contexts_queue_ = storage_queue_->write_contexts_queue_.end();

    const uint64_t space_used =
        storage_queue_->options().disk_space_resource()->GetUsed();
    const uint64_t space_total =
        storage_queue_->options().disk_space_resource()->GetTotal();
    Response(
        Status(error::RESOURCE_EXHAUSTED,
               base::StrCat({"Not enough disk space available to write "
                             "new record.\nSize of new record: ",
                             base::NumberToString(space_to_recover),
                             "\nDisk space available: ",
                             base::NumberToString(space_total - space_used)})));
  }

  void OnCompletion(const Status& status) override {
    if (storage_queue_->active_write_reservation_size_ > 0u) {
      CHECK(!status.ok());
      storage_queue_->options_.disk_space_resource()->Discard(
          storage_queue_->active_write_reservation_size_);
    }
  }

  scoped_refptr<StorageQueue> storage_queue_;

  Record record_;

  // Digest of the current record.
  std::string current_record_digest_;

  // Write buffer. When filled in (after encryption), |WriteRecord| can be
  // executed. Empty until encryption is done.
  std::string buffer_;

  // Atomic counter of insufficien memory retry attempts.
  // Accessed in serialized methods only.
  size_t remaining_attempts_ = 16u;

  // Copy of the original record, if required.
  std::optional<Record> record_copy_;

  // Position in the `storage_queue_`->`write_contexts_queue_`.
  // We use it in order to detect whether the context is in the queue
  // and to remove it from the queue, when the time comes.
  std::list<base::WeakPtr<WriteContext>>::iterator in_contexts_queue_
      GUARDED_BY_CONTEXT(storage_queue_->storage_queue_sequence_checker_);

  // Factory for the `context_queue_`.
  base::WeakPtrFactory<WriteContext> weak_ptr_factory_
      GUARDED_BY_CONTEXT(storage_queue_->storage_queue_sequence_checker_){this};
};

void StorageQueue::Write(Record record,
                         base::OnceCallback<void(Status)> completion_cb) {
  Start<WriteContext>(std::move(record), std::move(completion_cb), this);
}

Status StorageQueue::ReserveNewRecordDiskSpace(const size_t total_size) {
  if ((test_injection_handler_ &&  // Test only: Simulate failure if requested
       !test_injection_handler_
            .Run(test::StorageQueueOperationKind::kWriteLowDiskSpace,
                 next_sequencing_id_)
            .ok()) ||
      !options_.disk_space_resource()->Reserve(total_size)) {
    const uint64_t space_used = options_.disk_space_resource()->GetUsed();
    const uint64_t space_total = options_.disk_space_resource()->GetTotal();
    return Status(
        error::RESOURCE_EXHAUSTED,
        base::StrCat({"Not enough disk space available to write "
                      "new record.\nSize of new record: ",
                      base::NumberToString(total_size),
                      "\nDisk space available: ",
                      base::NumberToString(space_total - space_used)}));
  }
  return Status::StatusOK();
}

void StorageQueue::RecordsShedding(
    base::span<scoped_refptr<StorageQueue>> degradation_queues,
    scoped_refptr<StorageQueue> writing_storage_queue,
    const size_t space_to_recover,
    base::OnceClosure resume_writing_cb,
    base::OnceClosure writing_failure_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);

  // Try to shed files in the current queue.
  if (FilesShedding(space_to_recover)) {
    std::move(resume_writing_cb).Run();
    return;
  }

  if (!degradation_queues.empty()) {
    // There are more queues, try shedding in the lowest priority
    // (schedule it on the respective task runner).
    degradation_queues.front()->sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &StorageQueue::RecordsShedding, degradation_queues.front(),
            degradation_queues.last(degradation_queues.size() - 1),
            writing_storage_queue, space_to_recover,
            std::move(resume_writing_cb), std::move(writing_failure_cb)));
    return;
  }

  // No more queues, try shedding in `write_storage_queue`.
  writing_storage_queue->sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StorageQueue::RecordsSheddingHelper,
                                writing_storage_queue, space_to_recover,
                                std::move(resume_writing_cb),
                                std::move(writing_failure_cb)));
}

void StorageQueue::RecordsSheddingHelper(const size_t space_to_recover,
                                         base::OnceClosure resume_writing_cb,
                                         base::OnceClosure writing_failure_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);

  if (FilesShedding(space_to_recover)) {
    std::move(resume_writing_cb).Run();
    return;
  }

  std::move(writing_failure_cb).Run();
}

bool StorageQueue::FilesShedding(const size_t space_to_recover) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  if (files_.size() > 1 && !active_read_operations_ &&
      options_.can_shed_records()) {
    auto file = files_.begin();
    auto last_file = std::prev(files_.end());
    while (file != last_file) {
      // Delete file and discard reserved space
      file->second->DeleteWarnIfFailed();
      file = files_.erase(file);
      first_sequencing_id_ = file->first;
      // Check if now there is enough space available.
      if (space_to_recover + options_.disk_space_resource()->GetUsed() <
          options_.disk_space_resource()->GetTotal()) {
        return true;
      }
    }
  }
  return false;
}

Status StorageQueue::SwitchLastFileIfNotEmpty() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  if (files_.empty()) {
    return Status(error::OUT_OF_RANGE,
                  "No files in the queue");  // No files in this queue yet.
  }
  if (files_.rbegin()->second->size() == 0) {
    return Status::StatusOK();  // Already empty.
  }
  files_.rbegin()->second->Close();
  ASSIGN_OR_RETURN(scoped_refptr<SingleFile> last_file, OpenNewWriteableFile());
  return Status::StatusOK();
}

std::map<int64_t, scoped_refptr<StorageQueue::SingleFile>>
StorageQueue::CollectFilesForUpload(int64_t sequencing_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  // Locate the last file that contains a sequencing ID <= sequencing_id. This
  // is to ensure that we do not miss an event that hasn't been uploaded (i.e.,
  // an event that has a sequencing ID >= sequencing_id). If no such file
  // exists, use files_.begin().
  auto file_it = files_.upper_bound(sequencing_id);
  if (file_it != files_.begin()) {
    --file_it;
  }

  // Create references to the files that will be uploaded.
  // Exclude the last file (still being written).
  std::map<int64_t, scoped_refptr<SingleFile>> files;
  for (; file_it != files_.end() &&
         file_it->second.get() != files_.rbegin()->second.get();
       ++file_it) {
    files.emplace(file_it->first, file_it->second);  // Adding reference.
  }
  return files;
}

class StorageQueue::ConfirmContext : public TaskRunnerContext<Status> {
 public:
  ConfirmContext(SequenceInformation sequence_information,
                 bool force,
                 base::OnceCallback<void(Status)> end_callback,
                 scoped_refptr<StorageQueue> storage_queue)
      : TaskRunnerContext<Status>(std::move(end_callback),
                                  storage_queue->sequenced_task_runner_),
        sequence_information_(std::move(sequence_information)),
        force_(force),
        storage_queue_(storage_queue) {
    CHECK(storage_queue.get());
  }

 private:
  // Context can only be deleted by calling Response method.
  ~ConfirmContext() override = default;

  void OnStart() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        storage_queue_->storage_queue_sequence_checker_);
    if (sequence_information_.generation_id() !=
        storage_queue_->generation_id_) {
      Response(Status(
          error::FAILED_PRECONDITION,
          base::StrCat(
              {"Generation mismatch - ",
               base::NumberToString(sequence_information_.generation_id()),
               ", expected=",
               base::NumberToString(storage_queue_->generation_id_)})));
      return;
    }
    if (force_) {
      storage_queue_->first_unconfirmed_sequencing_id_ =
          sequence_information_.sequencing_id() + 1;
      Response(Status::StatusOK());
    } else {
      Response(storage_queue_->RemoveConfirmedData(
          sequence_information_.sequencing_id()));
    }
  }

  // Confirmed sequencing information.
  const SequenceInformation sequence_information_;

  // Force-confirm flag.
  const bool force_;

  const scoped_refptr<StorageQueue> storage_queue_;
};

void StorageQueue::Confirm(SequenceInformation sequence_information,
                           bool force,
                           base::OnceCallback<void(Status)> completion_cb) {
  Start<ConfirmContext>(std::move(sequence_information), force,
                        std::move(completion_cb), this);
}

Status StorageQueue::RemoveConfirmedData(int64_t sequencing_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  // Update first unconfirmed id, unless new one is lower.
  if (!first_unconfirmed_sequencing_id_.has_value() ||
      first_unconfirmed_sequencing_id_.value() <= sequencing_id) {
    first_unconfirmed_sequencing_id_ = sequencing_id + 1;
  }
  // Update first available id, if new one is higher.
  if (first_sequencing_id_ <= sequencing_id) {
    first_sequencing_id_ = sequencing_id + 1;
  }
  if (active_read_operations_ > 0) {
    // If there are read locks registered, bail out
    // (expect to remove unused files later).
    return Status::StatusOK();
  }
  // Remove all files with sequencing ids below or equal only.
  // Note: files_ cannot be empty ever (there is always the current
  // file for writing).
  for (;;) {
    CHECK(!files_.empty()) << "Empty storage queue";
    auto next_it = std::next(files_.begin());  // Need to consider the next file
    if (next_it == files_.end()) {
      // We are on the last file, keep it.
      break;
    }
    if (next_it->first > sequencing_id + 1) {
      // Current file ends with (next_it->first - 1).
      // If it is sequencing_id >= (next_it->first - 1), we must keep it.
      break;
    }
    // Current file holds only ids <= sequencing_id.
    // Delete it.
    files_.begin()->second->Close();
    files_.begin()->second->DeleteWarnIfFailed();
    files_.erase(files_.begin());
  }
  // Even if there were errors, ignore them.
  return Status::StatusOK();
}

void StorageQueue::CheckBackUpload(Status status, int64_t next_sequencing_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  if (!status.ok()) {
    // Previous upload failed, retry.
    Start<ReadContext>(UploaderInterface::UploadReason::FAILURE_RETRY,
                       base::DoNothing(), this);
    return;
  }

  if (!first_unconfirmed_sequencing_id_.has_value() ||
      first_unconfirmed_sequencing_id_.value() < next_sequencing_id) {
    // Not all uploaded events were confirmed after upload, retry.
    Start<ReadContext>(UploaderInterface::UploadReason::INCOMPLETE_RETRY,
                       base::DoNothing(), this);
    return;
  }

  // No need to retry.
}

void StorageQueue::PeriodicUpload() {
  Start<ReadContext>(UploaderInterface::UploadReason::PERIODIC,
                     base::DoNothing(), this);
}

void StorageQueue::Flush(base::OnceCallback<void(Status)> completion_cb) {
  Start<ReadContext>(UploaderInterface::UploadReason::MANUAL,
                     std::move(completion_cb), this);
}

void StorageQueue::ReleaseAllFileInstances() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(storage_queue_sequence_checker_);
  // Close files explicitly, because they might be still referred by contexts.
  for (auto& file : files_) {
    file.second->Close();
  }
  files_.clear();
}

void StorageQueue::RegisterCompletionCallback(base::OnceClosure callback) {
  // Although this is an asynchronous action, note that `StorageQueue` cannot be
  // destructed until the callback is registered - `StorageQueue` is held by
  // the added reference here. Thus, the callback being registered is guaranteed
  // to be called only when `StorageQueue` is being destructed.
  CHECK(callback);
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceClosure callback, scoped_refptr<StorageQueue> self) {
            self->completion_closure_list_->RegisterCompletionCallback(
                std::move(callback));
          },
          std::move(callback), base::WrapRefCounted(this)));
}

void StorageQueue::TestInjectErrorsForOperation(
    test::ErrorInjectionHandlerType handler) {
  test_injection_handler_ = handler;
}

//
// SingleFile implementation
//
StatusOr<scoped_refptr<StorageQueue::SingleFile>>
StorageQueue::SingleFile::Create(
    const base::FilePath& filename,
    int64_t size,
    scoped_refptr<ResourceManager> memory_resource,
    scoped_refptr<ResourceManager> disk_space_resource,
    scoped_refptr<RefCountedClosureList> completion_closure_list) {
  // Reserve specified disk space for the file.
  if (!disk_space_resource->Reserve(size)) {
    LOG(WARNING) << "Disk space exceeded adding file "
                 << filename.MaybeAsASCII();
    return base::unexpected(
        Status(error::RESOURCE_EXHAUSTED,
               base::StrCat({"Not enough disk space available to include file=",
                             filename.MaybeAsASCII()})));
  }

  // Cannot use base::MakeRefCounted, since the constructor is private.
  return scoped_refptr<StorageQueue::SingleFile>(
      new SingleFile(filename, size, memory_resource, disk_space_resource,
                     completion_closure_list));
}

StorageQueue::SingleFile::SingleFile(
    const base::FilePath& filename,
    int64_t size,
    scoped_refptr<ResourceManager> memory_resource,
    scoped_refptr<ResourceManager> disk_space_resource,
    scoped_refptr<RefCountedClosureList> completion_closure_list)
    : completion_closure_list_(completion_closure_list),
      filename_(filename),
      size_(size),
      memory_resource_(memory_resource),
      disk_space_resource_(disk_space_resource),
      buffer_(memory_resource) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

StorageQueue::SingleFile::~SingleFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  disk_space_resource_->Discard(size_);
  Close();
}

Status StorageQueue::SingleFile::Open(bool read_only) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (handle_) {
    CHECK_EQ(is_readonly(), read_only);
    // TODO(b/157943192): Restart auto-closing timer.
    return Status::StatusOK();
  }
  handle_ = std::make_unique<base::File>(
      filename_, read_only ? (base::File::FLAG_OPEN | base::File::FLAG_READ)
                           : (base::File::FLAG_OPEN_ALWAYS |
                              base::File::FLAG_APPEND | base::File::FLAG_READ));
  if (!handle_ || !handle_->IsValid()) {
    handle_.reset();
    base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                  DataLossErrorReason::FAILED_TO_OPEN_FILE,
                                  DataLossErrorReason::MAX_VALUE);
    return Status(error::DATA_LOSS,
                  base::StrCat({"Cannot open file=", name(), " for ",
                                read_only ? "read" : "append"}));
  }
  is_readonly_ = read_only;
  if (!read_only) {
    int64_t file_size = handle_->GetLength();
    if (file_size < 0) {
      base::UmaHistogramEnumeration(
          reporting::kUmaDataLossErrorReason,
          DataLossErrorReason::FAILED_TO_GET_SIZE_OF_FILE,
          DataLossErrorReason::MAX_VALUE);
      return Status(error::DATA_LOSS,
                    base::StrCat({"Cannot get size of file=", name()}));
    }
    size_ = static_cast<uint64_t>(file_size);
  }
  return Status::StatusOK();
}

void StorageQueue::SingleFile::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_readonly_ = std::nullopt;
  buffer_.Clear();
  if (!handle_) {
    // TODO(b/157943192): Restart auto-closing timer.
    return;
  }
  handle_.reset();
}

void StorageQueue::SingleFile::DeleteWarnIfFailed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!handle_) << "Deleting file that is still open";
  disk_space_resource_->Discard(size_);
  size_ = 0;
  DeleteFileWarnIfFailed(filename_);
}

StatusOr<std::string_view> StorageQueue::SingleFile::Read(
    uint32_t pos,
    uint32_t size,
    size_t max_buffer_size,
    bool expect_readonly) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!handle_) {
    base::UmaHistogramEnumeration(reporting::kUmaUnavailableErrorReason,
                                  UnavailableErrorReason::FILE_NOT_OPEN,
                                  UnavailableErrorReason::MAX_VALUE);
    return base::unexpected(
        Status(error::UNAVAILABLE, base::StrCat({"File not open ", name()})));
  }
  if (expect_readonly != is_readonly()) {
    return base::unexpected(Status(
        error::INTERNAL, base::StrCat({"Attempt to read ",
                                       is_readonly() ? "readonly" : "writeable",
                                       " File ", name()})));
  }
  if (size > max_buffer_size) {
    return base::unexpected(
        Status(error::RESOURCE_EXHAUSTED, "Too much data to read"));
  }
  if (size_ == 0) {
    // Empty file, return EOF right away.
    return base::unexpected(Status(error::OUT_OF_RANGE, "End of file"));
  }
  // If no buffer yet, allocate.
  // TODO(b/157943192): Add buffer management - consider adding an UMA for
  // tracking the average + peak memory the Storage module is consuming.
  if (buffer_.empty()) {
    const auto buffer_size =
        std::min(max_buffer_size, RoundUpToFrameSize(size_));
    auto alloc_status = buffer_.Allocate(buffer_size);
    if (!alloc_status.ok()) {
      return base::unexpected(std::move(alloc_status));
    }
    data_start_ = data_end_ = 0;
    file_position_ = 0;
  }
  // If file position does not match, reset buffer.
  if (pos != file_position_) {
    data_start_ = data_end_ = 0;
    file_position_ = pos;
  }
  // If expected data size does not fit into the buffer, move what's left to the
  // start.
  if (data_start_ + size > buffer_.size()) {
    CHECK_GT(data_start_, 0u);  // Cannot happen if 0.
    if (data_end_ > data_start_) {
      memmove(buffer_.at(0), buffer_.at(data_start_), data_end_ - data_start_);
    }
    data_end_ -= data_start_;
    data_start_ = 0;
  }
  size_t actual_size = data_end_ - data_start_;
  pos += actual_size;
  while (actual_size < size) {
    // Read as much as possible.
    CHECK_LT(data_end_, buffer_.size());
    const int32_t result =
        handle_->Read(pos, buffer_.at(data_end_), buffer_.size() - data_end_);
    if (result < 0) {
      base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                    DataLossErrorReason::FAILED_TO_READ_FILE,
                                    DataLossErrorReason::MAX_VALUE);
      return base::unexpected(Status(
          error::DATA_LOSS,
          base::StrCat({"File read error=",
                        handle_->ErrorToString(handle_->GetLastFileError()),
                        " ", name()})));
    }
    if (result == 0) {
      break;
    }
    pos += result;
    data_end_ += result;
    CHECK_LE(data_end_, buffer_.size());
    actual_size += result;
  }
  if (actual_size > size) {
    actual_size = size;
  }
  // If nothing read, report end of file.
  if (actual_size == 0) {
    return base::unexpected(Status(error::OUT_OF_RANGE, "End of file"));
  }
  // Prepare reference to actually loaded data.
  auto read_data = std::string_view(buffer_.at(data_start_), actual_size);
  // Move start and file position to after that data.
  data_start_ += actual_size;
  file_position_ += actual_size;
  CHECK_LE(data_start_, data_end_);
  // Return what has been loaded.
  return read_data;
}

StatusOr<uint32_t> StorageQueue::SingleFile::Append(std::string_view data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!handle_) {
    base::UmaHistogramEnumeration(reporting::kUmaUnavailableErrorReason,
                                  UnavailableErrorReason::FILE_NOT_OPEN,
                                  UnavailableErrorReason::MAX_VALUE);
    return base::unexpected(
        Status(error::UNAVAILABLE, base::StrCat({"File not open ", name()})));
  }
  if (is_readonly()) {
    return base::unexpected(
        Status(error::INTERNAL,
               base::StrCat({"Attempt to append to read-only File ", name()})));
  }
  size_t actual_size = 0;
  while (data.size() > 0) {
    const int32_t result = handle_->Write(size_, data.data(), data.size());
    if (result < 0) {
      base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                    DataLossErrorReason::FAILED_TO_WRITE_FILE,
                                    DataLossErrorReason::MAX_VALUE);
      return base::unexpected(Status(
          error::DATA_LOSS,
          base::StrCat({"File write error=",
                        handle_->ErrorToString(handle_->GetLastFileError()),
                        " ", name()})));
    }
    size_ += result;
    actual_size += result;
    data = data.substr(result);  // Skip data that has been written.
  }
  return actual_size;
}
}  // namespace reporting
