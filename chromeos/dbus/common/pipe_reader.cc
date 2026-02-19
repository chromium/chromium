// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/common/pipe_reader.h"

#include <utility>

#include "base/byte_size.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/task_runner.h"
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
  auto rv = RequestRead();
  if (rv.has_value() || rv.error() != net::ERR_IO_PENDING) {
    LOG(ERROR) << "Unable to post initial read";
    data_stream_.reset();
    return base::ScopedFD();
  }

  // The operation is successfully started. Keep objects in the members,
  // and returns the write-side of the pipe.
  callback_ = std::move(callback);
  return pipe_write_end;
}

base::expected<base::ByteSize, net::Error> PipeReader::RequestRead() {
  DCHECK(data_stream_.get());
  return data_stream_->Read(
      io_buffer_.get(), io_buffer_->size(),
      base::BindOnce(&PipeReader::OnRead, weak_ptr_factory_.GetWeakPtr()));
}

void PipeReader::OnRead(base::expected<base::ByteSize, net::Error> result) {
  if (!result.has_value()) {
    // The error variant should never contain net::OK; EOF is represented as
    // a success result with zero bytes.
    CHECK_NE(result.error(), net::OK);
    DVLOG(1) << "OnRead error: " << net::ErrorToString(result.error());
    data_.clear();
    data_stream_.reset();
    std::move(callback_).Run(std::nullopt);
    return;
  }

  DVLOG(1) << "OnRead byte_count: " << *result;
  if (result->is_zero()) {
    // EOF - return collected data.
    std::string data = std::move(data_);
    data_.clear();
    data_stream_.reset();
    std::move(callback_).Run(std::move(data));
    return;
  }

  data_.append(io_buffer_->data(),
               base::checked_cast<size_t>(result->InBytes()));

  // Post another read.
  auto read_result = RequestRead();
  if (read_result.has_value() || read_result.error() != net::ERR_IO_PENDING) {
    // TODO(hjanuschka): crbug.com/485271327 - Fix synchronous read to append
    // data instead of discarding. Preserving old behavior for now.
    OnRead((read_result.has_value() && read_result->is_positive())
               ? base::unexpected(net::ERR_FAILED)
               : std::move(read_result));
  }
}

}  // namespace chromeos
