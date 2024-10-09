// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/download/public/common/download_file_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/download/internal/common/parallel_download_utils.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_destination_observer.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_interrupt_reasons_utils.h"
#include "components/download/public/common/download_stats.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "mojo/public/c/system/types.h"
#include "net/base/io_buffer.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/content_uri_utils.h"
#include "components/download/internal/common/android/download_collection_bridge.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace download {

namespace {

const int kUpdatePeriodMs = 500;
const int kMaxTimeBlockingFileThreadMs = 1000;

// These constants control the default retry behavior for failing renames. Each
// retry is performed after a delay that is twice the previous delay. The
// initial delay is specified by kInitialRenameRetryDelayMs.
const int kInitialRenameRetryDelayMs = 200;

// Number of times a failing rename is retried before giving up.
const int kMaxRenameRetries = 3;

// Because DownloadSaveInfo::kLengthFullContent is 0, we should avoid using
// 0 for length if we found that a stream can no longer write any data.
const int kNoBytesToWrite = -1;

// Default content length when the potential file size is not yet determined.
const int kUnknownContentLength = -1;

#if BUILDFLAG(IS_MAC)
void UnHideFile(const base::FilePath& path) {
  base::stat_wrapper_t stat;
  if (base::File::Stat(path, &stat) < 0) {
    return;
  }

  if (!S_ISREG(stat.st_mode)) {
    return;
  }

  // Skip files starting with ".".
  if (base::StartsWith(path.BaseName().value(), ".")) {
    return;
  }

  // Update the file's hidden flags.
  if (stat.st_flags & UF_HIDDEN) {
    stat.st_flags ^= UF_HIDDEN;
    chflags(path.value().c_str(), stat.st_flags);
  }
}
#endif

}  // namespace

DownloadFileImpl::SourceStream::SourceStream(
    int64_t offset,
    int64_t starting_file_write_offset,
    std::unique_ptr<InputStream> stream)
    : offset_(offset),
      length_(DownloadSaveInfo::kLengthFullContent),
      starting_file_write_offset_(starting_file_write_offset),
      bytes_read_(0),
      bytes_written_(0),
      finished_(false),
      index_(0u),
      input_stream_(std::move(stream)) {
  CHECK_LE(offset_, starting_file_write_offset_);
  CHECK_GE(offset_, 0);
}

DownloadFileImpl::SourceStream::~SourceStream() = default;

void DownloadFileImpl::SourceStream::Initialize() {
  input_stream_->Initialize();
}

void DownloadFileImpl::SourceStream::OnBytesConsumed(int64_t bytes_read,
                                                     int64_t bytes_written) {
  CHECK_GE(bytes_read, bytes_written);
  bytes_read_ += bytes_read;
  bytes_written_ += bytes_written;
}

void DownloadFileImpl::SourceStream::TruncateLengthWithWrittenDataBlock(
    int64_t received_slice_offset,
    int64_t bytes_written) {
  DCHECK_GT(bytes_written, 0);
  if (length_ == kNoBytesToWrite)
    return;

  if (received_slice_offset <= starting_file_write_offset_) {
    // If validation has completed, mark the stream as finished if the file
    // write position already has data.
    if (received_slice_offset + bytes_written > starting_file_write_offset_ &&
        GetRemainingBytesToValidate() == 0) {
      length_ = kNoBytesToWrite;
      finished_ = true;
    }
    return;
  }

  if (length_ == DownloadSaveInfo::kLengthFullContent ||
      (length_ > received_slice_offset - offset_ &&
       length_ > starting_file_write_offset_ - offset_)) {
    // Stream length should always include the validation data, unless the
    // response is too short.
    length_ =
        std::max(received_slice_offset, starting_file_write_offset_) - offset_;
  }
}

void DownloadFileImpl::SourceStream::RegisterDataReadyCallback(
    const mojo::SimpleWatcher::ReadyCallback& callback) {
  input_stream_->RegisterDataReadyCallback(callback);
}

void DownloadFileImpl::SourceStream::ClearDataReadyCallback() {
  read_stream_callback_.Cancel();
  input_stream_->ClearDataReadyCallback();
}

DownloadInterruptReason DownloadFileImpl::SourceStream::GetCompletionStatus()
    const {
  return input_stream_->GetCompletionStatus();
}

