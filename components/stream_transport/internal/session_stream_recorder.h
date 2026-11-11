// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STREAM_TRANSPORT_INTERNAL_SESSION_STREAM_RECORDER_H_
#define COMPONENTS_STREAM_TRANSPORT_INTERNAL_SESSION_STREAM_RECORDER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/span.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"
#include "components/stream_transport/internal/proto/transport_messages.pb.h"
#include "components/stream_transport/internal/transport_message_observer.h"
#include "components/stream_transport/public/transport_session_registry.h"

namespace base {
class DictValue;
}  // namespace base

namespace stream_transport {

class TransportSession;

// Metadata for an active or closed transport session.
struct SessionMetadata {
  std::string session_id;
  base::TimeTicks start_time;
  base::Time start_wall_time;
  std::optional<base::TimeTicks> end_time;
  size_t total_downstream_messages = 0;
  size_t total_upstream_messages = 0;
  bool is_active = true;

  // Serializes this metadata for diagnostic dumps. Keep in sync with the
  // fields above.
  base::DictValue ToValue() const;
};

// One recorded message, converted for diagnostic consumers. The proto body is
// already serialized here so that callers do not need a dependency on the
// transport protos.
// LINT.IfChange(SessionEventSnapshot)
struct SessionEventSnapshot {
  SessionEventSnapshot();
  SessionEventSnapshot(const SessionEventSnapshot&);
  SessionEventSnapshot(SessionEventSnapshot&&);
  SessionEventSnapshot& operator=(const SessionEventSnapshot&);
  SessionEventSnapshot& operator=(SessionEventSnapshot&&);
  ~SessionEventSnapshot();

  // Absolute time, derived from the session start. See the note in
  // GetAllSessions() about why the stored value is a TimeTicks.
  base::Time timestamp;
  bool is_downstream = false;
  // One entry for each typed payload the envelope carries.
  std::vector<std::string> payload_types;
  // JSON of the whole envelope.
  std::string message;
  // True when `message` was cut to the byte limit.
  bool message_truncated = false;
  // The HTTP response status code for upstream messages.
  int http_response_code = 0;
  // The delivery or connection status for downstream messages.
  DownstreamDeliveryStatus downstream_status =
      DownstreamDeliveryStatus::kUnknown;
};
// LINT.ThenChange(//chrome/browser/browser_actuator/internals/browser_actuator_internals_mojom_traits.cc)

// One active or closed session.
// LINT.IfChange(SessionSnapshot)
struct SessionSnapshot {
  SessionSnapshot();
  SessionSnapshot(const SessionSnapshot&);
  SessionSnapshot(SessionSnapshot&&);
  SessionSnapshot& operator=(const SessionSnapshot&);
  SessionSnapshot& operator=(SessionSnapshot&&);
  ~SessionSnapshot();

  std::string session_id;
  base::Time start_wall_time;
  // Null while the session is still active.
  std::optional<base::Time> end_wall_time;
  size_t total_downstream_messages = 0;
  size_t total_upstream_messages = 0;
  // The true event count, before `max_events` is applied.
  size_t total_events = 0;
  // The newest `max_events` events, oldest first.
  std::vector<SessionEventSnapshot> events;
};
// LINT.ThenChange(//chrome/browser/browser_actuator/internals/browser_actuator_internals_mojom_traits.cc)

// Records canonical protobuf messages (ActuatorDownstreamMessage and
// ActuatorUpstreamMessage) for a single TransportSession using a bounded
// buffer (`kMaxEntriesPerSession`). Observers can subscribe to receive live
// messages as they occur.
class SessionStreamRecorder {
 public:
  static constexpr size_t kMaxEntriesPerSession = 256;

  // Stored entry pairing timestamp with canonical protobuf message.
  struct Entry {
    base::TimeTicks timestamp;
    std::variant<browser_actuator::ActuatorDownstreamMessage,
                 browser_actuator::ActuatorUpstreamMessage>
        message;
    int http_response_code = 0;
    DownstreamDeliveryStatus downstream_status =
        DownstreamDeliveryStatus::kUnknown;

    // Serializes this entry for diagnostic dumps. The message body is
    // serialized by the generated `ToValue()` from //components/proto_extras,
    // so new proto fields are picked up automatically.
    base::DictValue ToValue() const;
  };

