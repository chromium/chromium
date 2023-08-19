// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_PIPE_READER_POSIX_H_
#define CHROME_TEST_CHROMEDRIVER_NET_PIPE_READER_POSIX_H_

#include "base//files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/threading/thread_checker.h"
#include "net/base/completion_once_callback.h"

namespace net {
class IOBuffer;
}

// Non-blocking pipe reader implemented on top of POSIX API.
class PipeReaderPosix : public base::MessagePumpForIO::FdWatcher {
 public:
  PipeReaderPosix();

  PipeReaderPosix(const PipeReaderPosix&) = delete;
  PipeReaderPosix& operator=(const PipeReaderPosix&) = delete;

  ~PipeReaderPosix() override;

  int Bind(base::ScopedFD fd);

  bool IsConnected() const;

  // Non-blocking read from the underlying pipe.
  // In case if the operation can be accomplished immediately without blocking:
  // * If succeeded the amount of read bytes is returned.
  // * If failed the error code is returned.
  // In case if the operation cannot be accomplished immediately:
  // * net::ERR_IO_PENDING is returned.
  // * The callback is invoked after the asynchronous read.
  // * If succeeded the amount of received bytes is passed to the callback.
  // * If failed the error code is passed to the callback.
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback);

  void Close();

  // Detaches from the current thread, to allow the reader to be transferred to
  // a new thread. Should only be called when the object is no longer used by
  // the old thread.
  void DetachFromThread();

 private:
  // base::MessagePumpForIO::FdWatcher methods.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  int ReadIfReady(net::IOBuffer* buf,
                  int buf_len,
                  net::CompletionOnceCallback callback);
  int CancelReadIfReady();

  int DoRead(net::IOBuffer* buf, int buf_len);
  void RetryRead(int rv);

  THREAD_CHECKER(thread_checker_);

  base::ScopedFD fd_;

  base::MessagePumpForIO::FdWatchController read_fd_watcher_;

  // Non-null when a Read() is in progress.
  scoped_refptr<net::IOBuffer> read_buf_;
  int read_buf_len_ = 0;
  net::CompletionOnceCallback read_callback_;

  // Non-null when a ReadIfReady() is in progress.
  net::CompletionOnceCallback read_if_ready_callback_;

  bool is_connected_ = false;
};

#endif  // CHROME_TEST_CHROMEDRIVER_NET_PIPE_READER_POSIX_H_
