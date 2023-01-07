// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_PIPE_HANDLER_H_
#define CHROME_TEST_CHROMEDRIVER_NET_PIPE_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "chrome/test/chromedriver/net/websocket.h"

namespace net {
class DrainableIOBuffer;
}

class WebSocketListener;

class PipeHandler {
 public:
  PipeHandler(WebSocketListener* listener, int write_fd, int read_fd);

  PipeHandler(const PipeHandler&) = delete;
  PipeHandler& operator=(const PipeHandler&) = delete;

  virtual ~PipeHandler();

  // Sends the given message and returns true on success.
  bool Send(const std::string& message);

 private:
  void WriteIntoPipe();

  void Read();
  void Close();

  raw_ptr<WebSocketListener> listener_;
  int write_fd_;
  int read_fd_;

  scoped_refptr<net::DrainableIOBuffer> write_buffer_;
  scoped_refptr<net::DrainableIOBuffer> read_buffer_;
  std::string pending_write_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_NET_PIPE_HANDLER_H_
