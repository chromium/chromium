// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/session_stream_recorder.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/browser_actuator/internal/transport/probe_payload.h"
#include "components/browser_actuator/internal/transport/proto/transport_messages.pb.h"
#include "components/browser_actuator/public/transport/common.h"
#include "components/browser_actuator/public/transport/payload_type_mapping.h"
#include "components/browser_actuator/public/transport/transport_session.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator::transport {
namespace {

class FakeTransportSession : public TransportSession {
 public:
  explicit FakeTransportSession(std::string_view session_id)
      : session_id_(session_id) {}
  ~FakeTransportSession() override = default;

  std::string_view GetSessionId() const override { return session_id_; }
  base::expected<void, SendUpstreamMessageError> SendUpstreamMessage(
      PayloadType payload_type,
      const google::protobuf::MessageLite& message) override {
    return {};
  }
  void OnMessage(PayloadType payload_type,
                 std::string_view serialized_payload) override {}
  void Close() override {}

 private:
  std::string session_id_;
};

class TestObserver : public SessionStreamRecorder::Observer {
 public:
  void OnDownstreamMessage(base::TimeTicks timestamp,
                           const ActuatorDownstreamMessage& message,
                           DownstreamDeliveryStatus status) override {
    downstream_count++;
    last_downstream_seq = message.sequence_number();
    last_downstream_status = status;
  }

  void OnUpstreamMessage(base::TimeTicks timestamp,
                         const ActuatorUpstreamMessage& message,
                         int http_response_code) override {
    upstream_count++;
    last_upstream_seq = message.client_sequence_number();
    last_http_response_code = http_response_code;
  }

  size_t downstream_count = 0;
  size_t upstream_count = 0;
  int64_t last_downstream_seq = 0;
  int64_t last_upstream_seq = 0;
  DownstreamDeliveryStatus last_downstream_status =
      DownstreamDeliveryStatus::kUnknown;
  int last_http_response_code = 0;
};

class DefaultObserver : public SessionStreamRecorder::Observer {};

TEST(SessionStreamRecorderTest, InitialState) {
  SessionStreamRecorder recorder("session_123");

  EXPECT_EQ(recorder.session_id(), "session_123");
  EXPECT_EQ(recorder.metadata().session_id, "session_123");
  EXPECT_TRUE(recorder.metadata().is_active);
  EXPECT_EQ(recorder.metadata().total_downstream_messages, 0u);
  EXPECT_EQ(recorder.metadata().total_upstream_messages, 0u);
  EXPECT_TRUE(recorder.entries().empty());
}

TEST(SessionStreamRecorderTest, RecordsDownstreamMessage) {
  SessionStreamRecorder recorder("session_123");

  ActuatorDownstreamMessage downstream;
  downstream.set_session_id("session_123");
  downstream.set_sequence_number(5);

  recorder.RecordDownstreamMessage(downstream,
                                   DownstreamDeliveryStatus::kSuccess);

  EXPECT_EQ(recorder.metadata().total_downstream_messages, 1u);
  EXPECT_EQ(recorder.metadata().total_upstream_messages, 0u);
  ASSERT_EQ(recorder.entries().size(), 1u);

  const auto& entry = recorder.entries().front();
  EXPECT_FALSE(entry.timestamp.is_null());
  ASSERT_TRUE(std::holds_alternative<ActuatorDownstreamMessage>(entry.message));
  EXPECT_EQ(
      std::get<ActuatorDownstreamMessage>(entry.message).sequence_number(), 5);
}

TEST(SessionStreamRecorderTest, RecordsUpstreamMessage) {
  SessionStreamRecorder recorder("session_123");

  ActuatorUpstreamMessage upstream;
  upstream.set_session_id("session_123");
  upstream.set_client_sequence_number(10);

  recorder.RecordUpstreamMessage(upstream, 200);

  EXPECT_EQ(recorder.metadata().total_downstream_messages, 0u);
  EXPECT_EQ(recorder.metadata().total_upstream_messages, 1u);
  ASSERT_EQ(recorder.entries().size(), 1u);

  const auto& entry = recorder.entries().front();
  EXPECT_FALSE(entry.timestamp.is_null());
  ASSERT_TRUE(std::holds_alternative<ActuatorUpstreamMessage>(entry.message));
  EXPECT_EQ(
      std::get<ActuatorUpstreamMessage>(entry.message).client_sequence_number(),
      10);
  EXPECT_EQ(entry.http_response_code, 200);
}

TEST(SessionStreamRecorderTest, UpdatesUpstreamMessageResponseCodeInPlace) {
  SessionStreamRecorder recorder("session_123");
  TestObserver observer;
  recorder.AddObserver(&observer);

  ActuatorUpstreamMessage upstream;
  upstream.set_session_id("session_123");
  upstream.set_client_sequence_number(10);

  // Phase 1: Record initial pending upstream message with status 0.
  recorder.RecordUpstreamMessage(upstream, 0);

  EXPECT_EQ(recorder.metadata().total_upstream_messages, 1u);
  ASSERT_EQ(recorder.entries().size(), 1u);
  EXPECT_EQ(recorder.entries().front().http_response_code, 0);
  EXPECT_EQ(observer.upstream_count, 1u);
  EXPECT_EQ(observer.last_upstream_seq, 10);
  EXPECT_EQ(observer.last_http_response_code, 0);

  // Phase 2: Update HTTP response code in place to 200.
  EXPECT_TRUE(recorder.UpdateUpstreamResponseCode(10, 200));

  EXPECT_EQ(recorder.metadata().total_upstream_messages, 1u);
  ASSERT_EQ(recorder.entries().size(), 1u);
  EXPECT_EQ(recorder.entries().front().http_response_code, 200);
  EXPECT_EQ(observer.upstream_count, 2u);
  EXPECT_EQ(observer.last_upstream_seq, 10);
  EXPECT_EQ(observer.last_http_response_code, 200);

  // Attempting to update an unknown sequence number returns false.
  EXPECT_FALSE(recorder.UpdateUpstreamResponseCode(99, 200));

  recorder.RemoveObserver(&observer);
}

TEST(SessionStreamRecorderTest, PreservesChronologicalOrderAcrossDirections) {
  SessionStreamRecorder recorder("session_123");

  ActuatorDownstreamMessage d1;
  d1.set_sequence_number(1);
  recorder.RecordDownstreamMessage(d1, DownstreamDeliveryStatus::kSuccess);

  ActuatorUpstreamMessage u1;
  u1.set_client_sequence_number(2);
  recorder.RecordUpstreamMessage(u1, 200);

  ActuatorDownstreamMessage d2;
  d2.set_sequence_number(3);
  recorder.RecordDownstreamMessage(d2, DownstreamDeliveryStatus::kSuccess);

  const auto& entries = recorder.entries();
  ASSERT_EQ(entries.size(), 3u);

  ASSERT_TRUE(
      std::holds_alternative<ActuatorDownstreamMessage>(entries[0].message));
  EXPECT_EQ(
      std::get<ActuatorDownstreamMessage>(entries[0].message).sequence_number(),
      1);

  ASSERT_TRUE(
      std::holds_alternative<ActuatorUpstreamMessage>(entries[1].message));
  EXPECT_EQ(std::get<ActuatorUpstreamMessage>(entries[1].message)
                .client_sequence_number(),
            2);

  ASSERT_TRUE(
      std::holds_alternative<ActuatorDownstreamMessage>(entries[2].message));
  EXPECT_EQ(
      std::get<ActuatorDownstreamMessage>(entries[2].message).sequence_number(),
      3);
}

TEST(SessionStreamRecorderTest, NotifiesObservers) {
  SessionStreamRecorder recorder("session_123");
  TestObserver observer;
  recorder.AddObserver(&observer);

  ActuatorDownstreamMessage d;
  d.set_sequence_number(1);
  recorder.RecordDownstreamMessage(d, DownstreamDeliveryStatus::kSuccess);

  EXPECT_EQ(observer.downstream_count, 1u);
  EXPECT_EQ(observer.last_downstream_seq, 1);
  EXPECT_EQ(observer.last_downstream_status,
            DownstreamDeliveryStatus::kSuccess);

  ActuatorUpstreamMessage u;
  u.set_client_sequence_number(2);
  recorder.RecordUpstreamMessage(u, 200);

  EXPECT_EQ(observer.upstream_count, 1u);
  EXPECT_EQ(observer.last_upstream_seq, 2);
  EXPECT_EQ(observer.last_http_response_code, 200);

  recorder.RemoveObserver(&observer);

  ActuatorDownstreamMessage d2;
  d2.set_sequence_number(3);
  recorder.RecordDownstreamMessage(d2, DownstreamDeliveryStatus::kSuccess);

  EXPECT_EQ(observer.downstream_count, 1u);
}

TEST(SessionStreamRecorderTest, ObserverDefaultMethodsDoNothing) {
  SessionStreamRecorder recorder("session_123");
  DefaultObserver observer;
  recorder.AddObserver(&observer);

  ActuatorDownstreamMessage downstream;
  recorder.RecordDownstreamMessage(downstream,
                                   DownstreamDeliveryStatus::kSuccess);

  ActuatorUpstreamMessage upstream;
  recorder.RecordUpstreamMessage(upstream, 200);

  recorder.RemoveObserver(&observer);
}

TEST(SessionStreamRecorderTest, MarkSessionClosed) {
  SessionStreamRecorder recorder("session_123");
  EXPECT_TRUE(recorder.metadata().is_active);
  EXPECT_FALSE(recorder.metadata().end_time.has_value());

  recorder.MarkSessionClosed();
  EXPECT_FALSE(recorder.metadata().is_active);
  EXPECT_TRUE(recorder.metadata().end_time.has_value());
  base::TimeTicks first_end_time = *recorder.metadata().end_time;

  recorder.MarkSessionClosed();
  EXPECT_FALSE(recorder.metadata().is_active);
  EXPECT_EQ(*recorder.metadata().end_time, first_end_time);
}

TEST(SessionStreamRecorderFactoryTest,
     CreatesEmptyRecorderOnSessionRegistered) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_xyz");