void DownloadFileImpl::SourceStream::RequestCompletionNotification(
    base::WeakPtr<DownloadFileImpl> download_file) {
  input_stream_->RegisterCompletionCallback(base::BindOnce(
      &DownloadFileImpl::OnStreamCompleted, std::move(download_file),
      // Precondition: `download_file` owns `this`.
      base::Unretained(this)));
}

InputStream::StreamState DownloadFileImpl::SourceStream::Read(
    scoped_refptr<net::IOBuffer>* data,
    size_t* length) {
  return input_stream_->Read(data, length);
}

size_t DownloadFileImpl::SourceStream::GetRemainingBytesToValidate() {
  int64_t bytes_remaining = starting_file_write_offset_ - offset_ - bytes_read_;
  return bytes_remaining < 0 ? 0 : bytes_remaining;
}

DownloadFileImpl::DownloadFileImpl(
    std::unique_ptr<DownloadSaveInfo> save_info,
    const base::FilePath& default_download_directory,
    std::unique_ptr<InputStream> stream,
    uint32_t download_id,
    base::WeakPtr<DownloadDestinationObserver> observer)
    : file_(download_id),
      save_info_(std::move(save_info)),
      default_download_directory_(default_download_directory),
      potential_file_length_(kUnknownContentLength),
      bytes_seen_(0),
      num_active_streams_(0),
      is_paused_(false),
      download_id_(download_id),
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      observer_(observer) {
  TRACE_EVENT_INSTANT0("download", "DownloadFileCreated",
                       TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("download", "DownloadFileActive",
                                    download_id);

  source_streams_.insert(
      {save_info_->offset,
       std::make_unique<SourceStream>(save_info_->offset,
                                      save_info_->GetStartingFileWriteOffset(),
                                      std::move(stream))});

  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DownloadFileImpl::~DownloadFileImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT_NESTABLE_ASYNC_END0("download", "DownloadFileActive",
                                  download_id_);
}

void DownloadFileImpl::Initialize(
    InitializeCallback initialize_callback,
    CancelRequestCallback cancel_request_callback,
    const DownloadItem::ReceivedSlices& received_slices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  update_timer_ = std::make_unique<base::RepeatingTimer>();
  int64_t bytes_so_far = 0;
  cancel_request_callback_ = cancel_request_callback;
  received_slices_ = received_slices;
  if (!task_runner_)
    task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();

  // If the last slice is finished, then we know the actual content size.
  if (!received_slices_.empty() && received_slices_.back().finished) {
    SetPotentialFileLength(received_slices_.back().offset +
                           received_slices_.back().received_bytes);
  }

  if (IsSparseFile()) {
    for (const auto& received_slice : received_slices_)
      bytes_so_far += received_slice.received_bytes;
    slice_to_download_ = FindSlicesToDownload(received_slices_);

  } else {
    bytes_so_far = save_info_->GetStartingFileWriteOffset();
  }

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  // Create the obfuscator if enterprise deep scanning is enabled.
  if (save_info_->needs_obfuscation && !IsParallelDownloadEnabled()) {
    obfuscator_ =
        std::make_unique<enterprise_obfuscation::DownloadObfuscator>();
  }
#endif

  int64_t bytes_wasted = 0;
  DownloadInterruptReason reason = file_.Initialize(
      save_info_->file_path, default_download_directory_,
      std::move(save_info_->file), bytes_so_far,
      save_info_->hash_of_partial_file, std::move(save_info_->hash_state),
      IsSparseFile(), &bytes_wasted);
  if (reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(initialize_callback), reason, bytes_wasted));
    return;
  }
  download_start_ = base::TimeTicks::Now();

  // Primarily to make reset to zero in restart visible to owner.
  SendUpdate();

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(initialize_callback),
                                DOWNLOAD_INTERRUPT_REASON_NONE, bytes_wasted));

  // Initial pull from the straw from all source streams.
  for (auto& source_stream : source_streams_)
    RegisterAndActivateStream(source_stream.second.get());
}

void DownloadFileImpl::AddInputStream(std::unique_ptr<InputStream> stream,
                                      int64_t offset) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // UI thread may not be notified about completion and detach download file,
  // clear up the network request.
  if (IsDownloadCompleted()) {
    CancelRequest(offset);
    return;
  }
  auto [it, inserted] =
      source_streams_.insert({offset, std::make_unique<SourceStream>(
                                          offset, offset, std::move(stream))});
  CHECK(inserted);
  OnSourceStreamAdded(it->second.get());
}

