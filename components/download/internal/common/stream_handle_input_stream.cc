// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/stream_handle_input_stream.h"

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "components/download/public/common/download_interrupt_reasons_utils.h"
#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/download_utils.h"
#include "mojo/public/c/system/types.h"

namespace download {

StreamHandleInputStream::StreamHandleInputStream(
    mojom::DownloadStreamHandlePtr stream_handle)
    : stream_handle_(std::move(stream_handle)),
      is_response_completed_(false),
      completion_status_(DOWNLOAD_INTERRUPT_REASON_NONE) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

StreamHandleInputStream::~StreamHandleInputStream() = default;

void StreamHandleInputStream::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receiver_ = std::make_unique<mojo::Receiver<mojom::DownloadStreamClient>>(
      this, std::move(stream_handle_->client_receiver));
  receiver_->set_disconnect_handler(base::BindOnce(
      &StreamHandleInputStream::OnStreamCompleted, base::Unretained(this),
      mojom::NetworkRequestStatus::USER_CANCELED));
  handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC);
}

bool StreamHandleInputStream::IsEmpty() {
  return !stream_handle_;
}

void StreamHandleInputStream::RegisterDataReadyCallback(
    const mojo::SimpleWatcher::ReadyCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (handle_watcher_) {
    if (handle_watcher_->IsWatching())
      ClearDataReadyCallback();
    handle_watcher_->Watch(stream_handle_->stream.get(),
                           MOJO_HANDLE_SIGNAL_READABLE, callback);
  }
}

void StreamHandleInputStream::ClearDataReadyCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (handle_watcher_)
    handle_watcher_->Cancel();
}

void StreamHandleInputStream::RegisterCompletionCallback(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  completion_callback_ = std::move(callback);
}

InputStream::StreamState StreamHandleInputStream::Read(
    scoped_refptr<net::IOBuffer>* data,
    size_t* length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!handle_watcher_)
    return InputStream::EMPTY;

  static size_t bytes_to_read = GetDownloadFileBufferSize();
  *data = base::MakeRefCounted<net::IOBufferWithSize>(bytes_to_read);
  MojoResult mojo_result = stream_handle_->stream->ReadData(
      MOJO_READ_DATA_FLAG_NONE, (*data)->span(), *length);
  // TODO(qinmin): figure out when COMPLETE should be returned.
  switch (mojo_result) {
    case MOJO_RESULT_OK:
      return InputStream::HAS_DATA;
    case MOJO_RESULT_SHOULD_WAIT:
      return InputStream::EMPTY;
    case MOJO_RESULT_FAILED_PRECONDITION:
      if (is_response_completed_)
        return InputStream::COMPLETE;
      stream_handle_->stream.reset();
      ClearDataReadyCallback();
      return InputStream::WAIT_FOR_COMPLETION;
    case MOJO_RESULT_INVALID_ARGUMENT:
    case MOJO_RESULT_OUT_OF_RANGE:
    case MOJO_RESULT_BUSY:
      RecordInputStreamReadError(mojo_result);
      return InputStream::COMPLETE;
  }
  return InputStream::EMPTY;
}

DownloadInterruptReason StreamHandleInputStream::GetCompletionStatus() {
  return completion_status_;
}

void StreamHandleInputStream::OnStreamCompleted(
    mojom::NetworkRequestStatus status) {
  // This method could get called again when the URLLoader is being destroyed.
  // However, if the response is already completed, don't set the
  // |completion_status_| again.
  if (is_response_completed_)
    return;
  // This can be called before or after data pipe is completely drained.
  completion_status_ = ConvertMojoNetworkRequestStatusToInterruptReason(status);
  is_response_completed_ = true;
  if (completion_callback_)
    std::move(completion_callback_).Run();
}

}  // namespace download
