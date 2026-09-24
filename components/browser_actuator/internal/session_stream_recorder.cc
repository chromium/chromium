// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/session_stream_recorder.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/internal/proto/transport_messages.to_value.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/payload_type_mapping.h"
#include "components/browser_actuator/public/transport_session.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"
#include "components/sharing_message/proto/actuator_downstream_message.to_value.h"

namespace browser_actuator {

base::DictValue SessionMetadata::ToValue() const {
  base::DictValue dict;
  dict.Set("session_id", session_id);
  dict.Set("is_active", is_active);
  dict.Set("total_downstream_messages",
           base::saturated_cast<int>(total_downstream_messages));
  dict.Set("total_upstream_messages",
           base::saturated_cast<int>(total_upstream_messages));
  dict.Set("start_time_ticks", start_time.since_origin().InMicrosecondsF());
  dict.Set("start_wall_time", base::TimeToValue(start_wall_time));
  if (end_time.has_value()) {
    dict.Set("end_time_ticks", end_time->since_origin().InMicrosecondsF());
    dict.Set("duration_ms", (*end_time - start_time).InMillisecondsF());
  }
  return dict;
}

base::DictValue SessionStreamRecorder::Entry::ToValue() const {
  base::DictValue dict;
  dict.Set("timestamp_ticks", timestamp.since_origin().InMicrosecondsF());
  if (const auto* downstream =
          std::get_if<ActuatorDownstreamMessage>(&message)) {
    dict.Set("direction", "Downstream");
    dict.Set("message", browser_actuator::ToValue(*downstream));
  } else {
    dict.Set("direction", "Upstream");
    dict.Set("message", browser_actuator::ToValue(
                            std::get<ActuatorUpstreamMessage>(message)));
  }
  return dict;
}

SessionStreamRecorder::SessionStreamRecorder(std::string_view session_id)
    : session_id_(session_id),
      metadata_{
          .session_id = std::string(session_id),
          .start_time = base::TimeTicks::Now(),
          .start_wall_time = base::Time::Now(),
          .total_downstream_messages = 0,
          .total_upstream_messages = 0,
          .is_active = true,
      } {}

SessionStreamRecorder::~SessionStreamRecorder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MarkSessionClosed();
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (destruction_callback_) {
    std::move(destruction_callback_).Run(metadata_, std::move(entries_));
  }
}

void SessionStreamRecorder::OnMessage(PayloadType payload_type,
                                      std::string_view serialized_payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Rebuild the single-payload message that the entry list records.
  ActuatorDownstreamMessage downstream;
  downstream.set_session_id(session_id_);
  auto* typed_payload = downstream.add_typed_payloads();
  typed_payload->set_payload_type(
      payload_type == PayloadType::kUnspecified
          ? ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_UNSPECIFIED
          : ToDownstreamProtoPayloadType(payload_type));
  typed_payload->mutable_proto_payload()->set_value(
      std::string(serialized_payload));
  RecordDownstreamMessage(std::move(downstream));
}

void SessionStreamRecorder::RecordDownstreamMessage(
    ActuatorDownstreamMessage message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metadata_.total_downstream_messages++;
  base::TimeTicks now = base::TimeTicks::Now();
  entries_.push_back(Entry{
      .timestamp = now,
      .message = std::move(message),
  });
  const auto& recorded_message =
      std::get<ActuatorDownstreamMessage>(entries_.back().message);
  for (auto& observer : observers_) {
    observer.OnDownstreamMessage(now, recorded_message);
  }
}

void SessionStreamRecorder::RecordUpstreamMessage(
    ActuatorUpstreamMessage message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metadata_.total_upstream_messages++;
  base::TimeTicks now = base::TimeTicks::Now();
  entries_.push_back(Entry{
      .timestamp = now,
      .message = std::move(message),
  });
  const auto& recorded_message =
      std::get<ActuatorUpstreamMessage>(entries_.back().message);
  for (auto& observer : observers_) {
    observer.OnUpstreamMessage(now, recorded_message);
  }
}

void SessionStreamRecorder::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void SessionStreamRecorder::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