void DownloadFileImpl::OnSourceStreamAdded(SourceStream* source_stream) {
  // There are writers at different offsets now, create the received slices
  // vector if necessary.
  if (received_slices_.empty() && TotalBytesReceived() > 0) {
    size_t index = AddOrMergeReceivedSliceIntoSortedArray(
        DownloadItem::ReceivedSlice(0, TotalBytesReceived()), received_slices_);
    DCHECK_EQ(index, 0u);
  }
  // If the file is initialized, start to write data, or wait until file opened.
  if (file_.in_progress())
    RegisterAndActivateStream(source_stream);
}

DownloadInterruptReason DownloadFileImpl::ValidateAndWriteDataToFile(
    int64_t offset,
    const char* data,
    size_t bytes_to_validate,
    size_t bytes_to_write) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Check if some of the data is for validation purpose.
  bool should_validate = bytes_to_validate > 0;
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  should_validate = should_validate && !obfuscator_;
#endif
  if (should_validate &&
      !file_.ValidateDataInFile(offset, data, bytes_to_validate)) {
    return DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH;
  }
  // If there is no data to write, just return DOWNLOAD_INTERRUPT_REASON_NONE
  // and read the next chunk.
  if (bytes_to_write <= 0)
    return DOWNLOAD_INTERRUPT_REASON_NONE;

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  if (obfuscator_) {
    bool is_last_chunk =
        save_info_->total_bytes > 0 &&
        static_cast<int64_t>(offset + bytes_to_validate + bytes_to_write) ==
            save_info_->total_bytes;
    auto obfuscated_data = obfuscator_->ObfuscateChunk(
        base::span(reinterpret_cast<const uint8_t*>(data + bytes_to_validate),
                   bytes_to_write),
        is_last_chunk);

    // TODO(b/367259664): Add better error handling for file obfuscation.
    if (!obfuscated_data.has_value()) {
      return DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
    }

    WillWriteToDisk(obfuscated_data.value().size());
    return file_.WriteDataToFile(
        file_.bytes_so_far(),
        reinterpret_cast<const char*>(obfuscated_data.value().data()),
        obfuscated_data.value().size());
  }
#endif

  // Write the remaining data to disk.
  WillWriteToDisk(bytes_to_write);
  return file_.WriteDataToFile(offset + bytes_to_validate,
                               data + bytes_to_validate, bytes_to_write);
}

bool DownloadFileImpl::CalculateBytesToWrite(SourceStream* source_stream,
                                             size_t bytes_available_to_write,
                                             size_t* bytes_to_validate,
                                             size_t* bytes_to_write) {
  *bytes_to_validate = 0;
  if (source_stream->length() == kNoBytesToWrite) {
    *bytes_to_write = 0;
    return true;
  }

  // First calculate the number of bytes to validate.
  *bytes_to_write = bytes_available_to_write;
  size_t remaining_bytes_to_validate =
      source_stream->GetRemainingBytesToValidate();
  if (remaining_bytes_to_validate > 0) {
    *bytes_to_validate =
        std::min(remaining_bytes_to_validate, bytes_available_to_write);
    *bytes_to_write -= *bytes_to_validate;
  }
  if (source_stream->length() != DownloadSaveInfo::kLengthFullContent &&
      source_stream->bytes_read() +
              static_cast<int64_t>(bytes_available_to_write) >
          source_stream->length()) {
    // Total bytes to consume is capped by the length of the stream.
    int64_t bytes_to_consume =
        source_stream->length() - source_stream->bytes_read();
    // The validation data should always be streamed.
    DCHECK_GE(bytes_to_consume, static_cast<int64_t>(*bytes_to_validate));
    *bytes_to_write = bytes_to_consume - *bytes_to_validate;
    return true;
  }

  // If a new slice finds that its target position has already been written,
  // terminate the stream if there are no bytes to validate.
  if (source_stream->bytes_written() == 0 && *bytes_to_write > 0) {
    for (const auto& received_slice : received_slices_) {
      if (received_slice.offset <=
              source_stream->starting_file_write_offset() &&
          received_slice.offset + received_slice.received_bytes >
              source_stream->starting_file_write_offset()) {
        *bytes_to_write = 0;
        return true;
      }
    }
  }

  return false;
}

