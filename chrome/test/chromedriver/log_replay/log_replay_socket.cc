// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/test/chromedriver/log_replay/log_replay_socket.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace {

std::string SessionIdJson(const std::string& session_id) {
  return session_id.empty() ? std::string()
                            : ",\"session_id\":\"" + session_id + "\"";
}

}  // namespace

LogReplaySocket::LogReplaySocket(const base::FilePath& log_path)
    : connected_(false), log_reader_(log_path) {}

LogReplaySocket::~LogReplaySocket() {}

void LogReplaySocket::SetId(const std::string& socket_id) {
  socket_id_ = socket_id;
}

bool LogReplaySocket::IsConnected() {
  return connected_;
}

bool LogReplaySocket::Connect(const GURL& url) {
  connected_ = true;
  return true;
}

bool LogReplaySocket::Send(const std::string& message) {
  std::optional<base::Value> json = base::JSONReader::Read(message);
  max_id_ = json->GetDict().FindInt("id").value();
  return true;
}

std::unique_ptr<LogEntry> LogReplaySocket::GetNextSocketEntry(
    bool include_requests) {
  while (true) {
    std::unique_ptr<LogEntry> next = log_reader_.GetNext(LogEntry::kWebSocket);
    if (next == nullptr)
      return nullptr;
    // it's a request (and |include_requests| is false)
    if (!include_requests && next->event_type == LogEntry::kRequest)
      continue;
    return next;
  }
}

SyncWebSocket::StatusCode LogReplaySocket::ReceiveNextMessage(
    std::string* message,
    const Timeout& timeout) {
  if (socket_id_ == "") {
    return SyncWebSocket::StatusCode::kDisconnected;
  }
  std::unique_ptr<LogEntry> next = GetNextSocketEntry(false);
  if (next->event_type == LogEntry::kResponse) {
    // We have to build the messages back up to what they would have been
    // in the actual WebSocket.
    *message = "{\"id\":" + base::NumberToString(next->id) +
               SessionIdJson(next->session_id) +
               ",\"result\":" + next->payload + "}";
    return SyncWebSocket::StatusCode::kOk;
  }
  // it's an event
  *message = "{\"method\":\"" + next->command_name +
             SessionIdJson(next->session_id) +
             "\",\"params\":" + next->payload + "}";
  return SyncWebSocket::StatusCode::kOk;
}

// Ensures that we are not getting ahead of Chromedriver.
//
// This means not returning events or responses before ChromeDriver has taken
// the action that triggers them.
//
// There is a rare case where the following order of events occurs in the log:
// client-side command
// WebSocket Command (id=X) (resulting from client-side command)
// WebSocket Event
// WebSocket Response (id=X)
// To ensure that we don't fire the second event before the client-side
// command is called (thus probably causing an error), we must block when we
// see the WebSocket Command until that id is sent through the Send method.
// Such a WebSocket Command will always occur after a client-side command.
// If the event occurs between the client-side command and the WebSocket
// Command, it will be fine to fire at that time because ChromeDriver hasn't
// taken any action on the client-side command yet.
bool LogReplaySocket::HasNextMessage() {
  // "Peek" the next entry
  std::unique_ptr<LogEntry> next = GetNextSocketEntry(true);
  if (next == nullptr)
    return false;
  if (next->event_type == LogEntry::kEvent) {
    log_reader_.UndoGetNext(std::move(next));
    return true;
  }
  bool have_message = next->id <= max_id_;
  log_reader_.UndoGetNext(std::move(next));
  return have_message;
}