  factory.OnSessionRegistered(&session);

  SessionStreamRecorder* recorder =
      factory.GetActiveRecorderForTesting(session.GetSessionId());
  ASSERT_NE(recorder, nullptr);
  EXPECT_EQ(recorder->session_id(), "session_xyz");
  EXPECT_TRUE(recorder->metadata().is_active);
  EXPECT_TRUE(recorder->entries().empty());
  EXPECT_EQ(factory.GetActiveRecordersCountForTesting(), 1u);
}

TEST(SessionStreamRecorderFactoryTest, IgnoresNullSession) {
  SessionStreamRecorderFactory factory;
  factory.OnSessionRegistered(nullptr);
  EXPECT_EQ(factory.GetActiveRecordersCountForTesting(), 0u);
}

TEST(SessionStreamRecorderFactoryTest, RecordsEnvelopesInArrivalOrder) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_1");
  factory.OnSessionRegistered(&session);

  // A downstream envelope and the reply a handler sends while handling it.
  ActuatorDownstreamMessage wake;
  wake.set_session_id("session_1");
  wake.set_sequence_number(0);
  wake.add_typed_payloads()->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  wake.add_typed_payloads()->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING);
  factory.OnDownstreamMessage("session_1", wake,
                              DownstreamDeliveryStatus::kSuccess);

  ActuatorUpstreamMessage ack;
  ack.set_session_id("session_1");
  ack.set_client_sequence_number(1);
  ack.set_responding_to_sequence_number(0);
  factory.OnUpstreamMessage("session_1", ack, 0);
  factory.OnUpstreamMessage("session_1", ack, 200);

  SessionStreamRecorder* recorder =
      factory.GetActiveRecorderForTesting("session_1");
  ASSERT_NE(recorder, nullptr);
  EXPECT_EQ(recorder->metadata().total_downstream_messages, 1u);
  EXPECT_EQ(recorder->metadata().total_upstream_messages, 1u);
  ASSERT_EQ(recorder->entries().size(), 2u);

  // The whole envelope is kept, including its sequence number and every
  // typed payload.
  ASSERT_TRUE(std::holds_alternative<ActuatorDownstreamMessage>(
      recorder->entries()[0].message));
  const auto& recorded_wake =
      std::get<ActuatorDownstreamMessage>(recorder->entries()[0].message);
  EXPECT_EQ(recorded_wake.sequence_number(), 0);
  EXPECT_EQ(recorded_wake.typed_payloads_size(), 2);

  ASSERT_TRUE(std::holds_alternative<ActuatorUpstreamMessage>(
      recorder->entries()[1].message));
  EXPECT_EQ(std::get<ActuatorUpstreamMessage>(recorder->entries()[1].message)
                .responding_to_sequence_number(),
            0);
}

TEST(SessionStreamRecorderFactoryTest,
     DropsDownstreamMessageForUnknownOrClosedSession) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_1");

  ActuatorDownstreamMessage message;
  message.set_session_id("session_1");
  factory.OnDownstreamMessage("session_1", message,
                              DownstreamDeliveryStatus::kSuccess);
  EXPECT_TRUE(factory.ExportAllSessionsAsValue().FindList("sessions")->empty());

  factory.OnSessionRegistered(&session);
  factory.OnSessionRemoved("session_1");
  factory.OnDownstreamMessage("session_1", message,
                              DownstreamDeliveryStatus::kSuccess);

  std::vector<SessionSnapshot> sessions =
      factory.GetAllSessions(/*max_events=*/10, /*max_message_bytes=*/1024);
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_EQ(sessions[0].total_downstream_messages, 0u);
  EXPECT_TRUE(sessions[0].end_wall_time.has_value());
}

TEST(SessionStreamRecorderFactoryTest,
     ReRegisteringSessionIdKeepsPriorHistory) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_1");

  factory.OnSessionRegistered(&session);
  ActuatorDownstreamMessage message;
  message.set_session_id("session_1");
  factory.OnDownstreamMessage("session_1", message,
                              DownstreamDeliveryStatus::kSuccess);
  factory.OnSessionRemoved("session_1");

  factory.OnSessionRegistered(&session);

  SessionStreamRecorder* active =
      factory.GetActiveRecorderForTesting("session_1");
  ASSERT_NE(active, nullptr);
  EXPECT_TRUE(active->entries().empty());

  std::vector<SessionSnapshot> sessions =
      factory.GetAllSessions(/*max_events=*/10, /*max_message_bytes=*/1024);
  ASSERT_EQ(sessions.size(), 2u);
  size_t closed_count = 0;
  for (const auto& snapshot : sessions) {
    EXPECT_EQ(snapshot.session_id, "session_1");
    if (snapshot.end_wall_time.has_value()) {
      ++closed_count;
      EXPECT_EQ(snapshot.total_downstream_messages, 1u);
    }
  }
  EXPECT_EQ(closed_count, 1u);
}

TEST(SessionStreamRecorderFactoryTest, DropsOldestRetainedSessionsPastCap) {
  SessionStreamRecorderFactory factory;
  const size_t total = SessionStreamRecorderFactory::kMaxRetainedSessions + 2;
  for (size_t i = 0; i < total; ++i) {
    FakeTransportSession session("session_" + base::NumberToString(i));
    factory.OnSessionRegistered(&session);
    factory.OnSessionRemoved(session.GetSessionId());
  }

  base::DictValue dump = factory.ExportAllSessionsAsValue();
  const base::ListValue* sessions = dump.FindList("sessions");
  ASSERT_NE(sessions, nullptr);
  ASSERT_EQ(sessions->size(),
            SessionStreamRecorderFactory::kMaxRetainedSessions);
  // Retained sessions are oldest first, so the first two were dropped.
  EXPECT_THAT((*sessions)[0].GetDict().FindString("session_id"),
              testing::Pointee(testing::Eq("session_2")));
}

TEST(SessionStreamRecorderFactoryTest, OnSessionRemovedIgnoresUnknownSession) {
  SessionStreamRecorderFactory factory;
  factory.OnSessionRemoved("unknown_session");
  EXPECT_TRUE(factory
                  .GetAllSessions(/*max_events=*/10,
                                  /*max_message_bytes=*/1024)
                  .empty());
}

