// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/etw_consumer_win.h"

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/cstring_view.h"
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
  uint64_t drop_count() const override { return 0u; }

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

struct ProcessData {
  uint32_t process_id;
  uint32_t parent_id;
  uint32_t session_id;
  std::string image_file_name;
  std::wstring command_line;
};

struct ThreadData {
  uint32_t process_id;
  uint32_t thread_id;
  std::optional<std::wstring> thread_name;
};

struct CSwitchData {
  uint32_t new_thread_id;
  uint32_t old_thread_id;
};

// Returns the MOF encoding of a sid, including the leading uint32_t and
// TOKEN_USER.
base::HeapArray<uint8_t> EncodeSid(size_t pointer_size) {
  static constexpr uint8_t kLeadingBytes[] = {0x04, 0x00, 0x00, 0x00};
  static constexpr uint8_t kTokenUserBytes[] = {
      0x20, 0xA8, 0xA4, 0x5C, 0x86, 0xD1, 0xFF, 0xFF,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  static constexpr uint8_t kSidBytes[] = {0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
                                          0x00, 0x05, 0x12, 0x00, 0x00, 0x00};

  std::vector<uint8_t> buffer;
  auto iter = std::back_inserter(buffer);
  std::ranges::copy(base::as_byte_span(kLeadingBytes), iter);
  std::ranges::copy(base::span(kTokenUserBytes).first(2 * pointer_size), iter);
  std::ranges::copy(base::as_byte_span(kSidBytes), iter);

  return base::HeapArray<uint8_t>::CopiedFrom({buffer});
}

// Returns the MOF encoding of a Process event.
base::HeapArray<uint8_t> EncodeProcess(const ProcessData& process,
                                       int version,
                                       size_t pointer_size) {
  std::vector<uint8_t> buffer;
  auto iter = std::back_inserter(buffer);
  if (version == 0) {
    // ProcessId and ParentId are pointer-sized in version 0.
    if (pointer_size == sizeof(uint64_t)) {
      uint64_t value = process.process_id;
      std::ranges::copy(base::byte_span_from_ref(value), iter);
      value = process.parent_id;
      std::ranges::copy(base::byte_span_from_ref(value), iter);
    } else {
      CHECK_EQ(pointer_size, sizeof(uint32_t));
      uint32_t value = process.process_id;
      std::ranges::copy(base::byte_span_from_ref(value), iter);
      value = process.parent_id;
      std::ranges::copy(base::byte_span_from_ref(value), iter);
    }
    std::ranges::copy(EncodeSid(pointer_size), iter);
    std::ranges::copy(process.image_file_name, iter);
    buffer.insert(buffer.end(), '\0');  // ImageFileName terminator
  } else {
    if (version == 1) {
      // PageDirectoryBase
      buffer.insert(buffer.end(), pointer_size, 0);
    } else if (version >= 2) {
      // UniqueProcessKey
      buffer.insert(buffer.end(), pointer_size, 0);
    }
    std::ranges::copy(base::byte_span_from_ref(process.process_id), iter);
    std::ranges::copy(base::byte_span_from_ref(process.parent_id), iter);
    std::ranges::copy(base::byte_span_from_ref(process.session_id), iter);
    buffer.insert(buffer.end(), sizeof(int32_t), 0);  // ExitStatus
    if (version >= 3) {
      buffer.insert(buffer.end(), pointer_size, 0);  // DirectoryTableBase
    }
    std::ranges::copy(EncodeSid(pointer_size), iter);
    std::ranges::copy(process.image_file_name, iter);
    buffer.insert(buffer.end(), '\0');  // ImageFileName terminator
    if (version >= 2) {
      std::ranges::copy(base::as_byte_span(process.command_line), iter);
      buffer.insert(buffer.end(), sizeof(wchar_t), 0);  // terminator
    }
    if (version >= 4) {
      buffer.insert(buffer.end(), sizeof(wchar_t), 0);  // PackageFullName
      buffer.insert(buffer.end(), sizeof(wchar_t), 0);  // ApplicationId
    }
  }
  return base::HeapArray<uint8_t>::CopiedFrom({buffer});
}

// Returns the MOF encoding of a Thread event (v4 by default).
base::HeapArray<uint8_t> EncodeThread(const ThreadData& thread,
                                      int version = 4) {
  std::vector<uint8_t> buffer;
  auto iter = std::back_inserter(buffer);
  if (version == 0) {
    std::ranges::copy(base::byte_span_from_ref(thread.thread_id), iter);
    std::ranges::copy(base::byte_span_from_ref(thread.process_id), iter);
  } else {
    std::ranges::copy(base::byte_span_from_ref(thread.process_id), iter);
    std::ranges::copy(base::byte_span_from_ref(thread.thread_id), iter);
    uintptr_t a_pointer = 0;
    uint32_t an_int = 0;
    // StackBase
    std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);
    // StackLimit
    std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);
    // UserStackBase
    std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);
    // UserStackLimit
    std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);
    // StartAddr (1, 2) / Affinity (>=3)
    std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);
    // Win32StartAddr
    std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);
    if (version == 1) {
      // WaitMode
      buffer.insert(buffer.end(), 0x0a);
    } else if (version >= 2) {
      // TebBase
      std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);
      // SubProcessTag
      std::ranges::copy(base::byte_span_from_ref(++an_int), iter);
    }
    if (version >= 3) {
      buffer.insert(buffer.end(), 0x0a);  // BasePriority
      buffer.insert(buffer.end(), 0x0b);  // PagePriority
      buffer.insert(buffer.end(), 0x0c);  // IoPriority
      buffer.insert(buffer.end(), 0x0d);  // ThreadFlags
    }
    if (version >= 4 && thread.thread_name.has_value()) {
      std::ranges::copy(base::as_byte_span(*thread.thread_name), iter);
      buffer.insert(buffer.end(), sizeof(wchar_t), 0);  // ThreadName terminator
    }
  }
  return base::HeapArray<uint8_t>::CopiedFrom({buffer});
}

