// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_ADB_CLIENT_SOCKET_H_
#define CHROME_TEST_CHROMEDRIVER_NET_ADB_CLIENT_SOCKET_H_

#include "base/callback.h"
#include "base/macros.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/socket/stream_socket.h"

class AdbClientSocket {
 public:
  typedef base::Callback<void(int, const std::string&)> CommandCallback;
  typedef base::Callback<void(int result,
                              net::StreamSocket*)> SocketCallback;
  typedef base::Callback<void(const std::string&)> ParserCallback;

  static void AdbQuery(int port,
                       const std::string& query,
                       const CommandCallback& callback);

  static void TransportQuery(int port,
                             const std::string& serial,
                             const std::string& socket_name,
                             const SocketCallback& callback);

  static void HttpQuery(int port,
                        const std::string& serial,
                        const std::string& socket_name,
                        const std::string& request,
                        const CommandCallback& callback);

  static void HttpQuery(int port,
                        const std::string& serial,
                        const std::string& socket_name,
                        const std::string& request,
                        const SocketCallback& callback);

  static void SendFile(int port,
                       const std::string& serial,
                       const std::string& filename,
                       const std::string& content,
                       const CommandCallback& callback);

  explicit AdbClientSocket(int port);
  ~AdbClientSocket();

 protected:
  void Connect(net::CompletionOnceCallback callback);

  void SendCommand(const std::string& command,
                   bool has_output,
                   bool has_length,
                   const CommandCallback& response_callback);

  std::unique_ptr<net::StreamSocket> socket_;

  void ReadResponse(const CommandCallback& callback,
                    bool has_output,
                    bool has_length,
                    int result);

 private:
  static void ReadStatusOutput(const CommandCallback& response_callback,
                               scoped_refptr<net::IOBuffer> socket_buffer,
                               int socket_result);

  void ReadUntilEOF(const ParserCallback& parse_output_callback,
                    const CommandCallback& response_callback,
                    scoped_refptr<net::GrowableIOBuffer> socket_buffer,
                    int socket_result);

  static void ParseOutput(bool has_length,
                          const CommandCallback& response_callback,
                          const std::string& adb_output);

  int port_;

  friend class AdbClientSocketTest;
  DISALLOW_COPY_AND_ASSIGN(AdbClientSocket);
};
#endif  // CHROME_TEST_CHROMEDRIVER_NET_ADB_CLIENT_SOCKET_H_