TEST(SessionStreamRecorderFactoryTest, ExportAllSessionsAsJson) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session1("session_1");
  FakeTransportSession session2("session_2");

  factory.OnSessionRegistered(&session1);
  factory.OnSessionRegistered(&session2);

  SessionStreamRecorder* r1 =
      factory.GetActiveRecorderForTesting(session1.GetSessionId());
  ASSERT_NE(r1, nullptr);
  SessionStreamRecorder* r2 =
      factory.GetActiveRecorderForTesting(session2.GetSessionId());
  ASSERT_NE(r2, nullptr);

  ActuatorDownstreamMessage d1;
  d1.set_session_id("session_1");
  d1.set_sequence_number(1);
  d1.add_typed_payloads()->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  r1->RecordDownstreamMessage(d1, DownstreamDeliveryStatus::kSuccess);

  ActuatorUpstreamMessage u1;
  u1.set_session_id("session_1");
  u1.set_client_sequence_number(2);
  r1->RecordUpstreamMessage(u1, 200);

  ActuatorDownstreamMessage d2;
  d2.set_session_id("session_2");
  d2.set_sequence_number(5);
  d2.add_typed_payloads()->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING);
  r2->RecordDownstreamMessage(d2, DownstreamDeliveryStatus::kSuccess);

  ActuatorUpstreamMessage u2;
  u2.set_session_id("session_2");
  u2.set_client_sequence_number(10);
  u2.set_responding_to_sequence_number(5);
  r2->RecordUpstreamMessage(u2, 200);
  r2->MarkSessionClosed();

  std::string json_str = factory.ExportAllSessionsAsJson();
  ASSERT_FALSE(json_str.empty());

  std::optional<base::DictValue> parsed_json =
      base::JSONReader::ReadDict(json_str, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed_json.has_value());

  const base::ListValue* sessions = parsed_json->FindList("sessions");
  ASSERT_NE(sessions, nullptr);
  ASSERT_EQ(sessions->size(), 2u);

  const base::DictValue* s1_dict = (*sessions)[0].GetIfDict();
  ASSERT_NE(s1_dict, nullptr);
  EXPECT_EQ(*s1_dict->FindString("session_id"), "session_1");
  EXPECT_TRUE(*s1_dict->FindBool("is_active"));
  EXPECT_EQ(s1_dict->FindInt("total_downstream_messages"), 1);
  EXPECT_EQ(s1_dict->FindInt("total_upstream_messages"), 1);
  const base::ListValue* s1_events = s1_dict->FindList("events");
  ASSERT_NE(s1_events, nullptr);
  ASSERT_EQ(s1_events->size(), 2u);

  // The message body is serialized by the generated `ToValue()`, which emits
  // every proto field. int64 fields are emitted as strings, as base::Value has
  // no int64 type.
  const base::DictValue* s1_e0 = (*s1_events)[0].GetIfDict();
  ASSERT_NE(s1_e0, nullptr);
  EXPECT_EQ(*s1_e0->FindString("direction"), "Downstream");
  const base::DictValue* s1_m0 = s1_e0->FindDict("message");
  ASSERT_NE(s1_m0, nullptr);
  EXPECT_EQ(*s1_m0->FindString("session_id"), "session_1");
  EXPECT_EQ(*s1_m0->FindString("sequence_number"), "1");
  const base::ListValue* s1_m0_payloads = s1_m0->FindList("typed_payloads");
  ASSERT_NE(s1_m0_payloads, nullptr);
  ASSERT_EQ(s1_m0_payloads->size(), 1u);
  EXPECT_EQ(*(*s1_m0_payloads)[0].GetIfDict()->FindString("payload_type"),
            "ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND");

  const base::DictValue* s1_e1 = (*s1_events)[1].GetIfDict();
  ASSERT_NE(s1_e1, nullptr);
  EXPECT_EQ(*s1_e1->FindString("direction"), "Upstream");
  const base::DictValue* s1_m1 = s1_e1->FindDict("message");
  ASSERT_NE(s1_m1, nullptr);
  EXPECT_EQ(*s1_m1->FindString("client_sequence_number"), "2");
  // Unset optional fields are omitted entirely.
  EXPECT_EQ(s1_m1->Find("responding_to_sequence_number"), nullptr);

  const base::DictValue* s2_dict = (*sessions)[1].GetIfDict();
  ASSERT_NE(s2_dict, nullptr);
  EXPECT_EQ(*s2_dict->FindString("session_id"), "session_2");
  EXPECT_FALSE(*s2_dict->FindBool("is_active"));
  EXPECT_EQ(s2_dict->FindInt("total_downstream_messages"), 1);
  EXPECT_EQ(s2_dict->FindInt("total_upstream_messages"), 1);
  const base::ListValue* s2_events = s2_dict->FindList("events");
  ASSERT_NE(s2_events, nullptr);
  ASSERT_EQ(s2_events->size(), 2u);
  const base::DictValue* s2_e0 = (*s2_events)[0].GetIfDict();
  ASSERT_NE(s2_e0, nullptr);
  EXPECT_EQ(*s2_e0->FindString("direction"), "Downstream");
  const base::DictValue* s2_m0 = s2_e0->FindDict("message");
  ASSERT_NE(s2_m0, nullptr);
  EXPECT_EQ(*s2_m0->FindString("sequence_number"), "5");
  const base::ListValue* s2_m0_payloads = s2_m0->FindList("typed_payloads");
  ASSERT_NE(s2_m0_payloads, nullptr);
  ASSERT_EQ(s2_m0_payloads->size(), 1u);
  EXPECT_EQ(*(*s2_m0_payloads)[0].GetIfDict()->FindString("payload_type"),
            "ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING");

  const base::DictValue* s2_e1 = (*s2_events)[1].GetIfDict();
  ASSERT_NE(s2_e1, nullptr);
  EXPECT_EQ(*s2_e1->FindString("direction"), "Upstream");
  const base::DictValue* s2_m1 = s2_e1->FindDict("message");
  ASSERT_NE(s2_m1, nullptr);
  EXPECT_EQ(*s2_m1->FindString("client_sequence_number"), "10");
  EXPECT_EQ(*s2_m1->FindString("responding_to_sequence_number"), "5");
}

TEST(SessionStreamRecorderFactoryTest,
     ExportSessionsAsJsonFiltersBySessionIds) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session1("session_1");
  FakeTransportSession session2("session_2");

  factory.OnSessionRegistered(&session1);
  factory.OnSessionRegistered(&session2);
  // Moves session_2 to the retained recorders.
  factory.OnSessionRemoved(session2.GetSessionId());

  const std::vector<std::string> filter_ids = {"session_2"};
  std::string json_str = factory.ExportSessionsAsJson(filter_ids);
  ASSERT_FALSE(json_str.empty());

  std::optional<base::DictValue> parsed_json =
      base::JSONReader::ReadDict(json_str, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed_json.has_value());

  const base::ListValue* sessions = parsed_json->FindList("sessions");
  ASSERT_NE(sessions, nullptr);
  ASSERT_EQ(sessions->size(), 1u);

  const base::DictValue* s2_dict = (*sessions)[0].GetIfDict();
  ASSERT_NE(s2_dict, nullptr);
  EXPECT_EQ(*s2_dict->FindString("session_id"), "session_2");
}

TEST(SessionStreamRecorderFactoryTest, RetainsRemovedSessionsForDump) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_temp");

  factory.OnSessionRegistered(&session);
  ActuatorDownstreamMessage downstream;
  downstream.set_session_id("session_temp");
  downstream.set_sequence_number(42);
  factory.OnDownstreamMessage("session_temp", downstream,
                              DownstreamDeliveryStatus::kSuccess);
  factory.OnSessionRemoved("session_temp");

  EXPECT_EQ(factory.GetActiveRecordersCountForTesting(), 0u);

  base::DictValue dump = factory.ExportAllSessionsAsValue();
  const base::ListValue* sessions = dump.FindList("sessions");
  ASSERT_NE(sessions, nullptr);
  ASSERT_EQ(sessions->size(), 1u);
  const base::DictValue* s_dict = (*sessions)[0].GetIfDict();
  ASSERT_NE(s_dict, nullptr);
  EXPECT_EQ(*s_dict->FindString("session_id"), "session_temp");
  EXPECT_FALSE(*s_dict->FindBool("is_active"));
  EXPECT_NE(s_dict->FindString("start_wall_time"), nullptr);
  EXPECT_TRUE(s_dict->FindDouble("end_time_ticks").has_value());
  EXPECT_TRUE(s_dict->FindDouble("duration_ms").has_value());
  EXPECT_EQ(s_dict->FindInt("total_downstream_messages"), 1);
  EXPECT_EQ(s_dict->FindInt("total_upstream_messages"), 0);
  const base::ListValue* events = s_dict->FindList("events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->size(), 1u);
  const base::DictValue* e0 = (*events)[0].GetIfDict();
  ASSERT_NE(e0, nullptr);
  EXPECT_EQ(*e0->FindString("direction"), "Downstream");
  const base::DictValue* message = e0->FindDict("message");
  ASSERT_NE(message, nullptr);
  EXPECT_EQ(*message->FindString("sequence_number"), "42");
}

TEST(SessionStreamRecorderTest,
     EntryToValueSerializesDownstreamStatusAsString) {
  SessionStreamRecorder recorder("session_123");

  ActuatorDownstreamMessage d1;
  d1.set_session_id("session_123");
  d1.set_sequence_number(1);
  recorder.RecordDownstreamMessage(d1, DownstreamDeliveryStatus::kSuccess);

  ActuatorDownstreamMessage d2;
  d2.set_session_id("session_123");
  d2.set_sequence_number(2);
  recorder.RecordDownstreamMessage(d2, DownstreamDeliveryStatus::kFailure);

  ActuatorDownstreamMessage d3;
  d3.set_session_id("session_123");
  d3.set_sequence_number(3);
  recorder.RecordDownstreamMessage(d3, DownstreamDeliveryStatus::kUnknown);

  ASSERT_EQ(recorder.entries().size(), 3u);

  base::DictValue v1 = recorder.entries()[0].ToValue();
  EXPECT_EQ(*v1.FindString("downstream_status"), "Success");

  base::DictValue v2 = recorder.entries()[1].ToValue();
  EXPECT_EQ(*v2.FindString("downstream_status"), "Failure");

  base::DictValue v3 = recorder.entries()[2].ToValue();
  EXPECT_EQ(*v3.FindString("downstream_status"), "Unknown");
}

