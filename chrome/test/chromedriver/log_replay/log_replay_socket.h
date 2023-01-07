// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_TEST_CHROMEDRIVER_LOG_REPLAY_LOG_REPLAY_SOCKET_H_
#define CHROME_TEST_CHROMEDRIVER_LOG_REPLAY_LOG_REPLAY_SOCKET_H_

#include <string>

#include "chrome/test/chromedriver/log_replay/devtools_log_reader.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"

// A "WebSocket" class for getting commands from a log file.
//
// Instead of communicating with DevTools, this implementation looks up the
// intended results of commands in a ChromeDriver log file. This enables the
// DevTools-side commands and responses to be replayed from a past session.
class LogReplaySocket : public SyncWebSocket {
 public:
  // Initialize a LogReplaySocket with a ChromeDriver log.
  explicit LogReplaySocket(const base::FilePath& log_path);
  ~LogReplaySocket() override;

  // Set the ID of this instance. This is intended to be the id_ of the
  // DevToolsClientImpl instance that owns this LogReplaySocket, which is
  // an identifier for the renderer process that this DevTools instance
  // was talking to. Since ChromeDriver logs often include messages from
  // multiple renderers, this enables a LogReplaySocket to know which
  // of these messages it should observe. Note that the id_ corresponds
  // to a frameID or window ID, so it stays stable between the old and
  // replay sessions (this is in contrast to sessionIDs, which change).
  void SetId(const std::string& socket_id) override;

  // The Connection methods here basically just mock out behavior of a
  // real SyncWebSocket
  bool IsConnected() override;
  bool Connect(const GURL& url) override;

  // "Sends" a message to the "renderer". This doesn't really give us any
  // actions to do (since we would know how to respond even if the message
  // wasn't sent). However, we need to keep track of the sequential id of
  // the latest message because we don't want to return the responses for
  // messages that have not been sent yet.
  bool Send(const std::string& message) override;

  // Return the next message (event or response) in the |message| parameter.
  // timeout is ignored (since the response is either ready or there is
  // some kind of problem with the log).
  StatusCode ReceiveNextMessage(std::string* message,
                                const Timeout& timeout) override;

  // Returns true if we "should" have the next message - that is, the next
  // message in the log either is an event or a response for a message that
  // has been sent.
  bool HasNextMessage() override;

 private:
  // Return the next WebSocket entry in the log. Returns requests only if
  // |include_requests| is true
  std::unique_ptr<LogEntry> GetNextSocketEntry(bool include_requests);
  bool connected_;
  int max_id_;
  DevToolsLogReader log_reader_;
  std::string socket_id_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_LOG_REPLAY_LOG_REPLAY_SOCKET_H_
