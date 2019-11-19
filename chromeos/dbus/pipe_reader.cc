// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/pipe_reader.h"

#include <utility>

#include "base/bind.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task_runner.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace chromeos {

PipeReader::PipeReader(const scoped_refptr<base::TaskRunner>& task_runner)
    : io_buffer_(base::MakeRefCounted<net::IOBufferWithSize>(4096)),
      task_runner_(task_runner) {}

PipeReader::~PipeReader() = default;

base::ScopedFD PipeReader::StartIO(CompletionCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(data_.empty());

  if (!callback_.is_null()) {
    LOG(ERROR) << "There already is in-flight operation.";
    return base::ScopedFD();
  }

  // Use a pipe to collect data
  int pipe_fds[2];
  const int status = HANDLE_EINTR(pipe(pipe_fds));
  if (status < 0) {
    PLOG(ERROR) << "pipe";
    return base::ScopedFD();
  }

  base::ScopedFD pipe_write_end(pipe_fds[1]);
  // Pass ownership of pipe_fds[0] to data_stream_, which will close it.
  data_stream_ =
      std::make_unique<net::FileStream>(base::File(pipe_fds[0]), task_runner_);

  // Post an initial async read to setup data collection
  // Expect asynchronous operation.
  int rv = RequestRead();
  if (rv != net::ERR_IO_PENDING) {
    LOG(ERROR) << "Unable to post initial read";
    data_stream_.reset();
    return base::ScopedFD();
  }

  // The operation is successfully started. Keep objects in the members,
  // and returns the write-side of the pipe.
  callback_ = std::move(callback);
  return pipe_write_end;
}

int PipeReader::RequestRead() {
  DCHECK(data_stream_.get());
  return data_stream_->Read(
      io_buffer_.get(), io_buffer_->size(),
      base::BindOnce(&PipeReader::OnRead, weak_ptr_factory_.GetWeakPtr()));
}

void PipeReader::OnRead(int byte_count) {
  DVLOG(1) << "OnRead byte_count: " << byte_count;
  if (byte_count <= 0) {
    // On EOF (= 0), or on error (< 0).
    base::Optional<std::string> result =
        byte_count < 0 ? base::nullopt : base::make_optional(std::move(data_));
    // Clear members before calling the |callback|.
    data_.clear();
    data_stream_.reset();
    std::move(callback_).Run(std::move(result));
    return;
  }

  data_.append(io_buffer_->data(), byte_count);

  // Post another read.
  int rv = RequestRead();
  if (rv != net::ERR_IO_PENDING) {
    // Calls OnRead() again with the error, which handles remaining clean up.
    OnRead(rv > 0 ? net::ERR_FAILED : rv);
  }
}

}  // namespace chromeos