// The message body is serialized by the generated `ToValue()` automatically,
// so fields that no caller explicitly asked for still end up in the dump.
TEST(SessionStreamRecorderFactoryTest, ExportIncludesAllProtoFields) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_1");
  factory.OnSessionRegistered(&session);
  SessionStreamRecorder* recorder =
      factory.GetActiveRecorderForTesting(session.GetSessionId());
  ASSERT_NE(recorder, nullptr);

  ActuatorUpstreamMessage upstream;
  upstream.set_session_id("session_1");
  auto* capabilities = upstream.mutable_capabilities();
  capabilities->set_chrome_major_version_number(140);
  capabilities->add_supported_features("feature_a");
  recorder->RecordUpstreamMessage(upstream, 200);

  ActuatorDownstreamMessage downstream;
  downstream.set_session_id("session_1");
  downstream.set_sequence_number(1);
  auto* typed_payload = downstream.add_typed_payloads();
  typed_payload->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  ControlCommand command;
  command.mutable_close_session();
  std::string serialized_command = command.SerializeAsString();
  typed_payload->mutable_proto_payload()->set_type_url(
      "type.googleapis.com/browser_actuator.ControlCommand");
  typed_payload->mutable_proto_payload()->set_value(serialized_command);
  recorder->RecordDownstreamMessage(downstream,
                                    DownstreamDeliveryStatus::kSuccess);

  base::DictValue dump = factory.ExportAllSessionsAsValue();
  const base::ListValue* events =
      (*dump.FindList("sessions"))[0].GetIfDict()->FindList("events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->size(), 2u);

  const base::DictValue* upstream_message =
      (*events)[0].GetIfDict()->FindDict("message");
  ASSERT_NE(upstream_message, nullptr);
  const base::DictValue* dumped_capabilities =
      upstream_message->FindDict("capabilities");
  ASSERT_NE(dumped_capabilities, nullptr);
  EXPECT_EQ(*dumped_capabilities->FindString("chrome_major_version_number"),
            "140");
  const base::ListValue* features =
      dumped_capabilities->FindList("supported_features");
  ASSERT_NE(features, nullptr);
  ASSERT_EQ(features->size(), 1u);
  EXPECT_EQ((*features)[0].GetString(), "feature_a");

  const base::DictValue* downstream_message =
      (*events)[1].GetIfDict()->FindDict("message");
  ASSERT_NE(downstream_message, nullptr);
  const base::ListValue* dumped_payloads =
      downstream_message->FindList("typed_payloads");
  ASSERT_NE(dumped_payloads, nullptr);
  ASSERT_EQ(dumped_payloads->size(), 1u);
  const base::DictValue* dumped_proto_payload =
      (*dumped_payloads)[0].GetIfDict()->FindDict("proto_payload");
  ASSERT_NE(dumped_proto_payload, nullptr);
  EXPECT_EQ(*dumped_proto_payload->FindString("type_url"),
            "type.googleapis.com/browser_actuator.ControlCommand");
  EXPECT_EQ(*dumped_proto_payload->FindString("value"),
            base::Base64Encode(serialized_command));
}

TEST(SessionStreamRecorderFactoryTest, RoutesUpstreamMessageBySessionId) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session1("session_1");
  FakeTransportSession session2("session_2");

  factory.OnSessionRegistered(&session1);
  factory.OnSessionRegistered(&session2);
  SessionStreamRecorder* r1 =
      factory.GetActiveRecorderForTesting(session1.GetSessionId());
  ASSERT_NE(r1, nullptr);
  SessionStreamRecorder* r2 =
      factory.GetActiveRecorderForTesting(session2.GetSessionId());
  ASSERT_NE(r2, nullptr);

  ActuatorUpstreamMessage u2;
  u2.set_session_id("session_2");
  u2.set_client_sequence_number(11);
  factory.OnUpstreamMessage("session_2", u2, 0);
  factory.OnUpstreamMessage("session_2", u2, 200);

  ActuatorUpstreamMessage u1;
  u1.set_session_id("session_1");
  u1.set_client_sequence_number(22);
  factory.OnUpstreamMessage("session_1", u1, 0);
  factory.OnUpstreamMessage("session_1", u1, 200);

  EXPECT_EQ(r1->metadata().total_upstream_messages, 1u);
  ASSERT_EQ(r1->entries().size(), 1u);
  ASSERT_TRUE(std::holds_alternative<ActuatorUpstreamMessage>(
      r1->entries()[0].message));
  EXPECT_EQ(std::get<ActuatorUpstreamMessage>(r1->entries()[0].message)
                .client_sequence_number(),
            22);

  EXPECT_EQ(r2->metadata().total_upstream_messages, 1u);
  ASSERT_EQ(r2->entries().size(), 1u);
  ASSERT_TRUE(std::holds_alternative<ActuatorUpstreamMessage>(
      r2->entries()[0].message));
  EXPECT_EQ(std::get<ActuatorUpstreamMessage>(r2->entries()[0].message)
                .client_sequence_number(),
            11);
}

TEST(SessionStreamRecorderFactoryTest,
     DropsUpstreamMessageForUnknownOrClosedSession) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session1("session_1");

  ActuatorUpstreamMessage u_unknown;
  u_unknown.set_session_id("unknown_session");
  u_unknown.set_client_sequence_number(1);
  factory.OnUpstreamMessage("unknown_session", u_unknown, 0);
  factory.OnUpstreamMessage("unknown_session", u_unknown, 200);

  // Removing session_1 closes it and moves it to the retained recorders.
  factory.OnSessionRegistered(&session1);
  factory.OnSessionRemoved("session_1");

  ActuatorUpstreamMessage u_closed;
  u_closed.set_session_id("session_1");
  u_closed.set_client_sequence_number(2);
  factory.OnUpstreamMessage("session_1", u_closed, 0);
  factory.OnUpstreamMessage("session_1", u_closed, 200);

  base::DictValue dump = factory.ExportAllSessionsAsValue();
  const base::ListValue* sessions = dump.FindList("sessions");
  ASSERT_NE(sessions, nullptr);
  ASSERT_EQ(sessions->size(), 1u);
  const base::DictValue* s1_dict = (*sessions)[0].GetIfDict();
  ASSERT_NE(s1_dict, nullptr);
  ASSERT_NE(s1_dict->FindString("session_id"), nullptr);
  EXPECT_EQ(*s1_dict->FindString("session_id"), "session_1");
  EXPECT_EQ(s1_dict->FindInt("total_upstream_messages"), 0);
  const base::ListValue* events = s1_dict->FindList("events");
  ASSERT_NE(events, nullptr);
  EXPECT_TRUE(events->empty());
}

TEST(SessionStreamRecorderFactoryTest, GetAllSessionsIsEmptyByDefault) {
  SessionStreamRecorderFactory factory;
  EXPECT_TRUE(factory
                  .GetAllSessions(/*max_events=*/10,
                                  /*max_message_bytes=*/1024)
                  .empty());
}

TEST(SessionStreamRecorderFactoryTest,
     GetAllSessionsIncludesActiveAndRetained) {
  SessionStreamRecorderFactory factory;

  FakeTransportSession session_active("session_active");
  factory.OnSessionRegistered(&session_active);

  FakeTransportSession session_retained("session_retained");
  factory.OnSessionRegistered(&session_retained);
  // Moves the session to the retained recorders.
  factory.OnSessionRemoved(session_retained.GetSessionId());

  std::vector<SessionSnapshot> sessions =
      factory.GetAllSessions(/*max_events=*/10, /*max_message_bytes=*/1024);
  ASSERT_EQ(sessions.size(), 2u);

  bool found_active = false;
  bool found_retained = false;
  for (const auto& s : sessions) {
    if (s.session_id == "session_active") {
      found_active = true;
      EXPECT_FALSE(s.end_wall_time.has_value());
    } else if (s.session_id == "session_retained") {
      found_retained = true;
      EXPECT_TRUE(s.end_wall_time.has_value());
    }
  }
  EXPECT_TRUE(found_active);
  EXPECT_TRUE(found_retained);
}

TEST(SessionStreamRecorderFactoryTest, GetAllSessionsCapsEvents) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_1");
  factory.OnSessionRegistered(&session);
  SessionStreamRecorder* recorder =
      factory.GetActiveRecorderForTesting(session.GetSessionId());
  ASSERT_NE(recorder, nullptr);

  for (int i = 0; i < 10; ++i) {
    ActuatorDownstreamMessage msg;
    msg.set_session_id("session_1");
    msg.set_sequence_number(i);
    recorder->RecordDownstreamMessage(std::move(msg),
                                      DownstreamDeliveryStatus::kSuccess);
  }

  std::vector<SessionSnapshot> sessions =
      factory.GetAllSessions(/*max_events=*/3, /*max_message_bytes=*/1024);
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_EQ(sessions[0].total_events, 10u);
  EXPECT_EQ(sessions[0].events.size(), 3u);
}