base::HeapArray<uint8_t> EncodeThreadSetName(uint32_t process_id,
                                             uint32_t thread_id,
                                             base::wcstring_view thread_name) {
  std::vector<uint8_t> buffer;
  auto iter = std::back_inserter(buffer);
  std::ranges::copy(base::byte_span_from_ref(process_id), iter);
  std::ranges::copy(base::byte_span_from_ref(thread_id), iter);
  std::ranges::copy(base::as_byte_span(thread_name), iter);
  buffer.insert(buffer.end(), sizeof(wchar_t), 0);  // ThreadName terminator
  return base::HeapArray<uint8_t>::CopiedFrom({buffer});
}

// Returns the MOF encoding of a v2 CSwitch event.
base::HeapArray<uint8_t> EncodeCSwitch(const CSwitchData& c_switch) {
  std::vector<uint8_t> buffer;
  auto iter = std::back_inserter(buffer);
  std::ranges::copy(base::byte_span_from_ref(c_switch.new_thread_id), iter);
  std::ranges::copy(base::byte_span_from_ref(c_switch.old_thread_id), iter);
  buffer.insert(buffer.end(), 0x01);  // NewThreadPriority
  buffer.insert(buffer.end(), 0x02);  // OldThreadPriority
  buffer.insert(buffer.end(), 0x03);  // PreviousCState
  buffer.insert(buffer.end(), 0x42);  // SpareByte
  buffer.insert(buffer.end(), 36);    // OldThreadWaitReason = WR_RUNDOWN
  buffer.insert(buffer.end(), 1);     // OldThreadWaitMode = USER_MODE
  buffer.insert(buffer.end(), 7);     // OldThreadState = DEFERRED_READY
  buffer.insert(buffer.end(), 0x04);  // OldThreadWaitIdealProcessor
  const uint32_t new_thread_wait_time = 0x05;
  std::ranges::copy(base::byte_span_from_ref(new_thread_wait_time), iter);
  buffer.insert(buffer.end(), sizeof(uint32_t), 0x42);  // Reserved
  return base::HeapArray<uint8_t>::CopiedFrom({buffer});
}

}  // namespace

