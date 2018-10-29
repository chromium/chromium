// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_file_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop_current.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/download/internal/common/parallel_download_utils.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_destination_observer.h"
#include "components/download/public/common/download_interrupt_reasons_utils.h"
#include "components/download/public/common/download_stats.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "mojo/public/c/system/types.h"
#include "net/base/io_buffer.h"
#include "services/network/public/cpp/features.h"

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

}  // namespace

DownloadFileImpl::SourceStream::SourceStream(
    int64_t offset,
    int64_t length,
    std::unique_ptr<InputStream> stream)
    : offset_(offset),
      length_(length),
      bytes_written_(0),
      finished_(false),
      index_(0u),
      input_stream_(std::move(stream)) {}

DownloadFileImpl::SourceStream::~SourceStream() = default;

void DownloadFileImpl::SourceStream::Initialize() {
  input_stream_->Initialize();
}

void DownloadFileImpl::SourceStream::OnWriteBytesToDisk(int64_t bytes_write) {
  bytes_written_ += bytes_write;
}

void DownloadFileImpl::SourceStream::TruncateLengthWithWrittenDataBlock(
    int64_t offset,
    int64_t bytes_written) {
  DCHECK_GT(bytes_written, 0);
  if (length_ == kNoBytesToWrite)
    return;

  if (offset <= offset_) {
    if (offset + bytes_written > offset_) {
      length_ = kNoBytesToWrite;
      finished_ = true;
    }
    return;
  }

  if (length_ == DownloadSaveInfo::kLengthFullContent ||
      length_ > offset - offset_) {
    length_ = offset - offset_;
  }
}

void DownloadFileImpl::SourceStream::RegisterDataReadyCallback(
    const mojo::SimpleWatcher::ReadyCallback& callback) {
  input_stream_->RegisterDataReadyCallback(callback);
}

void DownloadFileImpl::SourceStream::ClearDataReadyCallback() {
  input_stream_->ClearDataReadyCallback();
}

DownloadInterruptReason DownloadFileImpl::SourceStream::GetCompletionStatus()
    const {
  return input_stream_->GetCompletionStatus();
}

void DownloadFileImpl::SourceStream::RegisterCompletionCallback(
    DownloadFileImpl::SourceStream::CompletionCallback callback) {
  input_stream_->RegisterCompletionCallback(
      base::BindOnce(std::move(callback), base::Unretained(this)));
}

InputStream::StreamState DownloadFileImpl::SourceStream::Read(
    scoped_refptr<net::IOBuffer>* data,
    size_t* length) {
  return input_stream_->Read(data, length);
}

DownloadFileImpl::DownloadFileImpl(
    std::unique_ptr<DownloadSaveInfo> save_info,
    const base::FilePath& default_download_directory,
    std::unique_ptr<InputStream> stream,
    uint32_t download_id,
    base::WeakPtr<DownloadDestinationObserver> observer)
    : DownloadFileImpl(std::move(save_info),
                       default_download_directory,
                       download_id,
                       observer) {
  source_streams_[save_info_->offset] = std::make_unique<SourceStream>(
      save_info_->offset, save_info_->length, std::move(stream));
}

DownloadFileImpl::DownloadFileImpl(
    std::unique_ptr<DownloadSaveInfo> save_info,
    const base::FilePath& default_download_directory,
    uint32_t download_id,
    base::WeakPtr<DownloadDestinationObserver> observer)
    : file_(download_id),
      save_info_(std::move(save_info)),
      default_download_directory_(default_download_directory),
      potential_file_length_(kUnknownContentLength),
      bytes_seen_(0),
      num_active_streams_(0),
      record_stream_bandwidth_(false),
      bytes_seen_with_parallel_streams_(0),
      bytes_seen_without_parallel_streams_(0),
      is_paused_(false),
      download_id_(download_id),
      main_task_runner_(base::MessageLoopCurrent::Get()->task_runner()),
      observer_(observer),
      weak_factory_(this) {
  TRACE_EVENT_INSTANT0("download", "DownloadFileCreated",
                       TRACE_EVENT_SCOPE_THREAD);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("download", "DownloadFileActive",
                                    download_id);

  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DownloadFileImpl::~DownloadFileImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT_NESTABLE_ASYNC_END0("download", "DownloadFileActive",
                                  download_id_);
}

