// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/etw_consumer_win.h"

#include <windows.h>

#include <stdint.h>

#include <optional>
#include <queue>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/protozero/message.h"
#include "third_party/perfetto/include/perfetto/protozero/message_handle.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_heap_buffer.h"
#include "third_party/perfetto/include/perfetto/tracing/trace_writer_base.h"
#include "third_party/perfetto/protos/perfetto/trace/etw/etw.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/etw/etw_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/etw/etw_event_bundle.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace tracing {

namespace {

// A trace writer that creates TracePacket messages on the heap and sends their
// serialized form to an owner-provided callback.
class FakeTraceWriter : public perfetto::TraceWriterBase,
                        public protozero::MessageFinalizationListener {
 public:
  using TracePacketHandle =
      protozero::MessageHandle<perfetto::protos::pbzero::TracePacket>;

  explicit FakeTraceWriter(
      base::RepeatingCallback<void(std::vector<uint8_t>)> on_packet)
      : on_packet_(std::move(on_packet)) {}

  // perfetto::TraceWriterBase:
  TracePacketHandle NewTracePacket() override {
    packet_ = std::make_unique<
        protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket>>();
    TracePacketHandle handle(packet_->get());
    handle.set_finalization_listener(this);
    return handle;
  }
  void FinishTracePacket() override { NOTREACHED(); }
  void Flush(std::function<void()> callback = {}) override {}
  uint64_t written() const override { return 0u; }

  // protozero::MessageFinalizationListener:
  void OnMessageFinalized(protozero::Message* message) override {
    on_packet_.Run(packet_->SerializeAsArray());
    packet_.reset();
  }

 private:
  base::RepeatingCallback<void(std::vector<uint8_t>)> on_packet_;
  std::unique_ptr<
      protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket>>
      packet_;
};

}  // namespace

class EtwConsumerTest : public testing::Test {
 protected:
  // Generates an ETW CSwitch event with `packet_data` as its payload and sends
  // it to the EtwConsumer for processing. If the EtwConsumer generates a
  // TracePacket containing a `CSwitchEtwEvent`, `cswitch_decoder` is
  // constructed from it.
  void ProcessCSwitchEvent(
      base::span<const uint8_t> packet_data,
      std::optional<perfetto::protos::pbzero::CSwitchEtwEvent::Decoder>&
          cswitch_decoder) {
    std::optional<perfetto::protos::pbzero::TracePacket::Decoder> decoder;
    ASSERT_NO_FATAL_FAILURE(ProcessThreadEvent(/*version=*/2u, /*opcode=*/36u,
                                               packet_data, decoder));
    if (!decoder.has_value()) {
      return;
    }
    ASSERT_TRUE(decoder->has_timestamp());
    ASSERT_NE(decoder->timestamp(), 0u);
    ASSERT_TRUE(decoder->has_etw_events());
    perfetto::protos::pbzero::EtwTraceEventBundle::Decoder bundle(
        decoder->etw_events());
    ASSERT_TRUE(bundle.has_event());
    perfetto::protos::pbzero::EtwTraceEvent::Decoder event(*bundle.event());
    ASSERT_TRUE(event.has_timestamp());
    ASSERT_TRUE(event.has_cpu());
    ASSERT_EQ(event.cpu(), kTestProcessorIndex);
    ASSERT_TRUE(event.has_c_switch());
    cswitch_decoder.emplace(event.c_switch());
  }

 private:
  static constexpr uint16_t kTestProcessorIndex = 47;

  // Generates an ETW EVENT_RECORD from the Thread provider of a particular
  // version and opcode with `packet_data` as its payload and sends it to the
  // EtwConsumer for processing. If the EtwConsumer generates a TracePacket,
  // `decoder` is constructed from it.
  void ProcessThreadEvent(
      uint8_t version,
      uint8_t opcode,
      base::span<const uint8_t> packet_data,
      std::optional<perfetto::protos::pbzero::TracePacket::Decoder>& decoder) {
    ProcessEvent({0x3d6fa8d1,
                  0xfe05,
                  0x11d0,
                  {0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c}},
                 version, opcode, packet_data, decoder);
  }