// A test fixture that instantiates an EtwConsumer and sends it some events to
// preconfigure active threads of each process category (a client proc, a
// system proc, and an "other" proc).
class EtwConsumerTest : public testing::Test {
 protected:
  // Identifiers of pre-configured procs and threads.
  static constexpr uint32_t kClientPid = 0x1000;
  static constexpr uint32_t kSystemPid = 0x2000;
  static constexpr uint32_t kOtherPid = 0x3000;

  static constexpr uint32_t kClientTid = kClientPid + 0x100;
  static constexpr uint32_t kClientTid2 = kClientTid + 1;
  static constexpr uint32_t kSystemTid = kSystemPid + 0x100;
  static constexpr uint32_t kOtherTid = kOtherPid + 0x100;

  // Holds a serialized TracePacket message and a decoder that reads from it.
  class MessageAndDecoder {
   public:
    explicit MessageAndDecoder(std::vector<uint8_t> data)
        : data_(std::move(data)), decoder_(data_.data(), data_.size()) {}
    MessageAndDecoder(const MessageAndDecoder&) = delete;
    MessageAndDecoder& operator=(const MessageAndDecoder&) = delete;

    const perfetto::protos::pbzero::TracePacket::Decoder& decoder() const {
      return decoder_;
    }

   private:
    std::vector<uint8_t> data_;
    perfetto::protos::pbzero::TracePacket::Decoder decoder_;
  };

  // testing::Test:
  void SetUp() override {
    // Send data collection start events for three processes w/ a thread each.
    SendProcessDcStartEvent(EncodeProcess({.process_id = kSystemPid,
                                           .parent_id = 4,
                                           .session_id = 0xFFFF,
                                           .image_file_name = "ntoskrnl.exe",
                                           .command_line = L"ntoskrnl.exe"}));
    SendThreadDcStartEvent(
        EncodeThread({.process_id = kSystemPid, .thread_id = kSystemTid}));
    SendProcessDcStartEvent(EncodeProcess({.process_id = kClientPid,
                                           .parent_id = kSystemPid,
                                           .session_id = 4,
                                           .image_file_name = "chrome.exe",
                                           .command_line = L"chrome.exe"}));
    SendThreadDcStartEvent(
        EncodeThread({.process_id = kClientPid, .thread_id = kClientTid}));
    SendThreadDcStartEvent(
        EncodeThread({.process_id = kClientPid, .thread_id = kClientTid2}));
    SendProcessDcStartEvent(EncodeProcess({.process_id = kOtherPid,
                                           .parent_id = kSystemPid,
                                           .session_id = 4,
                                           .image_file_name = "cmd.exe",
                                           .command_line = L"cmd.exe"}));
    SendThreadDcStartEvent(
        EncodeThread({.process_id = kOtherPid, .thread_id = kOtherTid}));
  }

  void TearDown() override {
    // Send data collection end events for the threads and processes.
    SendThreadDcEndEvent(
        EncodeThread({.process_id = kOtherPid, .thread_id = kOtherTid}));
    SendProcessDcEndEvent(EncodeProcess({.process_id = kOtherPid}));

    SendThreadDcEndEvent(
        EncodeThread({.process_id = kClientPid, .thread_id = kClientTid2}));
    SendThreadDcEndEvent(
        EncodeThread({.process_id = kClientPid, .thread_id = kClientTid}));
    SendProcessDcEndEvent(EncodeProcess({.process_id = kClientPid}));

    SendThreadDcEndEvent(
        EncodeThread({.process_id = kSystemPid, .thread_id = kSystemTid}));
    SendProcessDcEndEvent(EncodeProcess({.process_id = kSystemPid}));
  }

  // Generates an ETW CSwitch event with `packet_data` as its payload and sends
  // it to the EtwConsumer for processing. If the EtwConsumer generates a
  // TracePacket containing a `CSwitchEtwEvent`, `cswitch_decoder` is
  // constructed from it.
  void ProcessCSwitchEvent(base::span<const uint8_t> packet_data) {
    SendThreadEvent(/*version=*/2u, /*opcode=*/36u, packet_data);
  }