void DownloadFileImpl::RenameAndUniquify(const base::FilePath& full_path,
                                         RenameCompletionCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  if (full_path.IsContentUri()) {
    DownloadInterruptReason reason = file_.Rename(full_path);
    OnRenameComplete(full_path, std::move(callback), reason);
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)
  std::unique_ptr<RenameParameters> parameters(
      new RenameParameters(UNIQUIFY, full_path, std::move(callback)));
  RenameWithRetryInternal(std::move(parameters));
}

void DownloadFileImpl::RenameAndAnnotate(
    const base::FilePath& full_path,
    const std::string& client_guid,
    const GURL& source_url,
    const GURL& referrer_url,
    const std::optional<url::Origin>& request_initiator,
    mojo::PendingRemote<quarantine::mojom::Quarantine> remote_quarantine,
    RenameCompletionCallback callback) {
  std::unique_ptr<RenameParameters> parameters(new RenameParameters(
      ANNOTATE_WITH_SOURCE_INFORMATION, full_path, std::move(callback)));
  parameters->client_guid = client_guid;
  parameters->source_url = source_url;
  parameters->referrer_url = referrer_url;
  parameters->request_initiator = request_initiator;
  parameters->remote_quarantine = std::move(remote_quarantine);
  RenameWithRetryInternal(std::move(parameters));
}

#if BUILDFLAG(IS_ANDROID)
void DownloadFileImpl::PublishDownload(RenameCompletionCallback callback) {
  DownloadInterruptReason reason = file_.PublishDownload();
  OnRenameComplete(file_.full_path(), std::move(callback), reason);
}
#endif  // BUILDFLAG(IS_ANDROID)

base::TimeDelta DownloadFileImpl::GetRetryDelayForFailedRename(
    int attempt_number) {
  DCHECK_GE(attempt_number, 0);
  // |delay| starts at kInitialRenameRetryDelayMs and increases by a factor of
  // 2 at each subsequent retry. Assumes that |retries_left| starts at
  // kMaxRenameRetries. Also assumes that kMaxRenameRetries is less than the
  // number of bits in an int.
  return base::Milliseconds(kInitialRenameRetryDelayMs) * (1 << attempt_number);
}

bool DownloadFileImpl::ShouldRetryFailedRename(DownloadInterruptReason reason) {
  return reason == DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR;
}

DownloadInterruptReason DownloadFileImpl::HandleStreamCompletionStatus(
    SourceStream* source_stream) {
  DownloadInterruptReason reason = source_stream->GetCompletionStatus();
  if (source_stream->length() == DownloadSaveInfo::kLengthFullContent &&
      !received_slices_.empty() &&
      (source_stream->starting_file_write_offset() ==
       received_slices_.back().offset +
           received_slices_.back().received_bytes) &&
      reason == DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE) {
    // We are probably reaching the end of the stream, don't treat this
    // as an error.
    return DOWNLOAD_INTERRUPT_REASON_NONE;
  }
  return reason;
}

void DownloadFileImpl::RenameWithRetryInternal(
    std::unique_ptr<RenameParameters> parameters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::FilePath new_path = parameters->new_path;

  if ((parameters->option & UNIQUIFY) && new_path != file_.full_path()) {
    new_path = base::GetUniquePath(new_path);
  }

  DownloadInterruptReason reason = file_.Rename(new_path);

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  // Handle the case where the file is shorter due to deobfuscation.
  if (obfuscator_ && reason == DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT) {
    int64_t expected_size = bytes_seen_ + obfuscator_->GetTotalOverhead();
    int64_t actual_size = file_.bytes_so_far();
    if (expected_size >= 0 && actual_size == expected_size) {
      // Ignore error as the file was deobfuscated before being renamed.
      reason = DOWNLOAD_INTERRUPT_REASON_NONE;
    }
  }
#endif

  // Attempt to retry the rename if possible. If the rename failed and the
  // subsequent open also failed, then in_progress() would be false. We don't
  // try to retry renames if the in_progress() was false to begin with since we
  // have less assurance that the file at file_.full_path() was the one we were
  // working with.
  if (ShouldRetryFailedRename(reason) && file_.in_progress() &&
      parameters->retries_left > 0) {
    int attempt_number = kMaxRenameRetries - parameters->retries_left;
    --parameters->retries_left;
    if (parameters->time_of_first_failure.is_null())
      parameters->time_of_first_failure = base::TimeTicks::Now();
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DownloadFileImpl::RenameWithRetryInternal,
                       weak_factory_.GetWeakPtr(), std::move(parameters)),
        GetRetryDelayForFailedRename(attempt_number));
    return;
  }

  if (reason == DOWNLOAD_INTERRUPT_REASON_NONE &&
      (parameters->option & ANNOTATE_WITH_SOURCE_INFORMATION)) {
    // Doing the annotation after the rename rather than before leaves
    // a very small window during which the file has the final name but
    // hasn't been marked with the Mark Of The Web.  However, it allows
    // anti-virus scanners on Windows to actually see the data
    // (http://crbug.com/127999) under the correct name (which is information
    // it uses).
    //
    // If concurrent downloads with the same target path are allowed, an
    // asynchronous quarantine file may cause a file to be stamped with
    // incorrect mark-of-the-web data. Therefore, fall back to non-service
    // QuarantineFile when kPreventDownloadsWithSamePath is disabled.
    file_.AnnotateWithSourceInformation(
        parameters->client_guid, parameters->source_url,
        parameters->referrer_url, parameters->request_initiator,
        std::move(parameters->remote_quarantine),
        base::BindOnce(&DownloadFileImpl::OnRenameComplete,
                       weak_factory_.GetWeakPtr(), new_path,
                       std::move(parameters->completion_callback)));
    return;
  }

  OnRenameComplete(new_path, std::move(parameters->completion_callback),
                   reason);
}