void DownloadFileImpl::Initialize(
    InitializeCallback initialize_callback,
    const CancelRequestCallback& cancel_request_callback,
    const DownloadItem::ReceivedSlices& received_slices,
    bool is_parallelizable) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  update_timer_.reset(new base::RepeatingTimer());
  int64_t bytes_so_far = 0;
  cancel_request_callback_ = cancel_request_callback;
  received_slices_ = received_slices;

  // If the last slice is finished, then we know the actual content size.
  if (!received_slices_.empty() && received_slices_.back().finished) {
    SetPotentialFileLength(received_slices_.back().offset +
                           received_slices_.back().received_bytes);
  }

  if (IsSparseFile()) {
    for (const auto& received_slice : received_slices_) {
      bytes_so_far += received_slice.received_bytes;
    }
  } else {
    bytes_so_far = save_info_->offset;
  }
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
  last_update_time_ = download_start_;
  record_stream_bandwidth_ = is_parallelizable;

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
                                      int64_t offset,
                                      int64_t length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // UI thread may not be notified about completion and detach download file,
  // clear up the network request.
  if (IsDownloadCompleted()) {
    CancelRequest(offset);
    return;
  }
  DCHECK(source_streams_.find(offset) == source_streams_.end());
  source_streams_[offset] =
      std::make_unique<SourceStream>(offset, length, std::move(stream));
  OnSourceStreamAdded(source_streams_[offset].get());
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

DownloadInterruptReason DownloadFileImpl::WriteDataToFile(int64_t offset,
                                                          const char* data,
                                                          size_t data_len) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  WillWriteToDisk(data_len);
  return file_.WriteDataToFile(offset, data, data_len);
}

bool DownloadFileImpl::CalculateBytesToWrite(SourceStream* source_stream,
                                             size_t bytes_available_to_write,
                                             size_t* bytes_to_write) {
  if (source_stream->length() == kNoBytesToWrite) {
    *bytes_to_write = 0;
    return true;
  }

  // If a new slice finds that its target position has already been written,
  // terminate the stream.
  if (source_stream->bytes_written() == 0) {
    for (const auto& received_slice : received_slices_) {
      if (received_slice.offset <= source_stream->offset() &&
          received_slice.offset + received_slice.received_bytes >
              source_stream->offset()) {
        *bytes_to_write = 0;
        return true;
      }
    }
  }

  if (source_stream->length() != DownloadSaveInfo::kLengthFullContent &&
      source_stream->bytes_written() +
              static_cast<int64_t>(bytes_available_to_write) >
          source_stream->length()) {
    // Write a partial buffer as the incoming data exceeds the length limit.
    *bytes_to_write = source_stream->length() - source_stream->bytes_written();
    return true;
  }

  *bytes_to_write = bytes_available_to_write;
  return false;
}

void DownloadFileImpl::RenameAndUniquify(
    const base::FilePath& full_path,
    const RenameCompletionCallback& callback) {
  std::unique_ptr<RenameParameters> parameters(
      new RenameParameters(UNIQUIFY, full_path, callback));
  RenameWithRetryInternal(std::move(parameters));
}

void DownloadFileImpl::RenameAndAnnotate(
    const base::FilePath& full_path,
    const std::string& client_guid,
    const GURL& source_url,
    const GURL& referrer_url,
    const RenameCompletionCallback& callback) {
  std::unique_ptr<RenameParameters> parameters(new RenameParameters(
      ANNOTATE_WITH_SOURCE_INFORMATION, full_path, callback));
  parameters->client_guid = client_guid;
  parameters->source_url = source_url;
  parameters->referrer_url = referrer_url;
  RenameWithRetryInternal(std::move(parameters));
}

base::TimeDelta DownloadFileImpl::GetRetryDelayForFailedRename(
    int attempt_number) {
  DCHECK_GE(attempt_number, 0);
  // |delay| starts at kInitialRenameRetryDelayMs and increases by a factor of
  // 2 at each subsequent retry. Assumes that |retries_left| starts at
  // kMaxRenameRetries. Also assumes that kMaxRenameRetries is less than the
  // number of bits in an int.
  return base::TimeDelta::FromMilliseconds(kInitialRenameRetryDelayMs) *
         (1 << attempt_number);
}

bool DownloadFileImpl::ShouldRetryFailedRename(DownloadInterruptReason reason) {
  return reason == DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR;
}