  // Validates the TracePacket processed by `decoder` and populates `c_switch`
  // with a decoder for the first ETW event contained therein.
  void ValidateAndDecodeCSwitch(
      const MessageAndDecoder& decoder,
      std::optional<perfetto::protos::pbzero::CSwitchEtwEvent::Decoder>&
          c_switch) {
    auto& trace_packet_decoder = decoder.decoder();

    ASSERT_TRUE(trace_packet_decoder.has_timestamp());
    ASSERT_NE(trace_packet_decoder.timestamp(), 0u);
    ASSERT_TRUE(trace_packet_decoder.has_etw_events());
    perfetto::protos::pbzero::EtwTraceEventBundle::Decoder bundle(
        trace_packet_decoder.etw_events());
    ASSERT_TRUE(bundle.has_event());
    perfetto::protos::pbzero::EtwTraceEvent::Decoder event(*bundle.event());
    ASSERT_TRUE(event.has_timestamp());
    ASSERT_TRUE(event.has_cpu());
    ASSERT_EQ(event.cpu(), kTestProcessorIndex);
    ASSERT_TRUE(event.has_c_switch());
    c_switch.emplace(event.c_switch());
  }

  void SendProcessStartEvent(base::span<const uint8_t> packet_data) {
    SendProcessEvent(/*version=*/4u, /*opcode=*/1u, packet_data);
  }

  void SendProcessEndEvent(base::span<const uint8_t> packet_data) {
    SendProcessEvent(/*version=*/4u, /*opcode=*/2u, packet_data);
  }

  void SendProcessDcStartEvent(base::span<const uint8_t> packet_data) {
    SendProcessEvent(/*version=*/4u, /*opcode=*/3u, packet_data);
  }

  void SendProcessDcEndEvent(base::span<const uint8_t> packet_data) {
    SendProcessEvent(/*version=*/4u, /*opcode=*/4u, packet_data);
  }

  void SendThreadStartEvent(base::span<const uint8_t> packet_data) {
    SendThreadEvent(/*version=*/4u, /*opcode=*/1u, packet_data);
  }

  void SendThreadEndEvent(base::span<const uint8_t> packet_data) {
    SendThreadEvent(/*version=*/4u, /*opcode=*/2u, packet_data);
  }

  void SendThreadDcStartEvent(base::span<const uint8_t> packet_data) {
    SendThreadEvent(/*version=*/4u, /*opcode=*/3u, packet_data);
  }

  void SendThreadDcEndEvent(base::span<const uint8_t> packet_data) {
    SendThreadEvent(/*version=*/4u, /*opcode=*/4u, packet_data);
  }

  void SendThreadSetName(uint32_t process_id,
                         uint32_t thread_id,
                         base::wcstring_view thread_name) {
    SendThreadEvent(/*version=*/2, /*opcode=*/72,
                    EncodeThreadSetName(process_id, thread_id, thread_name));
  }

  const ActiveProcesses& active_processes() const {
    return etw_consumer_.active_processes();
  }

  // Returns the collection of decoders for serialized TracePacket messages
  // generated by the test's EtwConsumer.
  const std::vector<std::unique_ptr<MessageAndDecoder>>& decoders() const {
    return decoders_;
  }

  // Generates an ETW EVENT_RECORD from the Thread provider of a particular
  // version and opcode with `packet_data` as its payload and sends it to the
  // EtwConsumer for processing. If the EtwConsumer generates a TracePacket,
  // `decoder` is constructed from it.
  void SendThreadEvent(uint8_t version,
                       uint8_t opcode,
                       base::span<const uint8_t> packet_data) {
    ProcessEvent({0x3d6fa8d1,
                  0xfe05,
                  0x11d0,
                  {0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c}},
                 version, opcode, packet_data);
  }

  // Generates an ETW Process event with `packet_data` as its payload and sends
  // it to the EtwConsumer for processing.
  void SendProcessEvent(uint8_t version,
                        uint8_t opcode,
                        base::span<const uint8_t> packet_data) {
    ProcessEvent({0x3d6fa8d0,
                  0xfe05,
                  0x11d0,
                  {0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c}},
                 version, opcode, packet_data);
  }

