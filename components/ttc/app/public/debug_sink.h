// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TTC_APP_PUBLIC_DEBUG_SINK_H_
#define COMPONENTS_TTC_APP_PUBLIC_DEBUG_SINK_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <variant>
#include <vector>

#include "components/ttc/app/public/server_journal_event.h"

namespace ttc {

// Typed instrumentation of a TTC session from end to end: audio capture, what
// goes to and comes back from the server (including tool calls), and playback.
// Consumed by the developer debug tools. All methods are called on the main
// sequence and must be cheap: some are called for every audio chunk. Only
// provided while the debug tools are enabled; callers hold a nullable pointer
// and skip reporting otherwise.
class DebugSink {
 public:
  // The frames the client sends, one type per kind of frame (ClientFrame).
  struct SetupRequest {};
  // `pcm` is exactly what was put into the frame.
  struct AudioInput {
    std::vector<int16_t> pcm;
  };
  struct TextInput {
    std::string text;
  };
  // Page context: the active tab's URL, and the size of its content.
  struct ContextUpdate {
    std::string url;
    size_t content_bytes = 0;
  };
  // The tools offered to the server.
  struct ToolsetUpdate {
    std::vector<std::string> tool_names;
  };
  // The response to the server's ToolCall with the same `call_id`.
  struct ToolCallResponse {
    std::string call_id;
    std::string response_json;
  };
  struct PlaybackStatus {
    int64_t last_played_sequence_number = 0;
  };
  struct CloseRequest {
    std::string reason;
  };
  using ClientFrame = std::variant<SetupRequest,
                                   AudioInput,
                                   TextInput,
                                   ContextUpdate,
                                   ToolsetUpdate,
                                   ToolCallResponse,
                                   PlaybackStatus,
                                   CloseRequest>;

  // The frames the server sends, one type per kind of frame (ServerFrame).
  // The server's SessionStatusResponse.
  struct SessionStatus {
    std::string server_session_id;
    std::string state;
    // Explains a rejection.
    std::string diagnostic;
  };
  // Audio exactly as the server sent it, before playback.
  struct AudioOutput {
    int64_t sequence_number = 0;
    std::vector<int16_t> pcm;
  };
  // A transcript of the user's speech (`is_input`) or of the reply.
  struct Transcript {
    bool is_input = false;
    std::string text;
  };
  struct TurnComplete {};
  struct Interrupted {};
  // `call_id` is the server's identifier for the call.
  struct ToolCall {
    std::string call_id;
    std::string name;
    std::string arguments_json;
  };
  // Either a ServerErrorNotification, with the server's code and message, or a
  // server response that couldn't be used (an execution error, an unparsable
  // frame or malformed audio). `code` is then the name of the problem, e.g.
  // "UnparsableServerFrame".
  struct ServerError {
    std::string code;
    std::string message;
  };
  // The server asked the client to disconnect within `time_left_ms`.
  struct GoAway {
    int64_t time_left_ms = 0;
  };
  // A frame of a type this client doesn't know.
  struct UnknownFrame {};
  using ServerFrame = std::variant<SessionStatus,
                                   AudioOutput,
                                   Transcript,
                                   TurnComplete,
                                   Interrupted,
                                   ToolCall,
                                   ServerError,
                                   GoAway,
                                   ServerJournalEvent,
                                   UnknownFrame>;

  virtual ~DebugSink() = default;

  // From the client to the server, and the connection between them.
  virtual void OnClientFrame(const ClientFrame& frame) {}
  // A frame couldn't be sent because there is no session, e.g. a tool that
  // finished after the session ended.
  virtual void OnFrameDropped() {}
  virtual void OnTransportStateChanged(bool connected) {}

  // From the server to the client.
  virtual void OnServerFrame(const ServerFrame& frame) {}
};

}  // namespace ttc

#endif  // COMPONENTS_TTC_APP_PUBLIC_DEBUG_SINK_H_
