// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_WEBSOCKET_H_
#define CHROME_TEST_CHROMEDRIVER_NET_WEBSOCKET_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "net/base/completion_once_callback.h"
#include "net/socket/tcp_client_socket.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_frame_parser.h"
#include "url/gurl.h"

namespace net {
class DrainableIOBuffer;
class IOBufferWithSize;
}

class WebSocketListener;

// A text-only, non-thread safe WebSocket. Must be created and used on a single
// thread. Intended particularly for use with net::HttpServer.
class WebSocket {
 public:
  // |url| must be an IP v4/v6 literal, not a host name.
  WebSocket(const GURL& url,
            WebSocketListener* listener,
            size_t read_buffer_size = 4096);
  virtual ~WebSocket();

  // Initializes the WebSocket connection. Invokes the given callback with
  // a net::Error. May only be called once.
  void Connect(net::CompletionOnceCallback callback);

  // Sends the given message and returns true on success.
  bool Send(const std::string& message);

 private:
  enum State {
    INITIALIZED,
    CONNECTING,
    OPEN,
    CLOSED
  };

  void OnSocketConnect(int code);

  void Write(const std::string& data);
  void OnWrite(int code);
  void ContinueWritingIfNecessary();

  void Read();
  void OnRead(bool read_again, int code);
  void OnReadDuringHandshake(const char* data, int len);
  void OnReadDuringOpen(const char* data, int len);

  void InvokeConnectCallback(int code);
  void Close(int code);

  base::ThreadChecker thread_checker_;

  GURL url_;
  WebSocketListener* listener_;
  State state_;
  std::unique_ptr<net::TCPClientSocket> socket_;

  net::CompletionOnceCallback connect_callback_;
  std::string sec_key_;
  std::string handshake_response_;

  scoped_refptr<net::DrainableIOBuffer> write_buffer_;
  std::string pending_write_;

  scoped_refptr<net::IOBufferWithSize> read_buffer_;
  net::WebSocketFrameParser parser_;
  net::WebSocketMaskingKey current_masking_key_ = {};
  bool is_current_frame_masked_ = false;
  uint64_t current_frame_offset_ = 0;
  std::string next_message_;

  DISALLOW_COPY_AND_ASSIGN(WebSocket);
};

// Listens for WebSocket messages and disconnects on the same thread as the
// WebSocket.
class WebSocketListener {
 public:
  virtual ~WebSocketListener() {}

  // Called when a WebSocket message is received.
  virtual void OnMessageReceived(const std::string& message) = 0;

  // Called when the WebSocket connection closes. Will be called at most once.
  // Will not be called if the connection was never established or if the close
  // was initiated by the client.
  virtual void OnClose() = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_NET_WEBSOCKET_H_
