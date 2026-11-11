// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/internal/session_stream_recorder.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/containers/circular_deque.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"
#include "components/sharing_message/proto/actuator_downstream_message.to_value.h"
#include "components/stream_transport/internal/metrics_utils.h"
#include "components/stream_transport/internal/proto/transport_messages.pb.h"
#include "components/stream_transport/internal/proto/transport_messages.to_value.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/payload_type_mapping.h"
#include "components/stream_transport/public/transport_session.h"

namespace stream_transport {

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
    browser_actuator::ActuatorUpstreamPayloadType proto_type) {
  switch (proto_type) {
    case browser_actuator::ACTUATOR_UPSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND:
      return PayloadType::kControl;
    case browser_actuator::
        ACTUATOR_UPSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING:
      return PayloadType::kExperimentalTriggering;
    case browser_actuator::ACTUATOR_UPSTREAM_PAYLOAD_TYPE_ACTUATION:
      return PayloadType::kActuation;
    case browser_actuator::ACTUATOR_UPSTREAM_PAYLOAD_TYPE_TASK_INGRESS:
      return PayloadType::kTaskIngress;
    case browser_actuator::ACTUATOR_UPSTREAM_PAYLOAD_TYPE_PROBE:
      return PayloadType::kProbe;
    case browser_actuator::ACTUATOR_UPSTREAM_PAYLOAD_TYPE_UNSPECIFIED:
    default:
      return std::nullopt;
  }
}
// LINT.ThenChange(//components/stream_transport/public/common.h:PayloadType)

std::string_view PayloadTypeName(std::optional<PayloadType> type) {
  return type.has_value() ? PayloadTypeToMetricSuffix(*type) : "Unspecified";
}

std::string_view PayloadTypeName(
    browser_actuator::ActuatorDownstreamPayloadType type) {
  return PayloadTypeName(FromDownstreamProtoPayloadType(type));
}

std::string_view PayloadTypeName(
    browser_actuator::ActuatorUpstreamPayloadType type) {
  return PayloadTypeName(FromUpstreamProtoPayloadType(type));
}

// `Any::Is<T>()` is unavailable for LITE_RUNTIME protos, so `type_url` is
// compared by hand. An empty value is accepted because the server does not
// always populate it (crbug.com/572804026).
bool HasExpectedPayloadTypeUrl(std::string_view type_url,
                               PayloadType payload_type) {
  return type_url.empty() || type_url == ExpectedTypeUrl(payload_type);
}

std::string FormatScrubbedProbePayload(std::string_view payload) {
  static constexpr size_t kScrubbedProbePayloadPreviewBytes = 40;
  return base::StrCat(
      {"[<", base::NumberToString(payload.size()),
       " synthetic bytes elided; head=",
       base::TruncateUTF8ToByteSize(payload, kScrubbedProbePayloadPreviewBytes),
       "...>]"});
}

// Synthetic `ProbeRequest` and `ProbeResponse` payloads can be up to 10 MB.
// Because `SessionStreamRecorder` retains recorded entries in memory for the
// lifetime of the profile, payloads larger than 128 bytes are replaced on
// ingest with a compact summary marker preserving the original byte length and
// leading `PROBE:...` header prefix. The 128-byte threshold exceeds the maximum
// summary marker length (~97 bytes), keeping scrubbing strictly size-reducing
// and idempotent.
std::optional<std::string> TryScrubTransportProbePayload(
    std::string_view serialized_probe) {
  static constexpr size_t kMaxRecordedProbePayloadBytes = 128;
  browser_actuator::TransportProbe probe;
  if (!probe.ParseFromString(serialized_probe)) {
    return std::nullopt;
  }
  switch (probe.payload_case()) {
    case browser_actuator::TransportProbe::kRequest:
      if (probe.request().payload().size() > kMaxRecordedProbePayloadBytes) {
        probe.mutable_request()->set_payload(
            FormatScrubbedProbePayload(probe.request().payload()));
        return probe.SerializeAsString();
      }
      break;
    case browser_actuator::TransportProbe::kResponse:
      if (probe.response().payload().size() > kMaxRecordedProbePayloadBytes) {
        probe.mutable_response()->set_payload(
            FormatScrubbedProbePayload(probe.response().payload()));
        return probe.SerializeAsString();
      }
      break;
    case browser_actuator::TransportProbe::PAYLOAD_NOT_SET:
      break;
  }
  return std::nullopt;
}