  // Returns the MOF encoding of a Process event (v4 by default).
  base::HeapArray<uint8_t> EncodeProcess(const ProcessData& process,
                                         int version = 4) {
    // We are using EVENT_HEADER_FLAG_64_BIT_HEADER flag, so the pointer size
    // should be 8 bytes.
    const size_t pointer_size = EtwConsumer::GetPointerSize(kEventHeaderFlags);
    CHECK_EQ(pointer_size, sizeof(uint64_t));
    return ::tracing::EncodeProcess(process, version, pointer_size);
  }

 private:
  static constexpr uint16_t kTestProcessorIndex = 47;
  static constexpr uint16_t kEventHeaderFlags = EVENT_HEADER_FLAG_64_BIT_HEADER;

  // Generates an ETW EVENT_RECORD for a given trace provider of a particular
  // version and opcode with `packet_data` as its payload and sends it to the
  // EtwConsumer for processing. If the EtwConsumer generates a TracePacket,
  // `decoder` is constructed from it.
  void ProcessEvent(const GUID& provider,
                    uint8_t version,
                    uint8_t opcode,
                    base::span<const uint8_t> packet_data) {
    EVENT_RECORD event_record = {
        .EventHeader = {.Flags = kEventHeaderFlags,
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
    EXPECT_TRUE(etw_consumer_.ProcessBuffer(&event_trace_logfile));
  }

  // Called by FakeTraceWriter to process the message for a TracePacket.
  void OnPacket(std::vector<uint8_t> message) {
    decoders_.push_back(
        std::make_unique<MessageAndDecoder>(std::move(message)));
  }

  EtwConsumer etw_consumer_{kClientPid,
                            std::make_unique<FakeTraceWriter>(
                                base::BindRepeating(&EtwConsumerTest::OnPacket,
                                                    base::Unretained(this)))};
  // Serialized TracePacket messages and corresponding decoders emitted by the
  // EtwConsumer.
  std::vector<std::unique_ptr<MessageAndDecoder>> decoders_;
};

// Tests that no CSwitchEtwEvent is emitted for an empty CSwitch ETW event.
TEST_F(EtwConsumerTest, CSwitchEventIsEmpty) {
  ProcessCSwitchEvent({});
  ASSERT_TRUE(decoders().empty());
}

// Tests that no CSwitchEtwEvent is emitted for a small CSwitch ETW event.
TEST_F(EtwConsumerTest, CSwitchEventIsTooShort) {
  static constexpr uint8_t kData[] = {0x00, 23};
  ProcessCSwitchEvent({kData});
  ASSERT_TRUE(decoders().empty());
}

// Tests that CSwitchEtwEvent is emitted for a CSwitch ETW event.
TEST_F(EtwConsumerTest, CSwitchEvent) {
  ProcessCSwitchEvent(EncodeCSwitch(
      {.new_thread_id = kClientTid, .old_thread_id = kClientTid2}));
  ASSERT_EQ(decoders().size(), 1u);

  std::optional<perfetto::protos::pbzero::CSwitchEtwEvent::Decoder> c_switch;
  ASSERT_NO_FATAL_FAILURE(
      ValidateAndDecodeCSwitch(*decoders().front(), c_switch));

  EXPECT_EQ(kClientTid, c_switch->new_thread_id());
  EXPECT_EQ(kClientTid2, c_switch->old_thread_id());
  EXPECT_EQ(0x01, c_switch->new_thread_priority());
  EXPECT_EQ(0x02, c_switch->old_thread_priority());
  EXPECT_EQ(0x03u, c_switch->previous_c_state());
  EXPECT_EQ(perfetto::protos::pbzero::CSwitchEtwEvent::WR_RUNDOWN,
            c_switch->old_thread_wait_reason_int());
  EXPECT_EQ(perfetto::protos::pbzero::CSwitchEtwEvent::USER_MODE,
            c_switch->old_thread_wait_mode_int());
  EXPECT_EQ(perfetto::protos::pbzero::CSwitchEtwEvent::DEFERRED_READY,
            c_switch->old_thread_state_int());
  EXPECT_EQ(0x04, c_switch->old_thread_wait_ideal_processor());
  EXPECT_EQ(0x05u, c_switch->new_thread_wait_time());
}

// Tests that CSwitch events have the thread IDs filtered as appropriate.
TEST_F(EtwConsumerTest, CSwitchFiltering) {
  // Old TID is masked if it doesn't belong to Chrome.
  ProcessCSwitchEvent(EncodeCSwitch(
      {.new_thread_id = kClientTid, .old_thread_id = kSystemTid}));
  ASSERT_EQ(decoders().size(), 1u);
  std::optional<perfetto::protos::pbzero::CSwitchEtwEvent::Decoder> c_switch;
  ASSERT_NO_FATAL_FAILURE(
      ValidateAndDecodeCSwitch(*decoders().back(), c_switch));
  EXPECT_TRUE(c_switch->has_new_thread_id());
  EXPECT_FALSE(c_switch->has_old_thread_id());

  // Both TIDs are masked if neither belongs to Chrome.
  ProcessCSwitchEvent(
      EncodeCSwitch({.new_thread_id = kOtherTid, .old_thread_id = kSystemTid}));
  ASSERT_EQ(decoders().size(), 2u);
  ASSERT_NO_FATAL_FAILURE(
      ValidateAndDecodeCSwitch(*decoders().back(), c_switch));
  EXPECT_FALSE(c_switch->has_new_thread_id());
  EXPECT_FALSE(c_switch->has_old_thread_id());

  // New TID is masked if it doesn't belong to Chrome.
  ProcessCSwitchEvent(
      EncodeCSwitch({.new_thread_id = kOtherTid, .old_thread_id = kClientTid}));
  ASSERT_EQ(decoders().size(), 3u);
  ASSERT_NO_FATAL_FAILURE(
      ValidateAndDecodeCSwitch(*decoders().back(), c_switch));
  EXPECT_FALSE(c_switch->has_new_thread_id());
  EXPECT_TRUE(c_switch->has_old_thread_id());
}

TEST_F(EtwConsumerTest, ThreadSetName) {
  SendThreadSetName(kClientPid, kClientTid, L"kaboom");
  ASSERT_EQ(active_processes().GetThreadName(kClientTid), L"kaboom");
}

TEST_F(EtwConsumerTest, ProcessStartIsEmpty) {
  SendProcessStartEvent({});
}

// Tests that different versions of a Process event are handled.
TEST_F(EtwConsumerTest, ProcessVersions) {
  static constexpr uint32_t kPid = 0x4000;

  for (int version = 0; version <= 4; ++version) {
    ASSERT_TRUE(active_processes().GetProcessImageFileName(kPid).empty());
    auto payload = EncodeProcess({.process_id = kPid,
                                  .parent_id = kSystemPid,
                                  .image_file_name = "himom"},
                                 /*version=*/version);
    SendProcessEvent(/*version=*/version, /*opcode=*/1u, payload);  // Start
    ASSERT_EQ(active_processes().GetProcessImageFileName(kPid), "himom");
    SendProcessEvent(/*version=*/version, /*opcode=*/2u, payload);  // End
    ASSERT_TRUE(active_processes().GetProcessImageFileName(kPid).empty());
  }
}

TEST_F(EtwConsumerTest, ThreadVersions) {
  static constexpr uint32_t kTid = 0x4000;

  for (int version = 0; version <= 4; ++version) {
    ASSERT_EQ(active_processes().GetThreadCategory(kTid),
              ActiveProcesses::Category::kOther);
    auto payload = EncodeThread(
        {.process_id = kClientPid, .thread_id = kTid, .thread_name = {}},
        /*version=*/version);
    SendThreadEvent(/*version=*/version, /*opcode=*/1u, payload);  // Start
    ASSERT_EQ(active_processes().GetThreadCategory(kTid),
              ActiveProcesses::Category::kClient);
    SendThreadEvent(/*version=*/version, /*opcode=*/2u, payload);  // End
    ASSERT_EQ(active_processes().GetThreadCategory(kTid),
              ActiveProcesses::Category::kOther);
  }
}

TEST_F(EtwConsumerTest, ProcessEndIsEmpty) {
  SendProcessEndEvent({});
}

TEST_F(EtwConsumerTest, ThreadStartIsEmpty) {
  SendThreadStartEvent({});
}

TEST_F(EtwConsumerTest, ThreadEndIsEmpty) {
  SendThreadEndEvent({});
}

}  // namespace tracing
