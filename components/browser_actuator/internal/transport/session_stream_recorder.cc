// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/session_stream_recorder.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/browser_actuator/internal/transport/metrics_utils.h"
#include "components/browser_actuator/internal/transport/proto/transport_messages.pb.h"
#include "components/browser_actuator/internal/transport/proto/transport_messages.to_value.h"
#include "components/browser_actuator/public/transport/common.h"
#include "components/browser_actuator/public/transport/payload_type_mapping.h"
#include "components/browser_actuator/public/transport/transport_session.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"
#include "components/sharing_message/proto/actuator_downstream_message.to_value.h"

namespace browser_actuator::transport {

SessionEventSnapshot::SessionEventSnapshot() = default;
SessionEventSnapshot::SessionEventSnapshot(const SessionEventSnapshot&) =
    default;
SessionEventSnapshot::SessionEventSnapshot(SessionEventSnapshot&&) = default;
SessionEventSnapshot& SessionEventSnapshot::operator=(
    const SessionEventSnapshot&) = default;
SessionEventSnapshot& SessionEventSnapshot::operator=(SessionEventSnapshot&&) =
    default;
SessionEventSnapshot::~SessionEventSnapshot() = default;

SessionSnapshot::SessionSnapshot() = default;
SessionSnapshot::SessionSnapshot(const SessionSnapshot&) = default;
SessionSnapshot::SessionSnapshot(SessionSnapshot&&) = default;
SessionSnapshot& SessionSnapshot::operator=(const SessionSnapshot&) = default;
SessionSnapshot& SessionSnapshot::operator=(SessionSnapshot&&) = default;
SessionSnapshot::~SessionSnapshot() = default;

namespace {

// The recorder stores monotonic TimeTicks, which have no absolute epoch. The
// metadata carries both the TimeTicks and the wall time of the session start,
// so an absolute time for any entry is start_wall_time plus the elapsed ticks.
base::Time ToWallTime(const SessionMetadata& metadata, base::TimeTicks ticks) {
  return metadata.start_wall_time + (ticks - metadata.start_time);
}

// LINT.IfChange(FromUpstreamProtoPayloadType)
constexpr std::optional<PayloadType> FromUpstreamProtoPayloadType(
    ActuatorUpstreamPayloadType proto_type) {
  switch (proto_type) {
    case ACTUATOR_UPSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND:
      return PayloadType::kControl;
    case ACTUATOR_UPSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING:
      return PayloadType::kExperimentalTriggering;
    case ACTUATOR_UPSTREAM_PAYLOAD_TYPE_ACTUATION:
      return PayloadType::kActuation;
    case ACTUATOR_UPSTREAM_PAYLOAD_TYPE_UNSPECIFIED:
    default:
      return std::nullopt;
  }
}
// LINT.ThenChange(//components/browser_actuator/public/transport/common.h:PayloadType)

std::string_view PayloadTypeName(std::optional<PayloadType> type) {
  return type.has_value() ? PayloadTypeToMetricSuffix(*type) : "Unspecified";
}

std::string_view PayloadTypeName(ActuatorDownstreamPayloadType type) {
  return PayloadTypeName(FromDownstreamProtoPayloadType(type));
}

std::string_view PayloadTypeName(ActuatorUpstreamPayloadType type) {
  return PayloadTypeName(FromUpstreamProtoPayloadType(type));
}

SessionEventSnapshot BuildEventSnapshot(
    const SessionMetadata& metadata,
    const SessionStreamRecorder::Entry& entry,
    size_t max_message_bytes) {
  SessionEventSnapshot event;
  event.timestamp = ToWallTime(metadata, entry.timestamp);
  event.is_downstream =
      std::holds_alternative<ActuatorDownstreamMessage>(entry.message);

  std::visit(
      [&event](const auto& msg) {
        for (const auto& payload : msg.typed_payloads()) {
          event.payload_types.emplace_back(
              PayloadTypeName(payload.payload_type()));
        }
        event.message =
            base::WriteJson(browser_actuator::ToValue(msg)).value_or("");
      },
      entry.message);

  if (event.message.size() > max_message_bytes) {
    event.message = std::string(
        base::TruncateUTF8ToByteSize(event.message, max_message_bytes));
    event.message_truncated = true;
  }

  return event;
}

SessionSnapshot BuildSessionSnapshot(
    const SessionMetadata& metadata,
    const std::vector<SessionStreamRecorder::Entry>& entries,
    size_t max_events,
    size_t max_message_bytes) {
  SessionSnapshot snapshot;
  snapshot.session_id = metadata.session_id;
  snapshot.start_wall_time = metadata.start_wall_time;
  if (metadata.end_time.has_value()) {
    snapshot.end_wall_time = ToWallTime(metadata, *metadata.end_time);
  }
  snapshot.total_downstream_messages = metadata.total_downstream_messages;
  snapshot.total_upstream_messages = metadata.total_upstream_messages;
  snapshot.total_events = entries.size();

  size_t kept_count = std::min(max_events, entries.size());
  size_t start_index = entries.size() - kept_count;
  snapshot.events.reserve(kept_count);

  for (size_t i = start_index; i < entries.size(); ++i) {
    snapshot.events.push_back(
        BuildEventSnapshot(metadata, entries[i], max_message_bytes));
  }

  return snapshot;
}

}  // namespace

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

SessionStreamRecorderFactory::SessionStreamRecorderFactory() = default;

SessionStreamRecorderFactory::~SessionStreamRecorderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SessionStreamRecorderFactory::OnSessionRegistered(
    TransportSession* session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!session) {
    return;
  }
  std::string_view session_id = session->GetSessionId();
  // The registry never registers an ID that is still active, but retire any
  // leftover recorder so its history is not lost.
  RetireRecorder(session_id);
  active_recorders_.push_back(
      std::make_unique<SessionStreamRecorder>(session_id));
}

