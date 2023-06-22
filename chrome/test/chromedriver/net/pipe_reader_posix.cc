// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/pipe_reader_posix.h"

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

PipeReaderPosix::PipeReaderPosix()
    : fd_(base::kInvalidPlatformFile), read_fd_watcher_(FROM_HERE) {}

PipeReaderPosix::~PipeReaderPosix() {
  Close();
}

int PipeReaderPosix::Bind(base::ScopedFD fd) {
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

bool PipeReaderPosix::IsConnected() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return is_connected_;
}

int PipeReaderPosix::Read(net::IOBuffer* buf,
                          int buf_len,
                          net::CompletionOnceCallback callback) {
  // Use base::Unretained() is safe here because OnFileCanReadWithoutBlocking()
  // won't be called if |this| is gone.
  int rv = ReadIfReady(
      buf, buf_len,
      base::BindOnce(&PipeReaderPosix::RetryRead, base::Unretained(this)));
  if (rv == net::ERR_IO_PENDING) {
    read_buf_ = buf;
    read_buf_len_ = buf_len;
    read_callback_ = std::move(callback);
  }
  is_connected_ = (rv == net::ERR_IO_PENDING) || (rv > 0);
  return rv;
}

int PipeReaderPosix::ReadIfReady(net::IOBuffer* buf,
                                 int buf_len,
                                 net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(fd_.is_valid());
  CHECK(read_if_ready_callback_.is_null());
  DCHECK(!callback.is_null());
  DCHECK_LT(0, buf_len);

  int rv = DoRead(buf, buf_len);
  if (rv != net::ERR_IO_PENDING) {
    return rv;
  }

  if (!base::CurrentIOThread::Get()->WatchFileDescriptor(
          fd_.get(), true, base::MessagePumpForIO::WATCH_READ,
          &read_fd_watcher_, this)) {
    PLOG(ERROR) << "WatchFileDescriptor failed on read";
    return net::MapSystemError(errno);
  }

  read_if_ready_callback_ = std::move(callback);
  return net::ERR_IO_PENDING;
}

int PipeReaderPosix::CancelReadIfReady() {
  DCHECK(read_if_ready_callback_);

  bool ok = read_fd_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);

  read_if_ready_callback_.Reset();
  return net::OK;
}

void PipeReaderPosix::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_connected_ = false;

  bool ok = read_fd_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);

  // These needs to be done after the StopWatchingFileDescriptor() calls, but
  // before deleting the write buffer.
  fd_.reset();

  if (!read_callback_.is_null()) {
    read_buf_.reset();
    read_buf_len_ = 0;
    read_callback_.Reset();
  }

  read_if_ready_callback_.Reset();
}

void PipeReaderPosix::DetachFromThread() {
  DETACH_FROM_THREAD(thread_checker_);
}

void PipeReaderPosix::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK(read_if_ready_callback_);

  bool ok = read_fd_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  std::move(read_if_ready_callback_).Run(net::OK);
}

void PipeReaderPosix::OnFileCanWriteWithoutBlocking(int fd) {}

int PipeReaderPosix::DoRead(net::IOBuffer* buf, int buf_len) {
  int rv = HANDLE_EINTR(read(fd_.get(), buf->data(), buf_len));
  if (rv == 0) {
    return net::ERR_CONNECTION_CLOSED;
  }
  return rv > 0 ? rv : net::MapSystemError(errno);
}

void PipeReaderPosix::RetryRead(int rv) {
  DCHECK(read_callback_);
  DCHECK(read_buf_);
  DCHECK_LT(0, read_buf_len_);

  if (rv == net::OK) {
    rv = ReadIfReady(
        read_buf_.get(), read_buf_len_,
        base::BindOnce(&PipeReaderPosix::RetryRead, base::Unretained(this)));
    if (rv == net::ERR_IO_PENDING) {
      return;
    }
  }
  is_connected_ = (rv == net::ERR_IO_PENDING) || (rv > 0);
  read_buf_ = nullptr;
  read_buf_len_ = 0;
  std::move(read_callback_).Run(rv);
}