void DownloadFileImpl::OnRenameComplete(const base::FilePath& new_path,
                                        RenameCompletionCallback callback,
                                        DownloadInterruptReason reason) {
  if (reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
    // Make sure our information is updated, since we're about to
    // error out.
    SendUpdate();

    // Null out callback so that we don't do any more stream processing.
    // The request that writes to the pipe should be canceled after
    // the download being interrupted.
    for (auto& stream : source_streams_)
      stream.second->ClearDataReadyCallback();
  }
#if BUILDFLAG(IS_MAC)
  else {
    UnHideFile(new_path);
  }
#endif

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), reason,
                                reason == DOWNLOAD_INTERRUPT_REASON_NONE
                                    ? new_path
                                    : base::FilePath()));
}

void DownloadFileImpl::Detach() {
  file_.Detach();
}

void DownloadFileImpl::Cancel() {
  file_.Cancel();
}

void DownloadFileImpl::SetPotentialFileLength(int64_t length) {
  DCHECK(potential_file_length_ == length ||
         potential_file_length_ == kUnknownContentLength)
      << "Potential file length changed, the download might have updated.";

  if (length < potential_file_length_ ||
      potential_file_length_ == kUnknownContentLength) {
    potential_file_length_ = length;
  }

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  if (obfuscator_) {
    potential_file_length_ += obfuscator_->GetTotalOverhead();
  }
#endif

  // TODO(qinmin): interrupt the download if the received bytes are larger
  // than content length limit.
  LOG_IF(ERROR, TotalBytesReceived() > potential_file_length_)
      << "Received data is larger than the content length limit.";
}

const base::FilePath& DownloadFileImpl::FullPath() const {
  return file_.full_path();
}

bool DownloadFileImpl::InProgress() const {
  return file_.in_progress();
}

void DownloadFileImpl::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_paused_ = true;
  for (auto& stream : source_streams_)
    stream.second->ClearDataReadyCallback();

  // Stop sending updates since meaningless after paused.
  if (update_timer_ && update_timer_->IsRunning())
    update_timer_->Stop();
}

void DownloadFileImpl::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_paused_);
  is_paused_ = false;

  for (auto& stream : source_streams_) {
    SourceStream* source_stream = stream.second.get();
    if (!source_stream->is_finished())
      StreamActive(source_stream, MOJO_RESULT_OK);
  }
}