std::string_view SessionStreamRecorder::session_id() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return session_id_;
}

const SessionMetadata& SessionStreamRecorder::metadata() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return metadata_;
}

const std::vector<SessionStreamRecorder::Entry>&
SessionStreamRecorder::entries() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return entries_;
}

void SessionStreamRecorder::MarkSessionClosed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (metadata_.is_active) {
    metadata_.is_active = false;
    metadata_.end_time = base::TimeTicks::Now();
  }
}

void SessionStreamRecorder::SetDestructionCallback(
    DestructionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  destruction_callback_ = std::move(callback);
}

base::WeakPtr<SessionStreamRecorder> SessionStreamRecorder::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

SessionStreamRecorderFactory::SessionStreamRecorderFactory() = default;

SessionStreamRecorderFactory::~SessionStreamRecorderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

FactoryId SessionStreamRecorderFactory::GetFactoryId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return FactoryId::kSessionStreamRecorder;
}

// LINT.IfChange(RecorderPayloadTypes)
std::vector<PayloadType>
SessionStreamRecorderFactory::GetSupportedPayloadTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {PayloadType::kControl, PayloadType::kExperimentalTriggering};
}
// LINT.ThenChange(//components/browser_actuator/public/common.h:PayloadType)

std::unique_ptr<TransportHandler> SessionStreamRecorderFactory::OnNewSession(
    TransportSession* session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!session) {
    return nullptr;
  }
  auto recorder =
      std::make_unique<SessionStreamRecorder>(session->GetSessionId());
  recorder->SetDestructionCallback(
      base::BindOnce(&SessionStreamRecorderFactory::OnRecorderDestroyed,
                     weak_ptr_factory_.GetWeakPtr(), recorder.get()));
  active_recorders_.push_back(recorder->GetWeakPtr());
  return recorder;
}

void SessionStreamRecorderFactory::OnUpstreamMessage(
    std::string_view session_id,
    const ActuatorUpstreamMessage& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& weak_recorder : active_recorders_) {
    if (weak_recorder && weak_recorder->session_id() == session_id) {
      weak_recorder->RecordUpstreamMessage(message);
      return;
    }
  }
  // No active recorder for this session (unknown or already closed). Drop it.
}

void SessionStreamRecorderFactory::OnRecorderDestroyed(
    SessionStreamRecorder* recorder,
    const SessionMetadata& metadata,
    std::vector<SessionStreamRecorder::Entry> entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::erase_if(active_recorders_, [recorder](const auto& ptr) {
    return !ptr || ptr.get() == recorder;
  });
  retained_sessions_.push_back(RetainedSessionHistory{
      .metadata = metadata,
      .entries = std::move(entries),
  });
}

size_t SessionStreamRecorderFactory::GetActiveRecordersCountForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t count = 0;
  for (const auto& ptr : active_recorders_) {
    if (ptr) {
      count++;
    }
  }
  return count;
}

base::DictValue SessionStreamRecorderFactory::ExportAllSessionsAsValue() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::DictValue root_dict;
  base::ListValue sessions_list;

  auto append_session =
      [&sessions_list](
          const SessionMetadata& metadata,
          const std::vector<SessionStreamRecorder::Entry>& entries) {
        base::DictValue session_dict = metadata.ToValue();
        base::ListValue events_list;
        for (const auto& entry : entries) {
          events_list.Append(entry.ToValue());
        }
        session_dict.Set("events", std::move(events_list));
        sessions_list.Append(std::move(session_dict));
      };

  for (const auto& weak_recorder : active_recorders_) {
    if (weak_recorder) {
      append_session(weak_recorder->metadata(), weak_recorder->entries());
    }
  }

  for (const auto& retained : retained_sessions_) {
    append_session(retained.metadata, retained.entries);
  }

  root_dict.Set("sessions", std::move(sessions_list));
  return root_dict;
}

std::string SessionStreamRecorderFactory::ExportAllSessionsAsJson() const {
  return base::WriteJsonWithOptions(ExportAllSessionsAsValue(),
                                    base::OPTIONS_PRETTY_PRINT)
      .value_or("");
}

}  // namespace browser_actuator