void MaybeScrubTransportProbePayload(std::string& serialized_probe) {
  if (std::optional<std::string> scrubbed =
          TryScrubTransportProbePayload(serialized_probe)) {
    // Move-assign a fresh string rather than calling `SerializeToString()` in
    // place, which would call `clear()`/`resize()` and retain the multi-MB
    // heap `capacity()` for the lifetime of the profile.
    serialized_probe = *std::move(scrubbed);
  }
}

template <typename EnvelopeProto, typename ToPayloadTypeFn>
void ScrubEnvelopeProbePayloads(EnvelopeProto& envelope,
                                ToPayloadTypeFn to_payload_type) {
  for (auto& typed_payload : *envelope.mutable_typed_payloads()) {
    if (to_payload_type(typed_payload.payload_type()) == PayloadType::kProbe &&
        typed_payload.has_proto_payload() &&
        HasExpectedPayloadTypeUrl(typed_payload.proto_payload().type_url(),
                                  PayloadType::kProbe) &&
        !typed_payload.proto_payload().value().empty()) {
      MaybeScrubTransportProbePayload(
          *typed_payload.mutable_proto_payload()->mutable_value());
    }
  }
}

void ScrubLargeProbePayloads(
    browser_actuator::ActuatorDownstreamMessage& message) {
  ScrubEnvelopeProbePayloads(message, &FromDownstreamProtoPayloadType);
}

void ScrubLargeProbePayloads(
    browser_actuator::ActuatorUpstreamMessage& message) {
  ScrubEnvelopeProbePayloads(message, &FromUpstreamProtoPayloadType);
}

template <typename EnvelopeProto, typename ToPayloadTypeFn>
base::DictValue SerializeEnvelopeWithDecodedPayloads(
    const EnvelopeProto& envelope,
    ToPayloadTypeFn to_payload_type) {
  base::DictValue dict = browser_actuator::ToValue(envelope).TakeDict();
  base::ListValue* payloads_list = dict.FindList("typed_payloads");
  if (!payloads_list) {
    return dict;
  }
  const size_t count =
      std::min(payloads_list->size(),
               static_cast<size_t>(envelope.typed_payloads_size()));
  for (size_t i = 0; i < count; ++i) {
    const auto& typed_payload = envelope.typed_payloads(static_cast<int>(i));
    if (!typed_payload.has_proto_payload() ||
        typed_payload.proto_payload().value().empty()) {
      continue;
    }
    const std::optional<PayloadType> payload_type =
        to_payload_type(typed_payload.payload_type());
    if (!payload_type.has_value() || (*payload_type != PayloadType::kControl &&
                                      *payload_type != PayloadType::kProbe)) {
      continue;
    }
    if (!HasExpectedPayloadTypeUrl(typed_payload.proto_payload().type_url(),
                                   *payload_type)) {
      continue;
    }
    base::DictValue* payload_dict = (*payloads_list)[i].GetIfDict();
    if (!payload_dict) {
      continue;
    }
    if (*payload_type == PayloadType::kControl) {
      browser_actuator::ControlCommand parsed_command;
      if (parsed_command.ParseFromString(
              typed_payload.proto_payload().value())) {
        payload_dict->Set("decoded_control_command",
                          browser_actuator::ToValue(parsed_command));
      }
    } else if (*payload_type == PayloadType::kProbe) {
      browser_actuator::TransportProbe parsed_probe;
      if (parsed_probe.ParseFromString(typed_payload.proto_payload().value())) {
        payload_dict->Set("decoded_transport_probe",
                          browser_actuator::ToValue(parsed_probe));
      }
    }
  }
  return dict;
}