void DownloadFileImpl::StreamActive(SourceStream* source_stream,
                                    MojoResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_paused_)
    return;

  base::TimeTicks start(base::TimeTicks::Now());
  base::TimeTicks now;
  scoped_refptr<net::IOBuffer> incoming_data;
  size_t incoming_data_size = 0;
  size_t total_incoming_data_size = 0;
  size_t num_buffers = 0;
  size_t bytes_to_validate = 0;
  size_t bytes_to_write = 0;
  bool should_terminate = false;
  InputStream::StreamState state(InputStream::EMPTY);
  DownloadInterruptReason reason = DOWNLOAD_INTERRUPT_REASON_NONE;
  base::TimeDelta delta(base::Milliseconds(kMaxTimeBlockingFileThreadMs));
  // Take care of any file local activity required.
  do {
    state = source_stream->Read(&incoming_data, &incoming_data_size);
    switch (state) {
      case InputStream::EMPTY:
        should_terminate = (source_stream->length() == kNoBytesToWrite);
        break;
      case InputStream::HAS_DATA: {
        ++num_buffers;
        should_terminate =
            CalculateBytesToWrite(source_stream, incoming_data_size,
                                  &bytes_to_validate, &bytes_to_write);
        DCHECK_GE(incoming_data_size, bytes_to_write);
        reason = ValidateAndWriteDataToFile(
            source_stream->offset() + source_stream->bytes_read(),
            incoming_data->data(), bytes_to_validate, bytes_to_write);
        bytes_seen_ += bytes_to_write;
        total_incoming_data_size += incoming_data_size;
        if (reason == DOWNLOAD_INTERRUPT_REASON_NONE) {
          int64_t prev_bytes_written = source_stream->bytes_written();
          source_stream->OnBytesConsumed(incoming_data_size, bytes_to_write);
          if (!IsSparseFile())
            break;
          // If the write operation creates a new slice, add it to the
          // |received_slices_| and update all the entries in
          // |source_streams_|.
          if (bytes_to_write > 0 && prev_bytes_written == 0) {
            AddNewSlice(source_stream->starting_file_write_offset(),
                        bytes_to_write);
          } else {
            received_slices_[source_stream->index()].received_bytes +=
                bytes_to_write;
          }
        }
      } break;
      case InputStream::WAIT_FOR_COMPLETION:
        source_stream->RequestCompletionNotification(
            weak_factory_.GetWeakPtr());
        break;
      case InputStream::COMPLETE:
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
    now = base::TimeTicks::Now();
  } while (state == InputStream::HAS_DATA &&
           reason == DOWNLOAD_INTERRUPT_REASON_NONE && now - start <= delta &&
           !should_terminate);

  // If we're stopping to yield the thread, post a task so we come back.
  if (state == InputStream::HAS_DATA && now - start > delta &&
      !should_terminate) {
    source_stream->read_stream_callback()->Reset(base::BindOnce(
        &DownloadFileImpl::StreamActive, weak_factory_.GetWeakPtr(),
        source_stream, MOJO_RESULT_OK));
    task_runner_->PostTask(FROM_HERE,
                           source_stream->read_stream_callback()->callback());
  } else if (state == InputStream::EMPTY && !should_terminate) {
    source_stream->RegisterDataReadyCallback(
        base::BindRepeating(&DownloadFileImpl::StreamActive, weak_factory_.GetWeakPtr(),
                   source_stream));
  }

  if (state == InputStream::COMPLETE)
    OnStreamCompleted(source_stream);
  else
    NotifyObserver(source_stream, reason, state, should_terminate);

  TRACE_EVENT_INSTANT2("download", "DownloadStreamDrained",
                       TRACE_EVENT_SCOPE_THREAD, "stream_size",
                       total_incoming_data_size, "num_buffers", num_buffers);
}

void DownloadFileImpl::OnStreamCompleted(SourceStream* source_stream) {
  DownloadInterruptReason reason = HandleStreamCompletionStatus(source_stream);
  SendUpdate();

  NotifyObserver(source_stream, reason, InputStream::COMPLETE, false);
}

void DownloadFileImpl::NotifyObserver(SourceStream* source_stream,
                                      DownloadInterruptReason reason,
                                      InputStream::StreamState stream_state,
                                      bool should_terminate) {
  if (reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
    HandleStreamError(source_stream, reason);
  } else if (stream_state == InputStream::COMPLETE || should_terminate) {
    // Signal successful completion or termination of the current stream.
    source_stream->ClearDataReadyCallback();
    source_stream->set_finished(true);

    if (should_terminate)
      CancelRequest(source_stream->offset());
    if (source_stream->length() == DownloadSaveInfo::kLengthFullContent) {
      // Mark received slice as finished.
      if (IsSparseFile() && source_stream->bytes_written() > 0) {
        DCHECK_GT(received_slices_.size(), source_stream->index())
            << "Received slice index out of bound!";
        received_slices_[source_stream->index()].finished = true;
      }
      if (!should_terminate) {
        SetPotentialFileLength(source_stream->offset() +
                               source_stream->bytes_read());
      }
    }
    num_active_streams_--;

    // Inform observers.
    SendUpdate();

    // All the stream reader are completed, shut down file IO processing.
    if (IsDownloadCompleted()) {
      OnDownloadCompleted();
    } else {
      // If all the stream completes and we still not able to complete, trigger
      // a content length mismatch error so auto resumption will be performed.
      SendErrorUpdateIfFinished(
          DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH);
    }
  }
}