void SessionStreamRecorderFactory::OnSessionRemoved(
    std::string_view session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RetireRecorder(session_id);
}

void SessionStreamRecorderFactory::OnDownstreamMessage(
    std::string_view session_id,
    const ActuatorDownstreamMessage& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (SessionStreamRecorder* recorder = FindActiveRecorder(session_id)) {
    recorder->RecordDownstreamMessage(message);
  }
  // No active recorder for this session (unknown or already closed). Drop it.
}

void SessionStreamRecorderFactory::OnUpstreamMessage(
    std::string_view session_id,
    const ActuatorUpstreamMessage& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (SessionStreamRecorder* recorder = FindActiveRecorder(session_id)) {
    recorder->RecordUpstreamMessage(message);
  }
  // No active recorder for this session (unknown or already closed). Drop it.
}

SessionStreamRecorder* SessionStreamRecorderFactory::FindActiveRecorder(
    std::string_view session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = std::ranges::find(active_recorders_, session_id,
                              &SessionStreamRecorder::session_id);
  return it == active_recorders_.end() ? nullptr : it->get();
}

void SessionStreamRecorderFactory::RetireRecorder(std::string_view session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = std::ranges::find(active_recorders_, session_id,
                              &SessionStreamRecorder::session_id);
  if (it == active_recorders_.end()) {
    return;
  }
  (*it)->MarkSessionClosed();
  retained_recorders_.push_back(std::move(*it));
  active_recorders_.erase(it);
  if (retained_recorders_.size() > kMaxRetainedSessions) {
    retained_recorders_.erase(retained_recorders_.begin());
  }
}

size_t SessionStreamRecorderFactory::GetActiveRecordersCountForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return active_recorders_.size();
}

SessionStreamRecorder*
SessionStreamRecorderFactory::GetActiveRecorderForTesting(
    std::string_view session_id) {
  return FindActiveRecorder(session_id);
}

base::DictValue SessionStreamRecorderFactory::ExportAllSessionsAsValue() const {
  return ExportSessionsAsValue(std::nullopt);
}

base::DictValue SessionStreamRecorderFactory::ExportSessionsAsValue(
    std::optional<base::span<const std::string>> session_ids) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::DictValue root_dict;
  base::ListValue sessions_list;

  auto append_sessions =
      [&sessions_list,
       &session_ids](const std::vector<std::unique_ptr<SessionStreamRecorder>>&
                         recorders) {
        for (const auto& recorder : recorders) {
          if (session_ids.has_value() &&
              !std::ranges::contains(*session_ids, recorder->session_id())) {
            continue;
          }
          base::DictValue session_dict = recorder->metadata().ToValue();
          base::ListValue events_list;
          for (const auto& entry : recorder->entries()) {
            events_list.Append(entry.ToValue());
          }
          session_dict.Set("events", std::move(events_list));
          sessions_list.Append(std::move(session_dict));
        }
      };

  append_sessions(active_recorders_);
  append_sessions(retained_recorders_);

  root_dict.Set("sessions", std::move(sessions_list));
  return root_dict;
}

std::vector<SessionSnapshot> SessionStreamRecorderFactory::GetAllSessions(
    size_t max_events,
    size_t max_message_bytes) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<SessionSnapshot> sessions;
  sessions.reserve(active_recorders_.size() + retained_recorders_.size());

  for (const auto* recorders : {&active_recorders_, &retained_recorders_}) {
    for (const auto& recorder : *recorders) {
      sessions.push_back(BuildSessionSnapshot(recorder->metadata(),
                                              recorder->entries(), max_events,
                                              max_message_bytes));
    }
  }

  std::sort(sessions.begin(), sessions.end(),
            [](const SessionSnapshot& a, const SessionSnapshot& b) {
              return a.start_wall_time > b.start_wall_time;
            });

  return sessions;
}

std::string SessionStreamRecorderFactory::ExportAllSessionsAsJson() const {
  return base::WriteJsonWithOptions(ExportAllSessionsAsValue(),
                                    base::OPTIONS_PRETTY_PRINT)
      .value_or("");
}

std::string SessionStreamRecorderFactory::ExportSessionsAsJson(
    base::span<const std::string> session_ids) const {
  return base::WriteJsonWithOptions(ExportSessionsAsValue(session_ids),
                                    base::OPTIONS_PRETTY_PRINT)
      .value_or("");
}

}  // namespace browser_actuator::transport