base::DictValue SerializeMessageForDiagnostics(
    const browser_actuator::ActuatorDownstreamMessage& message) {
  return SerializeEnvelopeWithDecodedPayloads(message,
                                              &FromDownstreamProtoPayloadType);
}

base::DictValue SerializeMessageForDiagnostics(
    const browser_actuator::ActuatorUpstreamMessage& message) {
  return SerializeEnvelopeWithDecodedPayloads(message,
                                              &FromUpstreamProtoPayloadType);
}

SessionEventSnapshot BuildEventSnapshot(
    const SessionMetadata& metadata,
    const SessionStreamRecorder::Entry& entry,
    size_t max_message_bytes) {
  SessionEventSnapshot event;
  event.timestamp = ToWallTime(metadata, entry.timestamp);
  event.is_downstream =
      std::holds_alternative<browser_actuator::ActuatorDownstreamMessage>(
          entry.message);
  event.http_response_code = entry.http_response_code;
  event.downstream_status = entry.downstream_status;

  std::visit(
      [&event, max_message_bytes](const auto& msg) {
        bool has_oversized_payload = false;
        for (const auto& payload : msg.typed_payloads()) {
          event.payload_types.emplace_back(
              PayloadTypeName(payload.payload_type()));
          if (payload.proto_payload().value().size() > max_message_bytes) {
            has_oversized_payload = true;
          }
        }
        if (has_oversized_payload) {
          auto truncated_msg = msg;
          for (auto& payload : *truncated_msg.mutable_typed_payloads()) {
            if (payload.proto_payload().value().size() > max_message_bytes) {
              payload.mutable_proto_payload()->mutable_value()->resize(
                  max_message_bytes);
              event.message_truncated = true;
            }
          }
          event.message =
              base::WriteJson(SerializeMessageForDiagnostics(truncated_msg))
                  .value_or("");
        } else {
          event.message =
              base::WriteJson(SerializeMessageForDiagnostics(msg)).value_or("");
        }
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
    const base::circular_deque<SessionStreamRecorder::Entry>& entries,
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
  snapshot.total_events =
      metadata.total_downstream_messages + metadata.total_upstream_messages;

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

void SessionStreamRecorder::Observer::OnDownstreamMessage(
    base::TimeTicks timestamp,
    const browser_actuator::ActuatorDownstreamMessage& message,
    DownstreamDeliveryStatus status) {}

void SessionStreamRecorder::Observer::OnUpstreamMessage(
    base::TimeTicks timestamp,
    const browser_actuator::ActuatorUpstreamMessage& message,
    int http_response_code) {}

base::DictValue SessionStreamRecorder::Entry::ToValue() const {
  base::DictValue dict;
  dict.Set("timestamp_ticks", timestamp.since_origin().InMicrosecondsF());
  if (const auto* downstream =
          std::get_if<browser_actuator::ActuatorDownstreamMessage>(&message)) {
    dict.Set("direction", "Downstream");
    dict.Set("message", SerializeMessageForDiagnostics(*downstream));
    switch (downstream_status) {
      case DownstreamDeliveryStatus::kSuccess:
        dict.Set("downstream_status", "Success");
        break;
      case DownstreamDeliveryStatus::kFailure:
        dict.Set("downstream_status", "Failure");
        break;
      case DownstreamDeliveryStatus::kUnknown:
      default:
        dict.Set("downstream_status", "Unknown");
        break;
    }
  } else {
    dict.Set("direction", "Upstream");
    dict.Set("message",
             SerializeMessageForDiagnostics(
                 std::get<browser_actuator::ActuatorUpstreamMessage>(message)));
    dict.Set("http_response_code", http_response_code);
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
    browser_actuator::ActuatorDownstreamMessage message,
    DownstreamDeliveryStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScrubLargeProbePayloads(message);
  metadata_.total_downstream_messages++;
  for (browser_actuator::ActuatorDownstreamTypedPayload& typed_payload :
       *message.mutable_typed_payloads()) {
    if (typed_payload.has_proto_payload() &&
        typed_payload.proto_payload().type_url().empty()) {
      if (std::optional<PayloadType> payload_type =
              FromDownstreamProtoPayloadType(typed_payload.payload_type())) {
        typed_payload.mutable_proto_payload()->set_type_url(
            std::string(ExpectedTypeUrl(*payload_type)));
      }
    }
  }
  if (entries_.size() >= kMaxEntriesPerSession) {
    entries_.pop_front();
  }
  base::TimeTicks now = base::TimeTicks::Now();
  entries_.push_back(Entry{
      .timestamp = now,
      .message = std::move(message),
      .downstream_status = status,
  });
  const auto& recorded_message =
      std::get<browser_actuator::ActuatorDownstreamMessage>(
          entries_.back().message);
  for (auto& observer : observers_) {
    observer.OnDownstreamMessage(now, recorded_message, status);
  }
}

bool SessionStreamRecorder::UpdateUpstreamResponseCode(
    int64_t client_sequence_number,
    int http_response_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Entry& entry : base::Reversed(entries_)) {
    if (std::holds_alternative<browser_actuator::ActuatorUpstreamMessage>(
            entry.message)) {
      const auto& upstream =
          std::get<browser_actuator::ActuatorUpstreamMessage>(entry.message);
      if (upstream.client_sequence_number() == client_sequence_number &&
          entry.http_response_code == 0) {
        entry.http_response_code = http_response_code;
        for (auto& observer : observers_) {
          observer.OnUpstreamMessage(entry.timestamp, upstream,
                                     http_response_code);
        }
        return true;
      }
    }
  }
  return false;
}

void SessionStreamRecorder::RecordUpstreamMessage(
    browser_actuator::ActuatorUpstreamMessage message,
    int http_response_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScrubLargeProbePayloads(message);
  metadata_.total_upstream_messages++;
  if (entries_.size() >= kMaxEntriesPerSession) {
    entries_.pop_front();
  }
  base::TimeTicks now = base::TimeTicks::Now();
  entries_.push_back(Entry{
      .timestamp = now,
      .message = std::move(message),
      .http_response_code = http_response_code,
  });
  const auto& recorded_message =
      std::get<browser_actuator::ActuatorUpstreamMessage>(
          entries_.back().message);
  for (auto& observer : observers_) {
    observer.OnUpstreamMessage(now, recorded_message, http_response_code);
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

const base::circular_deque<SessionStreamRecorder::Entry>&
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
    const browser_actuator::ActuatorDownstreamMessage& message,
    DownstreamDeliveryStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (SessionStreamRecorder* recorder = FindActiveRecorder(session_id)) {
    recorder->RecordDownstreamMessage(message, status);
  }
  // No active recorder for this session (unknown or already closed). Drop it.
}

void SessionStreamRecorderFactory::OnUpstreamMessage(
    std::string_view session_id,
    const browser_actuator::ActuatorUpstreamMessage& message,
    int http_response_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (http_response_code != 0) {
    if (SessionStreamRecorder* recorder = FindActiveRecorder(session_id)) {
      recorder->UpdateUpstreamResponseCode(message.client_sequence_number(),
                                           http_response_code);
      return;
    }
    auto it = std::ranges::find(retained_recorders_, session_id,
                                &SessionStreamRecorder::session_id);
    if (it != retained_recorders_.end()) {
      (*it)->UpdateUpstreamResponseCode(message.client_sequence_number(),
                                        http_response_code);
    }
    return;
  }
  if (SessionStreamRecorder* recorder = FindActiveRecorder(session_id)) {
    recorder->RecordUpstreamMessage(message, /*http_response_code=*/0);
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

void SessionStreamRecorderFactory::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  active_recorders_.clear();
  retained_recorders_.clear();
}

}  // namespace stream_transport