void DownloadFileImpl::OnDownloadCompleted() {
  RecordFileBandwidth(bytes_seen_, base::TimeTicks::Now() - download_start_);
  weak_factory_.InvalidateWeakPtrs();

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  // If total bytes not provided, we append an empty obfuscated chunk to
  // protect against truncation.
  if (obfuscator_ && save_info_->total_bytes == 0) {
    auto obfuscated_empty_data = obfuscator_->ObfuscateChunk({}, true);
    if (!obfuscated_empty_data.has_value()) {
      SendErrorUpdateIfFinished(DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
      return;
    }

    DownloadInterruptReason reason = file_.WriteDataToFile(
        file_.bytes_so_far(),
        reinterpret_cast<const char*>(obfuscated_empty_data.value().data()),
        obfuscated_empty_data.value().size());

    if (reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
      SendErrorUpdateIfFinished(reason);
      return;
    }
  }

  std::unique_ptr<crypto::SecureHash> hash_state =
      obfuscator_ ? obfuscator_->GetUnobfuscatedHash() : file_.Finish();
#else
  std::unique_ptr<crypto::SecureHash> hash_state = file_.Finish();
#endif

  update_timer_.reset();
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DownloadDestinationObserver::DestinationCompleted,
                     observer_, TotalBytesReceived(), std::move(hash_state)));
}

void DownloadFileImpl::RegisterAndActivateStream(SourceStream* source_stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  source_stream->Initialize();
  // Truncate |source_stream|'s length if necessary.
  for (const auto& received_slice : received_slices_) {
    source_stream->TruncateLengthWithWrittenDataBlock(
        received_slice.offset, received_slice.received_bytes);
  }
  num_active_streams_++;
  StreamActive(source_stream, MOJO_RESULT_OK);
}

int64_t DownloadFileImpl::TotalBytesReceived() const {
  return file_.bytes_so_far();
}

void DownloadFileImpl::SendUpdate() {
  // TODO(qinmin): For each active stream, add the slice it has written so
  // far along with received_slices_.
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DownloadDestinationObserver::DestinationUpdate, observer_,
                     TotalBytesReceived(), rate_estimator_.GetCountPerSecond(),
                     received_slices_));
}

void DownloadFileImpl::WillWriteToDisk(size_t data_len) {
  if (!update_timer_->IsRunning()) {
    update_timer_->Start(FROM_HERE, base::Milliseconds(kUpdatePeriodMs), this,
                         &DownloadFileImpl::SendUpdate);
  }
  rate_estimator_.Increment(data_len);
}

void DownloadFileImpl::AddNewSlice(int64_t offset, int64_t length) {
  size_t index = AddOrMergeReceivedSliceIntoSortedArray(
      DownloadItem::ReceivedSlice(offset, length), received_slices_);
  // Check if the slice is added as a new slice, or merged with an existing one.
  bool slice_added = (offset == received_slices_[index].offset);
  // Update the index of exising SourceStreams.
  for (auto& stream : source_streams_) {
    SourceStream* source_stream = stream.second.get();
    if (source_stream->starting_file_write_offset() > offset) {
      if (slice_added && source_stream->bytes_written() > 0)
        source_stream->set_index(source_stream->index() + 1);
    } else if (source_stream->starting_file_write_offset() == offset) {
      source_stream->set_index(index);
    } else {
      source_stream->TruncateLengthWithWrittenDataBlock(offset, length);
    }
  }
}

bool DownloadFileImpl::IsDownloadCompleted() {
  for (auto& stream : source_streams_) {
    if (!stream.second->is_finished())
      return false;
  }

  if (!IsSparseFile())
    return true;

  // Verify that all the file slices have been downloaded.
  std::vector<DownloadItem::ReceivedSlice> slices_to_download =
      FindSlicesToDownload(received_slices_);
  if (slices_to_download.size() > 1) {
    // If there are 1 or more holes in the file, download is not finished.
    // Some streams might not have been added to |source_streams_| yet.
    return false;
  }
  return TotalBytesReceived() == potential_file_length_;
}