DownloadInterruptReason DownloadFileImpl::HandleStreamCompletionStatus(
    SourceStream* source_stream) {
  DownloadInterruptReason reason = source_stream->GetCompletionStatus();
  if (source_stream->length() == DownloadSaveInfo::kLengthFullContent &&
      !received_slices_.empty() &&
      (source_stream->offset() == received_slices_.back().offset +
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
    int uniquifier =
        base::GetUniquePathNumber(new_path, base::FilePath::StringType());
    if (uniquifier > 0)
      new_path = new_path.InsertBeforeExtensionASCII(
          base::StringPrintf(" (%d)", uniquifier));
  }

  DownloadInterruptReason reason = file_.Rename(new_path);

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
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
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
    reason = file_.AnnotateWithSourceInformation(parameters->client_guid,
                                                 parameters->source_url,
                                                 parameters->referrer_url);
  }

  if (reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
    // Make sure our information is updated, since we're about to
    // error out.
    SendUpdate();

    // Null out callback so that we don't do any more stream processing.
    // The request that writes to the pipe should be canceled after
    // the download being interrupted.
    for (auto& stream : source_streams_)
      stream.second->ClearDataReadyCallback();

    new_path.clear();
  }

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(parameters->completion_callback, reason, new_path));
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
  record_stream_bandwidth_ = false;
}

void DownloadFileImpl::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_paused_);
  is_paused_ = false;

  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  for (auto& stream : source_streams_) {
    SourceStream* source_stream = stream.second.get();
    if (!source_stream->is_finished()) {
      StreamActive(source_stream, MOJO_RESULT_OK);
    }
  }
}

void DownloadFileImpl::StreamActive(SourceStream* source_stream,
                                    MojoResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::FeatureList::IsEnabled(network::features::kNetworkService) &&
      is_paused_)
    return;

  base::TimeTicks start(base::TimeTicks::Now());
  base::TimeTicks now;
  scoped_refptr<net::IOBuffer> incoming_data;
  size_t incoming_data_size = 0;
  size_t total_incoming_data_size = 0;
  size_t num_buffers = 0;
  size_t bytes_to_write = 0;
  bool should_terminate = false;
  InputStream::StreamState state(InputStream::EMPTY);
  DownloadInterruptReason reason = DOWNLOAD_INTERRUPT_REASON_NONE;
  base::TimeDelta delta(
      base::TimeDelta::FromMilliseconds(kMaxTimeBlockingFileThreadMs));

  // Take care of any file local activity required.
  do {
    state = source_stream->Read(&incoming_data, &incoming_data_size);
    switch (state) {
      case InputStream::EMPTY:
        should_terminate = (source_stream->length() == kNoBytesToWrite);
        break;
      case InputStream::HAS_DATA: {
        ++num_buffers;
        should_terminate = CalculateBytesToWrite(
            source_stream, incoming_data_size, &bytes_to_write);
        DCHECK_GE(incoming_data_size, bytes_to_write);
        reason = WriteDataToFile(
            source_stream->offset() + source_stream->bytes_written(),
            incoming_data->data(), bytes_to_write);
        bytes_seen_ += bytes_to_write;
        total_incoming_data_size += bytes_to_write;
        if (reason == DOWNLOAD_INTERRUPT_REASON_NONE) {
          int64_t prev_bytes_written = source_stream->bytes_written();
          source_stream->OnWriteBytesToDisk(bytes_to_write);
          if (!IsSparseFile())
            break;
          // If the write operation creates a new slice, add it to the
          // |received_slices_| and update all the entries in
          // |source_streams_|.
          if (bytes_to_write > 0 && prev_bytes_written == 0) {
            AddNewSlice(source_stream->offset(), bytes_to_write);
          } else {
            received_slices_[source_stream->index()].received_bytes +=
                bytes_to_write;
          }
        }
      } break;
      case InputStream::WAIT_FOR_COMPLETION:
        source_stream->RegisterCompletionCallback(base::BindOnce(
            &DownloadFileImpl::OnStreamCompleted, weak_factory_.GetWeakPtr()));
        break;
      case InputStream::COMPLETE:
        break;
      default:
        NOTREACHED();
        break;
    }
    now = base::TimeTicks::Now();
  } while (state == InputStream::HAS_DATA &&
           reason == DOWNLOAD_INTERRUPT_REASON_NONE && now - start <= delta &&
           !should_terminate);

  // If we're stopping to yield the thread, post a task so we come back.
  if (state == InputStream::HAS_DATA && now - start > delta &&
      !should_terminate) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&DownloadFileImpl::StreamActive,
                                  weak_factory_.GetWeakPtr(), source_stream,
                                  MOJO_RESULT_OK));
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

      SetPotentialFileLength(source_stream->offset() +
                             source_stream->bytes_written());
    }
    num_active_streams_--;

    // Inform observers.
    SendUpdate();

    // All the stream reader are completed, shut down file IO processing.
    if (IsDownloadCompleted()) {
      RecordFileBandwidth(bytes_seen_,
                          base::TimeTicks::Now() - download_start_);
      if (record_stream_bandwidth_) {
        RecordParallelizableDownloadStats(
            bytes_seen_with_parallel_streams_,
            download_time_with_parallel_streams_,
            bytes_seen_without_parallel_streams_,
            download_time_without_parallel_streams_, IsSparseFile());
      }
      weak_factory_.InvalidateWeakPtrs();
      std::unique_ptr<crypto::SecureHash> hash_state = file_.Finish();
      update_timer_.reset();
      main_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&DownloadDestinationObserver::DestinationCompleted,
                         observer_, TotalBytesReceived(),
                         std::move(hash_state)));
    }
  }
}