  // Direct proto observer interface for real-time streaming.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDownstreamMessage(
        base::TimeTicks timestamp,
        const browser_actuator::ActuatorDownstreamMessage& message,
        DownstreamDeliveryStatus status);
    virtual void OnUpstreamMessage(
        base::TimeTicks timestamp,
        const browser_actuator::ActuatorUpstreamMessage& message,
        int http_response_code);
  };

  explicit SessionStreamRecorder(std::string_view session_id);
  ~SessionStreamRecorder();

  SessionStreamRecorder(const SessionStreamRecorder&) = delete;
  SessionStreamRecorder& operator=(const SessionStreamRecorder&) = delete;

  // Direct recording methods:
  void RecordDownstreamMessage(
      browser_actuator::ActuatorDownstreamMessage message,
      DownstreamDeliveryStatus status);

  // Records an upstream message. Operates in two phases: initially called with
  // `http_response_code = 0` when the message is dispatched to the transport
  // client, followed by an in-place update with the response status code upon
  // completion.
  void RecordUpstreamMessage(browser_actuator::ActuatorUpstreamMessage message,
                             int http_response_code);

  // Updates the HTTP response code of a previously recorded pending upstream
  // entry matching `client_sequence_number`. Returns true if a matching
  // pending entry was found and updated.
  bool UpdateUpstreamResponseCode(int64_t client_sequence_number,
                                  int http_response_code);

  // Observer management:
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  std::string_view session_id() const;
  const SessionMetadata& metadata() const;
  const base::circular_deque<Entry>& entries() const;

  // Marks the session as closed.
  void MarkSessionClosed();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  const std::string session_id_;
  SessionMetadata metadata_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::circular_deque<Entry> entries_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::ObserverList<Observer> observers_ GUARDED_BY_CONTEXT(sequence_checker_);
};

// Creates a `SessionStreamRecorder` for every transport session and feeds it
// whole message envelopes. It follows the session lifecycle through
// `TransportSessionRegistry::Observer`, so a session is recorded from the
// moment it is created (for example by the first FCM wake-up message), before
// any `TransportHandler` runs. Downstream and upstream envelopes arrive through
// `TransportMessageObserver` in the order the transport handles them.
// Provides export functionality to dump all sessions' history as JSON.
class SessionStreamRecorderFactory : public TransportSessionRegistry::Observer,
                                     public TransportMessageObserver {
 public:
  // Closed sessions kept for dumps. Older ones are dropped first so a stream of
  // short sessions cannot grow memory without bound.
  static constexpr size_t kMaxRetainedSessions = 100;

  SessionStreamRecorderFactory();
  ~SessionStreamRecorderFactory() override;

  SessionStreamRecorderFactory(const SessionStreamRecorderFactory&) = delete;
  SessionStreamRecorderFactory& operator=(const SessionStreamRecorderFactory&) =
      delete;

  // TransportSessionRegistry::Observer implementation:
  void OnSessionRegistered(TransportSession* session) override;
  void OnSessionRemoved(std::string_view session_id) override;

  // TransportMessageObserver implementation:
  void OnDownstreamMessage(
      std::string_view session_id,
      const browser_actuator::ActuatorDownstreamMessage& message,
      DownstreamDeliveryStatus status) override;
  void OnUpstreamMessage(
      std::string_view session_id,
      const browser_actuator::ActuatorUpstreamMessage& message,
      int http_response_code) override;

  // Exports all tracked sessions' metadata and history as a structured
  // dictionary.
  base::DictValue ExportAllSessionsAsValue() const;

  // Exports the tracked sessions matching `session_ids` (or all sessions if
  // `session_ids` is nullopt) as a structured dictionary.
  base::DictValue ExportSessionsAsValue(
      std::optional<base::span<const std::string>> session_ids =
          std::nullopt) const;

  // Returns every active and retained session, newest session first.
  //
  // `max_events` caps the events carried for each session. The newest events
  // are kept. `total_events` always reports the true count.
  // `max_message_bytes` caps the size of one `message`.
  //
  // Unlike ExportAllSessionsAsValue(), this returns typed data with absolute
  // timestamps, so callers do not need to parse a dictionary or resolve
  // TimeTicks.
  std::vector<SessionSnapshot> GetAllSessions(size_t max_events,
                                              size_t max_message_bytes) const;

  // TODO(b/561566505): Support export as trace file.
  // Exports all tracked sessions' metadata and history as JSON.
  std::string ExportAllSessionsAsJson() const;

  // Exports the tracked sessions matching `session_ids` as JSON.
  std::string ExportSessionsAsJson(
      base::span<const std::string> session_ids) const;

  // Clears all active and retained session recordings (e.g. on sign-out,
  // primary account switch, or service shutdown).
  void Clear();

  // For testing: returns the number of tracked active session recorders.
  size_t GetActiveRecordersCountForTesting() const;

  // For testing: returns the active recorder for `session_id`, or nullptr.
  SessionStreamRecorder* GetActiveRecorderForTesting(
      std::string_view session_id);

 private:
  // Returns the active recorder for `session_id`, or nullptr.
  SessionStreamRecorder* FindActiveRecorder(std::string_view session_id);

  // Marks the active recorder for `session_id` closed and moves it to the
  // retained recorders. Does nothing if there is no such recorder.
  void RetireRecorder(std::string_view session_id);

  SEQUENCE_CHECKER(sequence_checker_);

  // Recorders for sessions that are still registered, oldest first. The
  // transport caps concurrent sessions at a small number, so lookups scan.
  std::vector<std::unique_ptr<SessionStreamRecorder>> active_recorders_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Closed recorders, oldest first, kept so their history stays available for
  // dumps. Capped at `kMaxRetainedSessions`.
  std::vector<std::unique_ptr<SessionStreamRecorder>> retained_recorders_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace stream_transport

#endif  // COMPONENTS_STREAM_TRANSPORT_INTERNAL_SESSION_STREAM_RECORDER_H_
