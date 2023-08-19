// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_PIPE_WRITER_POSIX_H_
#define CHROME_TEST_CHROMEDRIVER_NET_PIPE_WRITER_POSIX_H_

#include "base//files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/threading/thread_checker.h"
#include "net/base/completion_once_callback.h"

namespace net {
class IOBuffer;
}

// Non-blocking pipe writer implemented on top of POSIX API.
class PipeWriterPosix : public base::MessagePumpForIO::FdWatcher {
 public:
  PipeWriterPosix();

  PipeWriterPosix(const PipeWriterPosix&) = delete;
  PipeWriterPosix& operator=(const PipeWriterPosix&) = delete;

  ~PipeWriterPosix() override;

  // Attach the writer to the file descriptor.
  int Bind(base::ScopedFD fd);

  bool IsConnected() const;

  // Non-blocking write to the underlying pipe.
  // In case if the operation can be accomplished immediately without blocking:
  // * If succeeded the amount of written bytes is returned.
  // * If failed the error code is returned.
  // In case if the operation cannot be accomplished immediately:
  // * net::ERR_IO_PENDING is returned.
  // * The callback is invoked after the asynchronous write.
  // * If succeeded the amount of written bytes is passed to the callback.
  // * If failed the error code is passed to the callback.
  int Write(net::IOBuffer* buf,
            int buf_len,
            net::CompletionOnceCallback callback);

  // Stop watching the file descriptor and close it immediately.
  void Close();

  // Detaches from the current thread, to allow the writer to be transferred to
  // a new thread. Should only be called when the object is no longer used by
  // the old thread.
  void DetachFromThread();

 private:
  // base::MessagePumpForIO::FdWatcher methods.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // Waits for next write event. Returns ERR_IO_PENDING if it starts
  // waiting for write event successfully. Otherwise, returns a net error code.
  // It must not be called after Write() because Write() calls it internally.
  int WaitForWrite(net::IOBuffer* buf,
                   int buf_len,
                   net::CompletionOnceCallback callback);

  int DoWrite(net::IOBuffer* buf, int buf_len);

  THREAD_CHECKER(thread_checker_);

  base::ScopedFD fd_;

  base::MessagePumpForIO::FdWatchController write_fd_watcher_;
  scoped_refptr<net::IOBuffer> write_buf_;
  int write_buf_len_ = 0;
  // External callback; called when write or connect is complete.
  net::CompletionOnceCallback write_callback_;
  bool is_connected_ = false;
};

#endif  // CHROME_TEST_CHROMEDRIVER_NET_PIPE_WRITER_POSIX_H_