TEST(SessionStreamRecorderFactoryTest, GetAllSessionsKeepsNewestEvents) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_1");
  factory.OnSessionRegistered(&session);
  SessionStreamRecorder* recorder =
      factory.GetActiveRecorderForTesting(session.GetSessionId());
  ASSERT_NE(recorder, nullptr);

  for (int i = 0; i < 10; ++i) {
    ActuatorDownstreamMessage msg;
    msg.set_session_id("session_1");
    msg.set_sequence_number(i);
    recorder->RecordDownstreamMessage(std::move(msg),
                                      DownstreamDeliveryStatus::kSuccess);
  }

  std::vector<SessionSnapshot> sessions =
      factory.GetAllSessions(/*max_events=*/3, /*max_message_bytes=*/1024);
  ASSERT_EQ(sessions.size(), 1u);
  ASSERT_EQ(sessions[0].events.size(), 3u);

  // The kept events should be the newest 3: sequence numbers 7, 8, 9, in order.
  for (size_t i = 0; i < 3; ++i) {
    int expected_seq = 7 + i;
    std::optional<base::DictValue> parsed = base::JSONReader::ReadDict(
        sessions[0].events[i].message, base::JSON_PARSE_RFC);
    ASSERT_TRUE(parsed.has_value());
    const std::string* seq = parsed->FindString("sequence_number");
    ASSERT_NE(seq, nullptr);
    EXPECT_EQ(*seq, base::NumberToString(expected_seq));
  }
}

TEST(SessionStreamRecorderFactoryTest, GetAllSessionsTruncatesLargeMessages) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_1");
  factory.OnSessionRegistered(&session);
  SessionStreamRecorder* recorder =
      factory.GetActiveRecorderForTesting(session.GetSessionId());
  ASSERT_NE(recorder, nullptr);

  ActuatorDownstreamMessage msg;
  msg.set_session_id("session_1");
  auto* typed_payload = msg.add_typed_payloads();
  typed_payload->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  typed_payload->mutable_proto_payload()->set_value(std::string(500, 'x'));
  recorder->RecordDownstreamMessage(std::move(msg),
                                    DownstreamDeliveryStatus::kSuccess);

  std::vector<SessionSnapshot> sessions =
      factory.GetAllSessions(/*max_events=*/10, /*max_message_bytes=*/50);
  ASSERT_EQ(sessions.size(), 1u);
  ASSERT_EQ(sessions[0].events.size(), 1u);
  EXPECT_TRUE(sessions[0].events[0].message_truncated);
  EXPECT_EQ(sessions[0].events[0].message.size(), 50u);
}

TEST(SessionStreamRecorderFactoryTest, GetAllSessionsSortsNewestFirst) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  SessionStreamRecorderFactory factory;

  FakeTransportSession session_older("session_older");
  factory.OnSessionRegistered(&session_older);

  task_environment.FastForwardBy(base::Seconds(10));

  FakeTransportSession session_newer("session_newer");
  factory.OnSessionRegistered(&session_newer);

  std::vector<SessionSnapshot> sessions =
      factory.GetAllSessions(/*max_events=*/10, /*max_message_bytes=*/1024);
  ASSERT_EQ(sessions.size(), 2u);
  EXPECT_EQ(sessions[0].session_id, "session_newer");
  EXPECT_EQ(sessions[1].session_id, "session_older");
  EXPECT_GT(sessions[0].start_wall_time, sessions[1].start_wall_time);
}

TEST(SessionStreamRecorderFactoryTest,
     GetAllSessionsConvertsTimestampsToWallTime) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_1");
  factory.OnSessionRegistered(&session);
  SessionStreamRecorder* recorder =
      factory.GetActiveRecorderForTesting(session.GetSessionId());
  ASSERT_NE(recorder, nullptr);

  task_environment.FastForwardBy(base::Seconds(5));

  ActuatorDownstreamMessage msg;
  msg.set_session_id("session_1");
  recorder->RecordDownstreamMessage(std::move(msg),
                                    DownstreamDeliveryStatus::kSuccess);

  task_environment.FastForwardBy(base::Seconds(5));
  factory.OnSessionRemoved(session.GetSessionId());  // Closes the session.

  std::vector<SessionSnapshot> sessions =
      factory.GetAllSessions(/*max_events=*/10, /*max_message_bytes=*/1024);
  ASSERT_EQ(sessions.size(), 1u);
  const auto& s = sessions[0];
  ASSERT_TRUE(s.end_wall_time.has_value());
  ASSERT_EQ(s.events.size(), 1u);

  EXPECT_EQ(s.events[0].timestamp, s.start_wall_time + base::Seconds(5));
  EXPECT_EQ(*s.end_wall_time - s.start_wall_time, base::Seconds(10));
}

TEST(SessionStreamRecorderFactoryTest, GetAllSessionsReportsPayloadTypes) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_1");
  factory.OnSessionRegistered(&session);
  SessionStreamRecorder* recorder =
      factory.GetActiveRecorderForTesting(session.GetSessionId());
  ASSERT_NE(recorder, nullptr);

  ActuatorDownstreamMessage downstream;
  downstream.set_session_id("session_1");
  auto* p1 = downstream.add_typed_payloads();
  p1->set_payload_type(ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  auto* p2 = downstream.add_typed_payloads();
  p2->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING);
  auto* p3 = downstream.add_typed_payloads();
  p3->set_payload_type(ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_ACTUATION);
  auto* p4 = downstream.add_typed_payloads();
  p4->set_payload_type(ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_TASK_INGRESS);
  auto* p5 = downstream.add_typed_payloads();
  p5->set_payload_type(ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_PROBE);
  auto* p6 = downstream.add_typed_payloads();
  p6->set_payload_type(ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_UNSPECIFIED);
  recorder->RecordDownstreamMessage(std::move(downstream),
                                    DownstreamDeliveryStatus::kSuccess);

  ActuatorUpstreamMessage upstream;
  upstream.set_session_id("session_1");
  auto* up1 = upstream.add_typed_payloads();
  up1->set_payload_type(ACTUATOR_UPSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  auto* up2 = upstream.add_typed_payloads();
  up2->set_payload_type(ACTUATOR_UPSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING);
  auto* up3 = upstream.add_typed_payloads();
  up3->set_payload_type(ACTUATOR_UPSTREAM_PAYLOAD_TYPE_ACTUATION);
  auto* up4 = upstream.add_typed_payloads();
  up4->set_payload_type(ACTUATOR_UPSTREAM_PAYLOAD_TYPE_TASK_INGRESS);
  auto* up5 = upstream.add_typed_payloads();
  up5->set_payload_type(ACTUATOR_UPSTREAM_PAYLOAD_TYPE_PROBE);
  auto* up6 = upstream.add_typed_payloads();
  up6->set_payload_type(ACTUATOR_UPSTREAM_PAYLOAD_TYPE_UNSPECIFIED);
  recorder->RecordUpstreamMessage(std::move(upstream), 200);

  std::vector<SessionSnapshot> sessions =
      factory.GetAllSessions(/*max_events=*/10, /*max_message_bytes=*/1024);
  ASSERT_EQ(sessions.size(), 1u);
  ASSERT_EQ(sessions[0].events.size(), 2u);

  EXPECT_TRUE(sessions[0].events[0].is_downstream);
  EXPECT_THAT(
      sessions[0].events[0].payload_types,
      ::testing::ElementsAre("Control", "ExperimentalTriggering", "Actuation",
                             "TaskIngress", "Probe", "Unspecified"));

  EXPECT_FALSE(sessions[0].events[1].is_downstream);
  EXPECT_THAT(
      sessions[0].events[1].payload_types,
      ::testing::ElementsAre("Control", "ExperimentalTriggering", "Actuation",
                             "TaskIngress", "Probe", "Unspecified"));
}

TEST(SessionStreamRecorderFactoryTest, GetAllSessionsIncludesUpstreamEvents) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_1");
  factory.OnSessionRegistered(&session);

  ActuatorUpstreamMessage upstream;
  upstream.set_session_id("session_1");
  upstream.set_client_sequence_number(42);
  factory.OnUpstreamMessage("session_1", upstream, 0);
  factory.OnUpstreamMessage("session_1", upstream, 200);

  std::vector<SessionSnapshot> sessions =
      factory.GetAllSessions(/*max_events=*/10, /*max_message_bytes=*/1024);
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_EQ(sessions[0].total_upstream_messages, 1u);
  ASSERT_EQ(sessions[0].events.size(), 1u);
  EXPECT_FALSE(sessions[0].events[0].is_downstream);
  EXPECT_EQ(sessions[0].events[0].http_response_code, 200);
}

