// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/cronet_upload_data_stream.h"

#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace cronet {

CronetUploadDataStream::CronetUploadDataStream(Delegate* delegate, int64_t size)
    : UploadDataStream(size < 0, 0),
      size_(size),
      waiting_on_read_(false),
      read_in_progress_(false),
      waiting_on_rewind_(false),
      rewind_in_progress_(false),
      at_front_of_stream_(true),
      delegate_(delegate) {}

CronetUploadDataStream::~CronetUploadDataStream() {
  delegate_->OnUploadDataStreamDestroyed();
}

int CronetUploadDataStream::InitInternal(const net::NetLogWithSource& net_log) {
  // ResetInternal should have been called before init, if the stream was in
  // use.
  DCHECK(!waiting_on_read_);
  DCHECK(!waiting_on_rewind_);

  if (!weak_factory_.HasWeakPtrs())
    delegate_->InitializeOnNetworkThread(weak_factory_.GetWeakPtr());

  // Set size of non-chunked uploads.
  if (size_ >= 0)
    SetSize(static_cast<uint64_t>(size_));

  // If already at the front of the stream, nothing to do.
  if (at_front_of_stream_) {
    // Being at the front of the stream implies there's no read or rewind in
    // progress.
    DCHECK(!read_in_progress_);
    DCHECK(!rewind_in_progress_);
    return net::OK;
  }

  // Otherwise, the request is now waiting for the stream to be rewound.
  waiting_on_rewind_ = true;

  // Start rewinding the stream if no operation is in progress.
  if (!read_in_progress_ && !rewind_in_progress_)
    StartRewind();
  return net::ERR_IO_PENDING;
}

int CronetUploadDataStream::ReadInternal(net::IOBuffer* buf, int buf_len) {
  // All pending operations should have completed before a read can start.
  DCHECK(!waiting_on_read_);
  DCHECK(!read_in_progress_);
  DCHECK(!waiting_on_rewind_);
  DCHECK(!rewind_in_progress_);

  DCHECK(buf);
  DCHECK_GT(buf_len, 0);

  read_in_progress_ = true;
  waiting_on_read_ = true;
  at_front_of_stream_ = false;
  scoped_refptr<net::IOBuffer> buffer(base::WrapRefCounted(buf));
  delegate_->Read(std::move(buffer), buf_len);
  return net::ERR_IO_PENDING;
}

void CronetUploadDataStream::ResetInternal() {
  // Consumer is not waiting on any operation.  Note that the active operation,
  // if any, will continue.
  waiting_on_read_ = false;
  waiting_on_rewind_ = false;
}

void CronetUploadDataStream::OnReadSuccess(int bytes_read, bool final_chunk) {
  DCHECK(read_in_progress_);
  DCHECK(!rewind_in_progress_);
  DCHECK(bytes_read > 0 || (final_chunk && bytes_read == 0));
  if (!is_chunked()) {
    DCHECK(!final_chunk);
  }

  read_in_progress_ = false;

  if (waiting_on_rewind_) {
    DCHECK(!waiting_on_read_);
    // Since a read just completed, can't be at the front of the stream.
    StartRewind();
    return;
  }
  // ResetInternal has been called, but still waiting on InitInternal.
  if (!waiting_on_read_)
    return;

  waiting_on_read_ = false;
  if (final_chunk)
    SetIsFinalChunk();
  OnReadCompleted(bytes_read);
}

void CronetUploadDataStream::OnRewindSuccess() {
  DCHECK(!waiting_on_read_);
  DCHECK(!read_in_progress_);
  DCHECK(rewind_in_progress_);
  DCHECK(!at_front_of_stream_);

  rewind_in_progress_ = false;
  at_front_of_stream_ = true;

  // Possible that ResetInternal was called since the rewind was started, but
  // InitInternal has not been.
  if (!waiting_on_rewind_)
    return;

  waiting_on_rewind_ = false;
  OnInitCompleted(net::OK);
}

void CronetUploadDataStream::StartRewind() {
  DCHECK(!waiting_on_read_);
  DCHECK(!read_in_progress_);
  DCHECK(waiting_on_rewind_);
  DCHECK(!rewind_in_progress_);
  DCHECK(!at_front_of_stream_);

  rewind_in_progress_ = true;
  delegate_->Rewind();
}

}  // namespace cronet