void DownloadFileImpl::RegisterAndActivateStream(SourceStream* source_stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  source_stream->Initialize();
  source_stream->RegisterDataReadyCallback(
      base::Bind(&DownloadFileImpl::StreamActive, weak_factory_.GetWeakPtr(),
                 source_stream));
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
    update_timer_->Start(FROM_HERE,
                         base::TimeDelta::FromMilliseconds(kUpdatePeriodMs),
                         this, &DownloadFileImpl::SendUpdate);
  }
  rate_estimator_.Increment(data_len);
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta time_elapsed = (now - last_update_time_);
  last_update_time_ = now;
  if (num_active_streams_ > 1) {
    download_time_with_parallel_streams_ += time_elapsed;
    bytes_seen_with_parallel_streams_ += data_len;
  } else {
    download_time_without_parallel_streams_ += time_elapsed;
    bytes_seen_without_parallel_streams_ += data_len;
  }
}

void DownloadFileImpl::AddNewSlice(int64_t offset, int64_t length) {
  size_t index = AddOrMergeReceivedSliceIntoSortedArray(
      DownloadItem::ReceivedSlice(offset, length), received_slices_);
  // Check if the slice is added as a new slice, or merged with an existing one.
  bool slice_added = (offset == received_slices_[index].offset);
  // Update the index of exising SourceStreams.
  for (auto& stream : source_streams_) {
    SourceStream* source_stream = stream.second.get();
    if (source_stream->offset() > offset) {
      if (slice_added && source_stream->bytes_written() > 0)
        source_stream->set_index(source_stream->index() + 1);
    } else if (source_stream->offset() == offset) {
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

  // If previous stream has already written data at the starting offset of
  // the error stream. The download can complete.
  bool can_recover_from_error = (source_stream->length() == kNoBytesToWrite);

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

  SendUpdate();  // Make info up to date before error.

  if (!can_recover_from_error) {
    // Error case for both upstream source and file write.
    // Shut down processing and signal an error to our observer.
    // Our observer will clean us up.
    weak_factory_.InvalidateWeakPtrs();
    std::unique_ptr<crypto::SecureHash> hash_state = file_.Finish();
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&DownloadDestinationObserver::DestinationError,
                       observer_, reason, TotalBytesReceived(),
                       std::move(hash_state)));
  }
}

bool DownloadFileImpl::IsSparseFile() const {
  return source_streams_.size() > 1 || !received_slices_.empty();
}

DownloadFileImpl::SourceStream* DownloadFileImpl::FindPrecedingNeighbor(
    SourceStream* source_stream) {
  int64_t max_preceding_offset = 0;
  SourceStream* ret = nullptr;
  for (auto& stream : source_streams_) {
    int64_t offset = stream.second->offset();
    if (offset < source_stream->offset() && offset >= max_preceding_offset) {
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
             << " , bytes_written = " << stream.second->bytes_written()
             << " , is_finished = " << stream.second->is_finished()
             << " , length = " << stream.second->length()
             << ", index = " << stream.second->index();
  }

  DebugSlicesInfo(received_slices_);
}

DownloadFileImpl::RenameParameters::RenameParameters(
    RenameOption option,
    const base::FilePath& new_path,
    const RenameCompletionCallback& completion_callback)
    : option(option),
      new_path(new_path),
      retries_left(kMaxRenameRetries),
      completion_callback(completion_callback) {}

DownloadFileImpl::RenameParameters::~RenameParameters() {}

}  // namespace download
