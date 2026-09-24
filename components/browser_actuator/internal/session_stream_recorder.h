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
