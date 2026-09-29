// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_SESSION_STREAM_RECORDER_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_SESSION_STREAM_RECORDER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/internal/transport_message_observer.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_handler.h"
#include "components/browser_actuator/public/transport_handler_factory.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"

namespace base {
class DictValue;
}  // namespace base

namespace browser_actuator {

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
// ActuatorUpstreamMessage) for a single TransportSession using an unbounded
// std::vector. Observers can subscribe to receive live messages as they occur.
class SessionStreamRecorder : public TransportHandler {
 public:
  // Stored entry pairing timestamp with canonical protobuf message.
  struct Entry {
    base::TimeTicks timestamp;
    std::variant<ActuatorDownstreamMessage, ActuatorUpstreamMessage> message;

    // Serializes this entry for diagnostic dumps. The message body is
    // serialized by the generated `ToValue()` from //components/proto_extras,
    // so new proto fields are picked up automatically.
    base::DictValue ToValue() const;
  };

  using DestructionCallback =
      base::OnceCallback<void(const SessionMetadata&, std::vector<Entry>)>;

  // Direct proto observer interface for real-time streaming.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDownstreamMessage(base::TimeTicks timestamp,
                                     const ActuatorDownstreamMessage& message) {
    }
    virtual void OnUpstreamMessage(base::TimeTicks timestamp,
                                   const ActuatorUpstreamMessage& message) {}
  };

  explicit SessionStreamRecorder(std::string_view session_id);
  ~SessionStreamRecorder() override;

  SessionStreamRecorder(const SessionStreamRecorder&) = delete;
  SessionStreamRecorder& operator=(const SessionStreamRecorder&) = delete;

  // TransportHandler implementation:
  void OnMessage(PayloadType payload_type,
                 std::string_view serialized_payload) override;

  // Direct recording methods:
  void RecordDownstreamMessage(ActuatorDownstreamMessage message);
  void RecordUpstreamMessage(ActuatorUpstreamMessage message);

  // Observer management:
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  std::string_view session_id() const;
  const SessionMetadata& metadata() const;
  const std::vector<Entry>& entries() const;

  // Marks the session as closed.
  void MarkSessionClosed();

  // Sets a callback invoked on destruction to retain history for dumps.
  void SetDestructionCallback(DestructionCallback callback);

  base::WeakPtr<SessionStreamRecorder> GetWeakPtr();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  const std::string session_id_;
  SessionMetadata metadata_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::vector<Entry> entries_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::ObserverList<Observer> observers_ GUARDED_BY_CONTEXT(sequence_checker_);

  DestructionCallback destruction_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<SessionStreamRecorder> weak_ptr_factory_{this};
};

// Factory that creates and tracks SessionStreamRecorder instances.
// Provides export functionality to dump all sessions' history as JSON.
class SessionStreamRecorderFactory : public TransportHandlerFactory,
                                     public TransportMessageObserver {
 public:
  SessionStreamRecorderFactory();
  ~SessionStreamRecorderFactory() override;

  SessionStreamRecorderFactory(const SessionStreamRecorderFactory&) = delete;
  SessionStreamRecorderFactory& operator=(const SessionStreamRecorderFactory&) =
      delete;

  // Narrows a factory obtained from BrowserActuatorService::GetFactory().
  // Returns nullptr if `factory` is null or is not a recorder factory.
  static SessionStreamRecorderFactory* FromFactory(
      TransportHandlerFactory* factory);

  // TransportHandlerFactory implementation:
  FactoryId GetFactoryId() const override;
  std::vector<PayloadType> GetSupportedPayloadTypes() const override;
  std::unique_ptr<TransportHandler> OnNewSession(
      TransportSession* session) override;

  // TransportMessageObserver implementation:
  void OnUpstreamMessage(std::string_view session_id,
                         const ActuatorUpstreamMessage& message) override;

  // Exports all tracked sessions' metadata and history as a structured
  // dictionary.
  base::DictValue ExportAllSessionsAsValue() const;

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

  // For testing: returns the number of tracked active session recorders.
  size_t GetActiveRecordersCountForTesting() const;

 private:
  struct RetainedSessionHistory {
    SessionMetadata metadata;
    std::vector<SessionStreamRecorder::Entry> entries;
  };

  void OnRecorderDestroyed(SessionStreamRecorder* recorder,
                           const SessionMetadata& metadata,
                           std::vector<SessionStreamRecorder::Entry> entries);

  SEQUENCE_CHECKER(sequence_checker_);

  // Active session recorders tracked via WeakPtr.
  std::vector<base::WeakPtr<SessionStreamRecorder>> active_recorders_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Retained history for closed/destroyed sessions.
  std::vector<RetainedSessionHistory> retained_sessions_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<SessionStreamRecorderFactory> weak_ptr_factory_{this};
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_SESSION_STREAM_RECORDER_H_