void DownloadFileImpl::HandleStreamError(SourceStream* source_stream,
                                         DownloadInterruptReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  source_stream->ClearDataReadyCallback();
  source_stream->set_finished(true);
  num_active_streams_--;

  bool can_recover_from_error = false;
  if (reason != DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH) {
    // If previous stream has already written data at the starting offset of
    // the error stream. The download can complete.
    can_recover_from_error = (source_stream->length() == kNoBytesToWrite);

    // See if the previous stream can download the full content.
    // If the current stream has written some data, length of all preceding
    // streams will be truncated.
    if (IsSparseFile() && !can_recover_from_error) {
      SourceStream* preceding_neighbor = FindPrecedingNeighbor(source_stream);
      while (preceding_neighbor) {
        if (CanRecoverFromError(source_stream, preceding_neighbor)) {
          can_recover_from_error = true;
          break;
        }

        // If the neighbor cannot recover the error and it has already created
        // a slice, just interrupt the download.
        if (preceding_neighbor->bytes_written() > 0)
          break;
        preceding_neighbor = FindPrecedingNeighbor(preceding_neighbor);
      }
    }
  }

  SendUpdate();  // Make info up to date before error.

  // If the download can recover from error, check if it already finishes.
  // Otherwise, send an error update when all streams are finished.
  if (!can_recover_from_error)
    SendErrorUpdateIfFinished(reason);
  else if (IsDownloadCompleted())
    OnDownloadCompleted();
}

void DownloadFileImpl::SendErrorUpdateIfFinished(
    DownloadInterruptReason reason) {
  // If there are still active streams, let them finish before
  // sending out the error to the observer.
  if (num_active_streams_ > 0)
    return;

  if (IsSparseFile()) {
    for (const auto& slice : slice_to_download_) {
      // Ignore last slice beyond file length.
      if (slice.offset > 0 && slice.offset == potential_file_length_)
        continue;
      if (source_streams_.find(slice.offset) == source_streams_.end())
        return;
    }
  }

  // Error case for both upstream source and file write.
  // Shut down processing and signal an error to our observer.
  // Our observer will clean us up.
  weak_factory_.InvalidateWeakPtrs();

  // TODO(b/367257039): Maintain obfuscated file hash for interrupted downloads.
  std::unique_ptr<crypto::SecureHash> hash_state = file_.Finish();
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DownloadDestinationObserver::DestinationError, observer_,
                     reason, TotalBytesReceived(), std::move(hash_state)));
}

bool DownloadFileImpl::IsSparseFile() const {
  return source_streams_.size() > 1 || !received_slices_.empty();
}

DownloadFileImpl::SourceStream* DownloadFileImpl::FindPrecedingNeighbor(
    SourceStream* source_stream) {
  int64_t max_preceding_offset = 0;
  SourceStream* ret = nullptr;
  for (auto& stream : source_streams_) {
    int64_t offset = stream.second->starting_file_write_offset();
    if (offset < source_stream->starting_file_write_offset() &&
        offset >= max_preceding_offset) {
      ret = stream.second.get();
      max_preceding_offset = offset;
    }
  }
  return ret;
}

void DownloadFileImpl::CancelRequest(int64_t offset) {
  if (!cancel_request_callback_.is_null()) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(cancel_request_callback_, offset));
  }
}

void DownloadFileImpl::DebugStates() const {
  DVLOG(1) << "### Debugging DownloadFile states:";
  DVLOG(1) << "Total source stream count = " << source_streams_.size();
  for (const auto& stream : source_streams_) {
    DVLOG(1) << "Source stream, offset = " << stream.second->offset()
             << " , bytes_read = " << stream.second->bytes_read()
             << " , starting_file_write_offset = "
             << stream.second->starting_file_write_offset()
             << " , bytes_written = " << stream.second->bytes_written()
             << " , is_finished = " << stream.second->is_finished()
             << " , length = " << stream.second->length()
             << ", index = " << stream.second->index();
  }

  DebugSlicesInfo(received_slices_);
}

void DownloadFileImpl::SetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_ = std::move(task_runner);
}

DownloadFileImpl::RenameParameters::RenameParameters(
    RenameOption option,
    const base::FilePath& new_path,
    RenameCompletionCallback completion_callback)
    : option(option),
      new_path(new_path),
      retries_left(kMaxRenameRetries),
      completion_callback(std::move(completion_callback)) {}

DownloadFileImpl::RenameParameters::~RenameParameters() {}

}  // namespace download