  // Generates an ETW EVENT_RECORD for a given trace provider of a particular
  // version and opcode with `packet_data` as its payload and sends it to the
  // EtwConsumer for processing. If the EtwConsumer generates a TracePacket,
  // `decoder` is constructed from it.
  void ProcessEvent(
      const GUID& provider,
      uint8_t version,
      uint8_t opcode,
      base::span<const uint8_t> packet_data,
      std::optional<perfetto::protos::pbzero::TracePacket::Decoder>& decoder) {
    EVENT_RECORD event_record = {
        .EventHeader = {.TimeStamp = {},
                        .ProviderId = provider,
                        .EventDescriptor = {.Version = version,
                                            .Opcode = opcode}},
        .BufferContext = {.ProcessorIndex = kTestProcessorIndex},
        .UserDataLength = base::checked_cast<uint16_t>(packet_data.size()),
        .UserData = const_cast<uint8_t*>(packet_data.data()),
        .UserContext = &etw_consumer_};
    ::QueryPerformanceCounter(&event_record.EventHeader.TimeStamp);
    etw_consumer_.ProcessEventRecord(&event_record);

    EVENT_TRACE_LOGFILE event_trace_logfile = {.Context = &etw_consumer_};
    ASSERT_TRUE(etw_consumer_.ProcessBuffer(&event_trace_logfile));

    if (messages_.empty()) {
      return;
    }
    auto& message = messages_.front();
    decoder.emplace(message.data(), message.size());
    // The decoder references the memory owned by `message`, so the item in
    // `messages_` must stay alive.
  }

  // Called by FakeTraceWriter to process the message for a TracePacket.
  void OnPacket(std::vector<uint8_t> message) {
    messages_.push(std::move(message));
  }

  EtwConsumer etw_consumer_{std::make_unique<FakeTraceWriter>(
      base::BindRepeating(&EtwConsumerTest::OnPacket, base::Unretained(this)))};
  std::queue<std::vector<uint8_t>> messages_;
};

// Tests that no CSwitchEtwEvent is emitted for an empty CSwitch ETW event.
TEST_F(EtwConsumerTest, CSwitchEventIsEmpty) {
  std::optional<perfetto::protos::pbzero::CSwitchEtwEvent::Decoder>
      cswitch_decoder;
  ASSERT_NO_FATAL_FAILURE(ProcessCSwitchEvent({}, cswitch_decoder));
  ASSERT_FALSE(cswitch_decoder.has_value());
}

// Tests that no CSwitchEtwEvent is emitted for a small CSwitch ETW event.
TEST_F(EtwConsumerTest, CSwitchEventIsTooShort) {
  static constexpr uint8_t kData[] = {0x00, 23};
  std::optional<perfetto::protos::pbzero::CSwitchEtwEvent::Decoder>
      cswitch_decoder;
  ASSERT_NO_FATAL_FAILURE(ProcessCSwitchEvent({kData}, cswitch_decoder));
  ASSERT_FALSE(cswitch_decoder.has_value());
}

// Tests that CSwitchEtwEvent is emitted for a CSwitch ETW event.
TEST_F(EtwConsumerTest, CSwitchEvent) {
  static constexpr uint8_t kData[] = {
      0x55, 0x00, 0x00, 0x00,  // new_thread_id
      0x44, 0x00, 0x00, 0x00,  // old_thread_id
      0x01,                    // new_thread_priority
      0x02,                    // old_thread_priority
      0x03,                    // previous_c_state
      0x42,                    // SpareByte
      36,                      // old_thread_wait_reason = WR_RUNDOWN
      1,                       // old_thread_wait_mode = USER_MODE
      7,                       // old_thread_state = DEFERRED_READY
      0x04,                    // old_thread_wait_ideal_processor
      0x05, 0x00, 0x00, 0x00,  // new_thread_wait_time
      0x42, 0x42, 0x42, 0x42,  // Reserved
  };
  std::optional<perfetto::protos::pbzero::CSwitchEtwEvent::Decoder>
      cswitch_decoder;
  ASSERT_NO_FATAL_FAILURE(ProcessCSwitchEvent({kData}, cswitch_decoder));
  ASSERT_TRUE(cswitch_decoder.has_value());
  auto& c_switch = *cswitch_decoder;

  EXPECT_EQ(0x55u, c_switch.new_thread_id());
  EXPECT_EQ(0x44u, c_switch.old_thread_id());
  EXPECT_EQ(0x01, c_switch.new_thread_priority());
  EXPECT_EQ(0x02, c_switch.old_thread_priority());
  EXPECT_EQ(0x03u, c_switch.previous_c_state());
  EXPECT_EQ(perfetto::protos::pbzero::CSwitchEtwEvent::WR_RUNDOWN,
            c_switch.old_thread_wait_reason());
  EXPECT_EQ(perfetto::protos::pbzero::CSwitchEtwEvent::USER_MODE,
            c_switch.old_thread_wait_mode());
  EXPECT_EQ(perfetto::protos::pbzero::CSwitchEtwEvent::DEFERRED_READY,
            c_switch.old_thread_state());
  EXPECT_EQ(0x04, c_switch.old_thread_wait_ideal_processor());
  EXPECT_EQ(0x05u, c_switch.new_thread_wait_time());
}

}  // namespace tracing
