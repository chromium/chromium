// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/pipe_writer_posix.h"

#include <errno.h>

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/current_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

#if BUILDFLAG(IS_FUCHSIA)
#include <poll.h>
#include <sys/ioctl.h>
#endif  // BUILDFLAG(IS_FUCHSIA)

PipeWriterPosix::PipeWriterPosix()
    : fd_(base::kInvalidPlatformFile), write_fd_watcher_(FROM_HERE) {}

PipeWriterPosix::~PipeWriterPosix() {
  Close();
}

int PipeWriterPosix::Bind(base::ScopedFD fd) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!fd_.is_valid());

  fd_ = std::move(fd);

  if (!base::SetNonBlocking(fd_.get())) {
    int rv = net::MapSystemError(errno);
    Close();
    return rv;
  }
  is_connected_ = true;
  return net::OK;
}

bool PipeWriterPosix::IsConnected() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return is_connected_;
}

int PipeWriterPosix::Write(net::IOBuffer* buf,
                           int buf_len,
                           net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(fd_.is_valid());
  CHECK(write_callback_.is_null());
  // Synchronous operation not supported
  CHECK(!callback.is_null());
  CHECK_LT(0, buf_len);

  int rv = DoWrite(buf, buf_len);
  if (rv == net::ERR_IO_PENDING) {
    rv = WaitForWrite(buf, buf_len, std::move(callback));
  }
  is_connected_ = (rv == net::ERR_IO_PENDING) || (rv > 0);
  return rv;
}

int PipeWriterPosix::WaitForWrite(net::IOBuffer* buf,
                                  int buf_len,
                                  net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(fd_.is_valid());
  DCHECK(write_callback_.is_null());
  // Synchronous operation not supported
  DCHECK(!callback.is_null());
  DCHECK_LT(0, buf_len);

  if (!base::CurrentIOThread::Get()->WatchFileDescriptor(
          fd_.get(), true, base::MessagePumpForIO::WATCH_WRITE,
          &write_fd_watcher_, this)) {
    PLOG(ERROR) << "WatchFileDescriptor failed on write";
    return net::MapSystemError(errno);
  }

  write_buf_ = buf;
  write_buf_len_ = buf_len;
  write_callback_ = std::move(callback);
  return net::ERR_IO_PENDING;
}

int PipeWriterPosix::DoWrite(net::IOBuffer* buf, int buf_len) {
  int rv = HANDLE_EINTR(write(fd_.get(), buf->data(), buf_len));
  if (rv >= 0) {
    CHECK_LE(rv, buf_len);
  }
  return rv >= 0 ? rv : net::MapSystemError(errno);
}

void PipeWriterPosix::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_connected_ = false;

  bool ok = write_fd_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);

  // These needs to be done after the StopWatchingFileDescriptor() calls, but
  // before deleting the write buffer.
  fd_.reset();

  if (!write_callback_.is_null()) {
    write_buf_.reset();
    write_buf_len_ = 0;
    write_callback_.Reset();
  }
}

void PipeWriterPosix::DetachFromThread() {
  DETACH_FROM_THREAD(thread_checker_);
}

void PipeWriterPosix::OnFileCanReadWithoutBlocking(int fd) {}

void PipeWriterPosix::OnFileCanWriteWithoutBlocking(int fd) {
  DCHECK(!write_callback_.is_null());
  int rv = DoWrite(write_buf_.get(), write_buf_len_);
  if (rv == net::ERR_IO_PENDING) {
    return;
  }
  is_connected_ = (rv == net::ERR_IO_PENDING) || (rv > 0);

  bool ok = write_fd_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  write_buf_.reset();
  write_buf_len_ = 0;
  std::move(write_callback_).Run(rv);
}
