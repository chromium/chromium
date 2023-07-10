// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_PIPE_CONNECTION_WIN_H_
#define CHROME_TEST_CHROMEDRIVER_NET_PIPE_CONNECTION_WIN_H_

#include "base/files/platform_file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/threading/thread_checker.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"

class PipeReader;
class PipeWriter;

class PipeConnectionWin : public SyncWebSocket {
 public:
  PipeConnectionWin(base::ScopedPlatformFile read_file,
                    base::ScopedPlatformFile write_file);

  PipeConnectionWin(const PipeConnectionWin&) = delete;
  PipeConnectionWin& operator=(const PipeConnectionWin&) = delete;

  ~PipeConnectionWin() override;

  void SetId(const std::string& socket_id) override {}

  // Return true if both reader and writer are connected.
  // Otherwise return false.
  bool IsConnected() override;

  // Initialize the pipe connection.
  // This method has to be called once before sending or receiving data.
  // Reconnect is not supported.
  bool Connect(const GURL& url) override;

  bool Send(const std::string& message) override;

  StatusCode ReceiveNextMessage(std::string* message,
                                const Timeout& timeout) override;

  bool HasNextMessage() override;

  void SetNotificationCallback(base::RepeatingClosure callback) override;

  // True if there is no underlying pipe_reader_ and pipe_writer_
  bool IsNull() const;

 private:
  friend class PipeReader;
  friend class PipeWriter;
  void Shutdown();
  void SendNotification();

  base::ScopedPlatformFile read_file_;
  base::ScopedPlatformFile write_file_;
  base::RepeatingClosure notify_;
  bool connection_requested_ = false;
  bool shutting_down_ = false;
  std::unique_ptr<PipeReader> pipe_reader_;
  std::unique_ptr<PipeWriter> pipe_writer_;
  base::WeakPtrFactory<PipeConnectionWin> weak_factory_{this};
};

#endif  // CHROME_TEST_CHROMEDRIVER_NET_PIPE_CONNECTION_WIN_H_
