// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <memory>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/net/pipe_handler.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"

#if BUILDFLAG(IS_WIN)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {}

const size_t read_buffer_size = 100 * 1024 * 1024;

PipeHandler::PipeHandler(WebSocketListener* listener, int write_fd, int read_fd)
    : listener_(listener),
      write_fd_(write_fd),
      read_fd_(read_fd),
      write_buffer_(base::MakeRefCounted<net::DrainableIOBuffer>(
          base::MakeRefCounted<net::IOBuffer>(0),
          0)),
      read_buffer_(base::MakeRefCounted<net::DrainableIOBuffer>(
          base::MakeRefCounted<net::IOBuffer>(read_buffer_size),
          0)) {}

PipeHandler::~PipeHandler() = default;

bool PipeHandler::Send(const std::string& message) {
  VLOG(4) << "PipeHandler::Send " << message;

  pending_write_.append(message);
  pending_write_.append("\0", 1);

  if (!write_buffer_->BytesRemaining()) {
    WriteIntoPipe();
  }
  return true;
}

void PipeHandler::WriteIntoPipe() {
  if (!write_buffer_->BytesRemaining()) {
    if (pending_write_.empty())
      return;

    write_buffer_ = base::MakeRefCounted<net::DrainableIOBuffer>(
        base::MakeRefCounted<net::StringIOBuffer>(pending_write_),
        pending_write_.length());
    pending_write_.clear();
  }

  while (write_buffer_->BytesRemaining()) {
    auto bytes_written = write(write_fd_, write_buffer_->data(),
                               write_buffer_->BytesRemaining());

    if (bytes_written == -1) {
      LOG(ERROR) << "Connection closed, not able to write into pipe";
      Close();
      return;
    }

    write_buffer_->DidConsume(bytes_written);
  }
}

void PipeHandler::Read() {
  while (true) {
    auto bytes_read = read(read_fd_, &read_buffer_, read_buffer_size);
    if (read_buffer_->BytesRemaining() == 0) {
      LOG(ERROR) << "Connection closed, not enough capacity";
      Close();
      break;
    }
    if (!bytes_read)
      break;

    // Go over the last read chunk, look for \0, extract messages.
    int offset = 0;
    for (int i = read_buffer_->size() - bytes_read; i < read_buffer_->size();
         ++i) {
      if (read_buffer_->data()[i] == '\0') {
        listener_->OnMessageReceived(std::string(read_buffer_->data() + offset,
                                                 read_buffer_->data() + i));
        offset = i + 1;
      }
    }
    if (offset)
      read_buffer_->DidConsume(offset);
  }
}

void PipeHandler::Close() {
  close(write_fd_);
  close(read_fd_);
  listener_->OnClose();
}