TEST(SessionStreamRecorderTest,
     PopulatesMissingCanonicalTypeUrlOnDownstreamPayloads) {
  SessionStreamRecorder recorder("session_123");

  ActuatorDownstreamMessage downstream;
  downstream.set_session_id("session_123");
  downstream.set_sequence_number(7);
  ActuatorDownstreamTypedPayload* control_payload =
      downstream.add_typed_payloads();
  control_payload->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  control_payload->mutable_proto_payload()->set_value("cmd");
  ActuatorDownstreamTypedPayload* unspecified_payload =
      downstream.add_typed_payloads();
  unspecified_payload->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_UNSPECIFIED);
  unspecified_payload->mutable_proto_payload()->set_value("unknown");
  ActuatorDownstreamTypedPayload* missing_proto_payload =
      downstream.add_typed_payloads();
  missing_proto_payload->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);

  recorder.RecordDownstreamMessage(std::move(downstream),
                                   DownstreamDeliveryStatus::kSuccess);

  ASSERT_EQ(recorder.entries().size(), 1u);
  const auto& recorded =
      std::get<ActuatorDownstreamMessage>(recorder.entries()[0].message);
  ASSERT_EQ(recorded.typed_payloads_size(), 3);
  EXPECT_EQ(recorded.typed_payloads(0).proto_payload().type_url(),
            "type.googleapis.com/browser_actuator.ControlCommand");
  EXPECT_TRUE(recorded.typed_payloads(1).proto_payload().type_url().empty());
  EXPECT_FALSE(recorded.typed_payloads(2).has_proto_payload());
}

TEST(SessionStreamRecorderTest,
     CapsEntriesPerSessionWhilePreservingTotalMessageCounts) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_bounded");
  factory.OnSessionRegistered(&session);
  SessionStreamRecorder* recorder =
      factory.GetActiveRecorderForTesting(session.GetSessionId());
  ASSERT_NE(recorder, nullptr);

  for (size_t i = 0; i < SessionStreamRecorder::kMaxEntriesPerSession + 10;
       ++i) {
    ActuatorDownstreamMessage msg;
    msg.set_session_id("session_bounded");
    msg.set_sequence_number(static_cast<int64_t>(i + 1));
    recorder->RecordDownstreamMessage(std::move(msg),
                                      DownstreamDeliveryStatus::kSuccess);
  }

  EXPECT_EQ(recorder->metadata().total_downstream_messages,
            SessionStreamRecorder::kMaxEntriesPerSession + 10);
  ASSERT_EQ(recorder->entries().size(),
            SessionStreamRecorder::kMaxEntriesPerSession);
  EXPECT_EQ(
      std::get<ActuatorDownstreamMessage>(recorder->entries().front().message)
          .sequence_number(),
      11);
  EXPECT_EQ(
      std::get<ActuatorDownstreamMessage>(recorder->entries().back().message)
          .sequence_number(),
      static_cast<int64_t>(SessionStreamRecorder::kMaxEntriesPerSession + 10));

  for (size_t i = 0; i < 5; ++i) {
    ActuatorUpstreamMessage upstream;
    upstream.set_session_id("session_bounded");
    upstream.set_client_sequence_number(static_cast<int64_t>(i + 1));
    recorder->RecordUpstreamMessage(std::move(upstream), 200);
  }

  EXPECT_EQ(recorder->metadata().total_upstream_messages, 5u);
  ASSERT_EQ(recorder->entries().size(),
            SessionStreamRecorder::kMaxEntriesPerSession);
  EXPECT_EQ(
      std::get<ActuatorDownstreamMessage>(recorder->entries().front().message)
          .sequence_number(),
      16);
  EXPECT_EQ(
      std::get<ActuatorUpstreamMessage>(recorder->entries().back().message)
          .client_sequence_number(),
      5);

  std::vector<SessionSnapshot> sessions =
      factory.GetAllSessions(/*max_events=*/500, /*max_message_bytes=*/1024);
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_EQ(sessions[0].events.size(),
            SessionStreamRecorder::kMaxEntriesPerSession);
  EXPECT_EQ(sessions[0].total_downstream_messages,
            SessionStreamRecorder::kMaxEntriesPerSession + 10);
  EXPECT_EQ(sessions[0].total_upstream_messages, 5u);
  EXPECT_EQ(sessions[0].total_events, sessions[0].total_downstream_messages +
                                          sessions[0].total_upstream_messages);
}

TEST(SessionStreamRecorderFactoryTest,
     ScrubsLargeDownstreamAndUpstreamProbePayloadsOnIngestAndRetainsSummary) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_probe");
  factory.OnSessionRegistered(&session);
  SessionStreamRecorder* recorder =
      factory.GetActiveRecorderForTesting(session.GetSessionId());
  ASSERT_NE(recorder, nullptr);

  struct PayloadSizeObserver : public SessionStreamRecorder::Observer {
    void OnDownstreamMessage(base::TimeTicks /*timestamp*/,
                             const ActuatorDownstreamMessage& msg,
                             DownstreamDeliveryStatus /*status*/) override {
      downstream_bytes = msg.typed_payloads(0).proto_payload().value().size();
    }
    void OnUpstreamMessage(base::TimeTicks /*timestamp*/,
                           const ActuatorUpstreamMessage& msg,
                           int /*http_response_code*/) override {
      upstream_bytes = msg.typed_payloads(0).proto_payload().value().size();
    }

    size_t downstream_bytes = 0;
    size_t upstream_bytes = 0;
  } observer;
  recorder->AddObserver(&observer);

  const ProbeResponseSpecInput specs[] = {
      {.payload_size_bytes = 1'000'000, .delay_ms = 100},
  };
  base::expected<TransportProbe::ProbeRequest, std::string> probe_request =
      BuildProbeRequest("mem-probe", 1'000'000, specs);
  ASSERT_TRUE(probe_request.has_value()) << probe_request.error();
  TransportProbe downstream_probe;
  *downstream_probe.mutable_request() = *std::move(probe_request);

  ActuatorDownstreamMessage downstream;
  downstream.set_session_id("session_probe");
  downstream.set_sequence_number(1);
  ActuatorDownstreamTypedPayload* downstream_typed =
      downstream.add_typed_payloads();
  downstream_typed->set_payload_type(ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_PROBE);
  downstream_typed->mutable_proto_payload()->set_type_url(
      std::string(ExpectedTypeUrl(PayloadType::kProbe)));
  downstream_typed->mutable_proto_payload()->set_value(
      downstream_probe.SerializeAsString());
  factory.OnDownstreamMessage("session_probe", downstream,
                              DownstreamDeliveryStatus::kSuccess);

  base::expected<std::string, std::string> response_payload =
      GenerateProbeResponsePayload("mem-probe", 0, 1, 1'000'000);
  ASSERT_TRUE(response_payload.has_value()) << response_payload.error();
  TransportProbe upstream_probe;
  TransportProbe::ProbeResponse* probe_response =
      upstream_probe.mutable_response();
  probe_response->set_probe_id("mem-probe");
  probe_response->set_response_index(0);
  probe_response->set_total_responses(1);
  probe_response->set_payload(*std::move(response_payload));

  ActuatorUpstreamMessage upstream;
  upstream.set_session_id("session_probe");
  upstream.set_client_sequence_number(1);
  ActuatorUpstreamTypedPayload* upstream_typed = upstream.add_typed_payloads();
  upstream_typed->set_payload_type(ACTUATOR_UPSTREAM_PAYLOAD_TYPE_PROBE);
  upstream_typed->mutable_proto_payload()->set_type_url(
      std::string(ExpectedTypeUrl(PayloadType::kProbe)));
  upstream_typed->mutable_proto_payload()->set_value(
      upstream_probe.SerializeAsString());
  factory.OnUpstreamMessage("session_probe", upstream, 0);

  recorder->RemoveObserver(&observer);

  // Observers and in-memory entries must hold the scrubbed marker (< 256 B
  // size and < 512 B heap capacity) rather than retaining 1 MB buffers.
  EXPECT_LT(observer.downstream_bytes, 256u);
  EXPECT_LT(observer.upstream_bytes, 256u);
  ASSERT_EQ(recorder->entries().size(), 2u);
  const std::string& recorded_down_value =
      std::get<ActuatorDownstreamMessage>(recorder->entries()[0].message)
          .typed_payloads(0)
          .proto_payload()
          .value();
  const std::string& recorded_up_value =
      std::get<ActuatorUpstreamMessage>(recorder->entries()[1].message)
          .typed_payloads(0)
          .proto_payload()
          .value();
  EXPECT_LT(recorded_down_value.size(), 256u);
  EXPECT_LT(recorded_down_value.capacity(), 512u);
  EXPECT_LT(recorded_up_value.size(), 256u);
  EXPECT_LT(recorded_up_value.capacity(), 512u);

  std::vector<SessionSnapshot> sessions =
      factory.GetAllSessions(/*max_events=*/10, /*max_message_bytes=*/8192);
  ASSERT_EQ(sessions.size(), 1u);
  ASSERT_EQ(sessions[0].events.size(), 2u);
  EXPECT_FALSE(sessions[0].events[0].message_truncated);
  EXPECT_FALSE(sessions[0].events[1].message_truncated);

  std::optional<base::DictValue> downstream_json = base::JSONReader::ReadDict(
      sessions[0].events[0].message, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(downstream_json.has_value());
  const base::DictValue* req_dict =
      downstream_json->FindList("typed_payloads")
          ->front()
          .GetDict()
          .FindDictByDottedPath("decoded_transport_probe.request");
  ASSERT_NE(req_dict, nullptr);
  EXPECT_EQ(*req_dict->FindString("probe_id"), "mem-probe");
  const std::string* scrubbed_req_payload = req_dict->FindString("payload");
  ASSERT_NE(scrubbed_req_payload, nullptr);
  EXPECT_THAT(*scrubbed_req_payload,
              ::testing::StartsWith(
                  "[<1000000 synthetic bytes elided; head=PROBE:mem-probe:"));
  EXPECT_EQ(req_dict->FindList("response_specs")
                ->front()
                .GetDict()
                .FindInt("delay_ms"),
            100);

  std::optional<base::DictValue> upstream_json = base::JSONReader::ReadDict(
      sessions[0].events[1].message, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(upstream_json.has_value());
  const base::DictValue* resp_dict =
      upstream_json->FindList("typed_payloads")
          ->front()
          .GetDict()
          .FindDictByDottedPath("decoded_transport_probe.response");
  ASSERT_NE(resp_dict, nullptr);
  EXPECT_EQ(*resp_dict->FindString("probe_id"), "mem-probe");
  EXPECT_EQ(resp_dict->FindInt("response_index"), 0);
  EXPECT_EQ(resp_dict->FindInt("total_responses"), 1);
  const std::string* scrubbed_resp_payload = resp_dict->FindString("payload");
  ASSERT_NE(scrubbed_resp_payload, nullptr);
  EXPECT_THAT(*scrubbed_resp_payload,
              ::testing::StartsWith(
                  "[<1000000 synthetic bytes elided; head=PROBE:mem-probe:"));

  // Move the session to `retained_recorders_` and verify JSON export retains
  // both `proto_payload.value` and `decoded_transport_probe`.
  factory.OnSessionRemoved(session.GetSessionId());
  base::DictValue exported = factory.ExportAllSessionsAsValue();
  const base::ListValue* exported_sessions = exported.FindList("sessions");
  ASSERT_NE(exported_sessions, nullptr);
  ASSERT_EQ(exported_sessions->size(), 1u);
  EXPECT_EQ(exported_sessions->front().GetDict().FindBool("is_active"), false);
  const base::ListValue* exported_events =
      exported_sessions->front().GetDict().FindList("events");
  ASSERT_NE(exported_events, nullptr);
  ASSERT_EQ(exported_events->size(), 2u);
  const base::DictValue& exported_payload = exported_events->front()
                                                .GetDict()
                                                .FindDict("message")
                                                ->FindList("typed_payloads")
                                                ->front()
                                                .GetDict();
  EXPECT_NE(exported_payload.FindByDottedPath("proto_payload.value"), nullptr);
  EXPECT_NE(
      exported_payload.FindByDottedPath("decoded_transport_probe.request"),
      nullptr);
}

TEST(SessionStreamRecorderFactoryTest,
     PreservesSmallProbePayloadsAtOrBelowThresholdVerbatim) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_small");
  factory.OnSessionRegistered(&session);
  SessionStreamRecorder* recorder =
      factory.GetActiveRecorderForTesting(session.GetSessionId());
  ASSERT_NE(recorder, nullptr);

  ControlCommand start_command;
  start_command.mutable_start_session();

  base::expected<TransportProbe::ProbeRequest, std::string> req =
      BuildProbeRequest("p-42", /*request_payload_size_bytes=*/128);
  ASSERT_TRUE(req.has_value()) << req.error();
  const std::string expected_req_payload = req->payload();
  ASSERT_EQ(expected_req_payload.size(), 128u);

  TransportProbe downstream_probe;
  *downstream_probe.mutable_request() = *std::move(req);
  ActuatorDownstreamMessage downstream;
  downstream.set_session_id("session_small");
  ActuatorDownstreamTypedPayload* down_control =
      downstream.add_typed_payloads();
  down_control->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  down_control->mutable_proto_payload()->set_type_url(
      std::string(ExpectedTypeUrl(PayloadType::kControl)));
  down_control->mutable_proto_payload()->set_value(
      start_command.SerializeAsString());
  ActuatorDownstreamTypedPayload* down_typed = downstream.add_typed_payloads();
  down_typed->set_payload_type(ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_PROBE);
  down_typed->mutable_proto_payload()->set_type_url(
      std::string(ExpectedTypeUrl(PayloadType::kProbe)));
  down_typed->mutable_proto_payload()->set_value(
      downstream_probe.SerializeAsString());
  recorder->RecordDownstreamMessage(std::move(downstream),
                                    DownstreamDeliveryStatus::kSuccess);

  ControlCommand ack_command;
  ack_command.mutable_start_session_ack();

  base::expected<std::string, std::string> expected_resp_payload =
      GenerateProbeResponsePayload("p-42", 0, 2, 128);
  ASSERT_TRUE(expected_resp_payload.has_value())
      << expected_resp_payload.error();
  TransportProbe upstream_probe;
  upstream_probe.mutable_response()->set_probe_id("p-42");
  upstream_probe.mutable_response()->set_response_index(0);
  upstream_probe.mutable_response()->set_total_responses(2);
  upstream_probe.mutable_response()->set_payload(*expected_resp_payload);
  ActuatorUpstreamMessage upstream;
  upstream.set_session_id("session_small");
  ActuatorUpstreamTypedPayload* up_control = upstream.add_typed_payloads();
  up_control->set_payload_type(ACTUATOR_UPSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  up_control->mutable_proto_payload()->set_type_url(
      std::string(ExpectedTypeUrl(PayloadType::kControl)));
  up_control->mutable_proto_payload()->set_value(
      ack_command.SerializeAsString());
  ActuatorUpstreamTypedPayload* up_typed = upstream.add_typed_payloads();
  up_typed->set_payload_type(ACTUATOR_UPSTREAM_PAYLOAD_TYPE_PROBE);
  up_typed->mutable_proto_payload()->set_value(
      upstream_probe.SerializeAsString());
  factory.OnUpstreamMessage("session_small", upstream, 0);

  std::vector<SessionSnapshot> sessions =
      factory.GetAllSessions(/*max_events=*/10, /*max_message_bytes=*/8192);
  ASSERT_EQ(sessions.size(), 1u);
  ASSERT_EQ(sessions[0].events.size(), 2u);

  std::optional<base::DictValue> down_json = base::JSONReader::ReadDict(
      sessions[0].events[0].message, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(down_json.has_value());
  const base::ListValue* down_payloads = down_json->FindList("typed_payloads");
  ASSERT_NE(down_payloads, nullptr);
  ASSERT_EQ(down_payloads->size(), 2u);
  EXPECT_NE((*down_payloads)[0].GetDict().FindDictByDottedPath(
                "decoded_control_command.start_session"),
            nullptr);
  EXPECT_EQ(*(*down_payloads)[1].GetDict().FindStringByDottedPath(
                "decoded_transport_probe.request.payload"),
            expected_req_payload);

  std::optional<base::DictValue> up_json = base::JSONReader::ReadDict(
      sessions[0].events[1].message, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(up_json.has_value());
  const base::ListValue* up_payloads = up_json->FindList("typed_payloads");
  ASSERT_NE(up_payloads, nullptr);
  ASSERT_EQ(up_payloads->size(), 2u);
  EXPECT_NE((*up_payloads)[0].GetDict().FindDictByDottedPath(
                "decoded_control_command.start_session_ack"),
            nullptr);
  EXPECT_EQ(*(*up_payloads)[1].GetDict().FindStringByDottedPath(
                "decoded_transport_probe.response.payload"),
            *expected_resp_payload);
}

TEST(SessionStreamRecorderFactoryTest,
     DoesNotScrubProbePayloadWithMismatchedTypeUrl) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_mismatch");
  factory.OnSessionRegistered(&session);
  SessionStreamRecorder* recorder =
      factory.GetActiveRecorderForTesting(session.GetSessionId());
  ASSERT_NE(recorder, nullptr);

  base::expected<TransportProbe::ProbeRequest, std::string> req =
      BuildProbeRequest("p-42", /*request_payload_size_bytes=*/256);
  ASSERT_TRUE(req.has_value()) << req.error();
  TransportProbe probe;
  *probe.mutable_request() = *std::move(req);
  const std::string raw_serialized = probe.SerializeAsString();

  ActuatorDownstreamMessage downstream;
  downstream.set_session_id("session_mismatch");
  ActuatorDownstreamTypedPayload* typed = downstream.add_typed_payloads();
  typed->set_payload_type(ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_PROBE);
  typed->mutable_proto_payload()->set_type_url(
      "type.googleapis.com/example.OtherMessage");
  typed->mutable_proto_payload()->set_value(raw_serialized);
  recorder->RecordDownstreamMessage(std::move(downstream),
                                    DownstreamDeliveryStatus::kSuccess);

  ASSERT_EQ(recorder->entries().size(), 1u);
  EXPECT_EQ(std::get<ActuatorDownstreamMessage>(recorder->entries()[0].message)
                .typed_payloads(0)
                .proto_payload()
                .value(),
            raw_serialized);

  base::DictValue exported = factory.ExportAllSessionsAsValue();
  const base::ListValue* exported_sessions = exported.FindList("sessions");
  ASSERT_NE(exported_sessions, nullptr);
  ASSERT_EQ(exported_sessions->size(), 1u);
  const base::ListValue* exported_events =
      exported_sessions->front().GetDict().FindList("events");
  ASSERT_NE(exported_events, nullptr);
  ASSERT_EQ(exported_events->size(), 1u);
  const base::DictValue& exported_payload = exported_events->front()
                                                .GetDict()
                                                .FindDict("message")
                                                ->FindList("typed_payloads")
                                                ->front()
                                                .GetDict();
  EXPECT_EQ(exported_payload.Find("decoded_control_command"), nullptr);
  EXPECT_EQ(exported_payload.Find("decoded_transport_probe"), nullptr);

  // Also verify that malformed serialized wire bytes ("\xff", a truncated
  // varint tag) with a valid/empty `type_url` for both `kControl` and `kProbe`
  // record the raw entry while omitting `decoded_control_command` and
  // `decoded_transport_probe`.
  ActuatorDownstreamMessage malformed;
  malformed.set_session_id("session_mismatch");
  ActuatorDownstreamTypedPayload* malformed_control =
      malformed.add_typed_payloads();
  malformed_control->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  malformed_control->mutable_proto_payload()->set_value("\xff");
  ActuatorDownstreamTypedPayload* malformed_probe =
      malformed.add_typed_payloads();
  malformed_probe->set_payload_type(ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_PROBE);
  malformed_probe->mutable_proto_payload()->set_value("\xff");
  recorder->RecordDownstreamMessage(std::move(malformed),
                                    DownstreamDeliveryStatus::kSuccess);

  ASSERT_EQ(recorder->entries().size(), 2u);
  base::DictValue exported_after = factory.ExportAllSessionsAsValue();
  const base::ListValue* malformed_payloads =
      exported_after.FindList("sessions")
          ->front()
          .GetDict()
          .FindList("events")
          ->back()
          .GetDict()
          .FindDict("message")
          ->FindList("typed_payloads");
  ASSERT_NE(malformed_payloads, nullptr);
  ASSERT_EQ(malformed_payloads->size(), 2u);
  EXPECT_EQ((*malformed_payloads)[0].GetDict().Find("decoded_control_command"),
            nullptr);
  EXPECT_EQ((*malformed_payloads)[1].GetDict().Find("decoded_transport_probe"),
            nullptr);
}

TEST(
    SessionStreamRecorderFactoryTest,
    ScrubsMultiByteUtf8ProbePayloadWithoutSplittingCodepointAtPreviewBoundary) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_utf8");
  factory.OnSessionRegistered(&session);
  SessionStreamRecorder* recorder =
      factory.GetActiveRecorderForTesting(session.GetSessionId());
  ASSERT_NE(recorder, nullptr);

  // Place a 3-byte UTF-8 Euro sign ("\xE2\x82\xAC") across byte offset 40
  // (preview limit, bytes 39..41) in a 142-byte (> 128) payload. Naive byte
  // slicing at 40 would leave a lone 0xE2 lead byte and fail proto3 UTF-8
  // validation when re-parsing `TransportProbe`.
  const std::string utf8_payload =
      std::string(39, 'A') + "\xE2\x82\xAC" + std::string(100, 'B');
  ASSERT_EQ(utf8_payload.size(), 142u);

  TransportProbe downstream_probe;
  downstream_probe.mutable_request()->set_probe_id("utf8-probe");
  downstream_probe.mutable_request()->set_payload(utf8_payload);
  ActuatorDownstreamMessage downstream;
  downstream.set_session_id("session_utf8");
  ActuatorDownstreamTypedPayload* down_typed = downstream.add_typed_payloads();
  down_typed->set_payload_type(ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_PROBE);
  down_typed->mutable_proto_payload()->set_value(
      downstream_probe.SerializeAsString());
  factory.OnDownstreamMessage("session_utf8", downstream,
                              DownstreamDeliveryStatus::kSuccess);

  TransportProbe upstream_probe;
  upstream_probe.mutable_response()->set_probe_id("utf8-probe");
  upstream_probe.mutable_response()->set_response_index(0);
  upstream_probe.mutable_response()->set_total_responses(1);
  upstream_probe.mutable_response()->set_payload(utf8_payload);
  ActuatorUpstreamMessage upstream;
  upstream.set_session_id("session_utf8");
  ActuatorUpstreamTypedPayload* up_typed = upstream.add_typed_payloads();
  up_typed->set_payload_type(ACTUATOR_UPSTREAM_PAYLOAD_TYPE_PROBE);
  up_typed->mutable_proto_payload()->set_value(
      upstream_probe.SerializeAsString());
  factory.OnUpstreamMessage("session_utf8", upstream, 0);

  // Also record a malformed `TransportProbe` wire payload to verify invalid
  // protobuf bytes are left untouched without omitting the event.
  ActuatorDownstreamMessage malformed;
  malformed.set_session_id("session_utf8");
  ActuatorDownstreamTypedPayload* malformed_typed =
      malformed.add_typed_payloads();
  malformed_typed->set_payload_type(ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_PROBE);
  malformed_typed->mutable_proto_payload()->set_value(
      std::string("\xFF\xFF\xFF", 3));
  factory.OnDownstreamMessage("session_utf8", malformed,
                              DownstreamDeliveryStatus::kSuccess);

  const std::string expected_scrubbed =
      "[<142 synthetic bytes elided; head=" + std::string(39, 'A') + "...>]";

  std::vector<SessionSnapshot> sessions =
      factory.GetAllSessions(/*max_events=*/10, /*max_message_bytes=*/8192);
  ASSERT_EQ(sessions.size(), 1u);
  ASSERT_EQ(sessions[0].events.size(), 3u);

  std::optional<base::DictValue> down_json = base::JSONReader::ReadDict(
      sessions[0].events[0].message, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(down_json.has_value());
  const std::string* down_scrubbed =
      down_json->FindList("typed_payloads")
          ->front()
          .GetDict()
          .FindStringByDottedPath("decoded_transport_probe.request.payload");
  ASSERT_NE(down_scrubbed, nullptr);
  EXPECT_EQ(*down_scrubbed, expected_scrubbed);

  std::optional<base::DictValue> up_json = base::JSONReader::ReadDict(
      sessions[0].events[1].message, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(up_json.has_value());
  const std::string* up_scrubbed =
      up_json->FindList("typed_payloads")
          ->front()
          .GetDict()
          .FindStringByDottedPath("decoded_transport_probe.response.payload");
  ASSERT_NE(up_scrubbed, nullptr);
  EXPECT_EQ(*up_scrubbed, expected_scrubbed);

  std::optional<base::DictValue> malformed_json = base::JSONReader::ReadDict(
      sessions[0].events[2].message, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(malformed_json.has_value());
  EXPECT_EQ(malformed_json->FindList("typed_payloads")
                ->front()
                .GetDict()
                .Find("decoded_transport_probe"),
            nullptr);

  base::DictValue exported = factory.ExportAllSessionsAsValue();
  const base::ListValue* exported_events =
      exported.FindList("sessions")->front().GetDict().FindList("events");
  ASSERT_NE(exported_events, nullptr);
  ASSERT_EQ(exported_events->size(), 3u);
  EXPECT_EQ(
      *(*exported_events)[0]
           .GetDict()
           .FindDict("message")
           ->FindList("typed_payloads")
           ->front()
           .GetDict()
           .FindStringByDottedPath("decoded_transport_probe.request.payload"),
      expected_scrubbed);
}

}  // namespace
}  // namespace browser_actuator::transport
