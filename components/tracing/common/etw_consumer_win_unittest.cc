// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/etw_consumer_win.h"

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <optional>
#include <queue>
#include <string_view>
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
#include "base/strings/utf_string_conversions.h"
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

struct ReadyThreadData {
  uint32_t t_thread_id;
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

// Returns the MOF encoding of a v2 ReadyThread event.
base::HeapArray<uint8_t> EncodeReadyThread(
    const ReadyThreadData& ready_thread) {
  std::vector<uint8_t> buffer;
  auto iter = std::back_inserter(buffer);
  std::ranges::copy(base::byte_span_from_ref(ready_thread.t_thread_id), iter);
  buffer.insert(buffer.end(), 0x01);                   // AdjustReason
  buffer.insert(buffer.end(), 0);                      // AdjustIncrement
  buffer.insert(buffer.end(), 0x01);                   // Flag = THREAD_READIED
  buffer.insert(buffer.end(), sizeof(uint8_t), 0x42);  // Reserved
  return base::HeapArray<uint8_t>::CopiedFrom({buffer});
}

// Returns the MOF encoding of a `FileIo_Create` event.
base::HeapArray<uint8_t> EncodeFileIoCreate(uint32_t ttid,
                                            uint32_t create_options,
                                            uint32_t file_attributes,
                                            uint32_t share_access,
                                            std::wstring_view open_path) {
  std::vector<uint8_t> buffer;
  auto iter = std::back_inserter(buffer);
  uintptr_t a_pointer = 0;
  std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);  // IrpPtr
  std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);  // FileObject
  std::ranges::copy(base::byte_span_from_ref(ttid), iter);
  std::ranges::copy(base::byte_span_from_ref(create_options), iter);
  std::ranges::copy(base::byte_span_from_ref(file_attributes), iter);
  std::ranges::copy(base::byte_span_from_ref(share_access), iter);
  std::ranges::copy(base::as_byte_span(open_path), iter);
  buffer.insert(buffer.end(), sizeof(wchar_t), 0);  // string terminator
  return base::HeapArray<uint8_t>::CopiedFrom({buffer});
}

// Returns the MOF encoding of a `FileIo_DirEnum` event.
base::HeapArray<uint8_t> EncodeFileIoDirEnum(uint32_t ttid,
                                             uint32_t length,
                                             uint32_t info_class,
                                             uint32_t file_index,
                                             std::wstring_view file_name) {
  std::vector<uint8_t> buffer;
  auto iter = std::back_inserter(buffer);
  uintptr_t a_pointer = 0;
  std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);  // IrpPtr
  std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);  // FileObject
  std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);  // FileKey
  std::ranges::copy(base::byte_span_from_ref(ttid), iter);
  std::ranges::copy(base::byte_span_from_ref(length), iter);
  std::ranges::copy(base::byte_span_from_ref(info_class), iter);
  std::ranges::copy(base::byte_span_from_ref(file_index), iter);
  std::ranges::copy(base::as_byte_span(file_name), iter);
  buffer.insert(buffer.end(), sizeof(wchar_t), 0);  // string terminator
  return base::HeapArray<uint8_t>::CopiedFrom({buffer});
}

// Returns the MOF encoding of a `FileIo_Info` event.
base::HeapArray<uint8_t> EncodeFileIoInfo(uint32_t ttid, uint32_t info_class) {
  std::vector<uint8_t> buffer;
  auto iter = std::back_inserter(buffer);
  uintptr_t a_pointer = 0;
  std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);  // IrpPtr
  std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);  // FileObject
  std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);  // FileKey
  std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);  // ExtraInfo
  std::ranges::copy(base::byte_span_from_ref(ttid), iter);
  std::ranges::copy(base::byte_span_from_ref(info_class), iter);
  return base::HeapArray<uint8_t>::CopiedFrom({buffer});
}

// Returns the MOF encoding of a `FileIo_ReadWrite` event.
base::HeapArray<uint8_t> EncodeFileIoReadWrite(uint32_t ttid,
                                               uint64_t offset,
                                               uint32_t io_size,
                                               uint32_t io_flags) {
  std::vector<uint8_t> buffer;
  auto iter = std::back_inserter(buffer);
  uintptr_t a_pointer = 0;
  std::ranges::copy(base::byte_span_from_ref(offset), iter);
  std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);  // IrpPtr
  std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);  // FileObject
  std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);  // FileKey
  std::ranges::copy(base::byte_span_from_ref(ttid), iter);
  std::ranges::copy(base::byte_span_from_ref(io_size), iter);
  std::ranges::copy(base::byte_span_from_ref(io_flags), iter);
  return base::HeapArray<uint8_t>::CopiedFrom({buffer});
}

// Returns the MOF encoding of a `FileIo_SimpleOp` event.
base::HeapArray<uint8_t> EncodeFileIoSimpleOp(uint32_t ttid) {
  std::vector<uint8_t> buffer;
  auto iter = std::back_inserter(buffer);
  uintptr_t a_pointer = 0;
  std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);  // IrpPtr
  std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);  // FileObject
  std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);  // FileKey
  std::ranges::copy(base::byte_span_from_ref(ttid), iter);
  return base::HeapArray<uint8_t>::CopiedFrom({buffer});
}

// Returns the MOF encoding of a `FileIo_OpEnd` event.
base::HeapArray<uint8_t> EncodeFileIoOpEnd(uint32_t ttid, uint32_t nt_status) {
  std::vector<uint8_t> buffer;
  auto iter = std::back_inserter(buffer);
  uintptr_t a_pointer = 0;
  std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);  // IrpPtr
  std::ranges::copy(base::byte_span_from_ref(++a_pointer), iter);  // ExtraInfo
  std::ranges::copy(base::byte_span_from_ref(nt_status), iter);
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
    etw_consumer_ = std::make_unique<EtwConsumer>(
        kClientPid,
        std::make_unique<FakeTraceWriter>(base::BindRepeating(
            &EtwConsumerTest::OnPacket, base::Unretained(this))),
        PrivacyFilteringEnabled());

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

  virtual bool PrivacyFilteringEnabled() { return false; }

  // Validates the TracePacket processed by `decoder` and populates `event`
  // with a decoder for the first ETW event contained therein.
  void ValidateAndDecodeEtwEvent(
      const MessageAndDecoder& decoder,
      std::optional<perfetto::protos::pbzero::EtwTraceEvent::Decoder>& event) {
    auto& trace_packet_decoder = decoder.decoder();

    ASSERT_TRUE(trace_packet_decoder.has_timestamp());
    ASSERT_NE(trace_packet_decoder.timestamp(), 0u);
    ASSERT_TRUE(trace_packet_decoder.has_etw_events());
    perfetto::protos::pbzero::EtwTraceEventBundle::Decoder bundle(
        trace_packet_decoder.etw_events());
    ASSERT_TRUE(bundle.has_event());
    event.emplace(*bundle.event());
    ASSERT_TRUE(event->has_timestamp());
    ASSERT_TRUE(event->has_cpu());
    ASSERT_EQ(event->cpu(), kTestProcessorIndex);
  }

  // Generates an ETW CSwitch event with `packet_data` as its payload and sends
  // it to the EtwConsumer for processing. If the EtwConsumer generates a
  // TracePacket containing a `CSwitchEtwEvent`, a new decoder is constructed
  // from it.
  void ProcessCSwitchEvent(base::span<const uint8_t> packet_data) {
    SendThreadEvent(/*version=*/2u, /*opcode=*/36u, kSystemTid, packet_data);
  }

  // Validates the TracePacket processed by `decoder` and populates `c_switch`
  // with a decoder for the first ETW event contained therein.
  void ValidateAndDecodeCSwitch(
      const MessageAndDecoder& decoder,
      std::optional<perfetto::protos::pbzero::CSwitchEtwEvent::Decoder>&
          c_switch) {
    std::optional<perfetto::protos::pbzero::EtwTraceEvent::Decoder> event;
    ValidateAndDecodeEtwEvent(decoder, event);

    ASSERT_TRUE(event->has_c_switch());
    c_switch.emplace(event->c_switch());
  }

  // Generates an ETW ReadyThread event with `packet_data` as its payload and
  // sends it to the EtwConsumer for processing. If the EtwConsumer generates a
  // TracePacket containing a `ReadyThreadEtwEvent`, a new decoder is
  // constructed from it.
  void ProcessReadyThreadEvent(uint32_t thread_id,
                               base::span<const uint8_t> packet_data) {
    SendThreadEvent(/*version=*/2u, /*opcode=*/50u, thread_id, packet_data);
  }

  // Validates the TracePacket processed by `decoder` and populates
  // `ready_thread` with a decoder for the first ETW event contained therein.
  void ValidateAndDecodeReadyThread(
      const MessageAndDecoder& decoder,
      std::optional<perfetto::protos::pbzero::EtwTraceEvent::Decoder>& event,
      std::optional<perfetto::protos::pbzero::ReadyThreadEtwEvent::Decoder>&
          ready_thread) {
    ValidateAndDecodeEtwEvent(decoder, event);

    ASSERT_TRUE(event->has_ready_thread());
    ready_thread.emplace(event->ready_thread());
  }

  // Generates an ETW FileIo_Create event with `packet_data` as its payload and
  // sends it to the EtwConsumer for processing. If the EtwConsumer generates a
  // TracePacket containing a `FileIoCreateEtwEvent`, a new decoder is
  // constructed from it.
  void ProcessFileIoCreateEvent(uint32_t thread_id,
                                base::span<const uint8_t> packet_data) {
    SendFileIoEvent(/*version=*/0u, /*opcode=*/64u, thread_id, packet_data);
  }

  // Generates an ETW FileIo_DirEnum event with `packet_data` as its payload and
  // sends it to the EtwConsumer for processing. If the EtwConsumer generates a
  // TracePacket containing a `FileIoDirEnumEtwEvent`, a new decoder is
  // constructed from it.
  void ProcessFileIoDirEnumEvent(uint32_t thread_id,
                                 base::span<const uint8_t> packet_data) {
    SendFileIoEvent(/*version=*/0u, /*opcode=*/72u, thread_id, packet_data);
  }

  // Generates an ETW FileIo_ReadWrite event with `packet_data` as its payload
  // and sends it to the EtwConsumer for processing. If the EtwConsumer
  // generates a TracePacket containing a `FileIoReadWriteEtwEvent`, a new
  // decoder is constructed from it.
  void ProcessFileIoReadWriteEvent(uint32_t thread_id,
                                   base::span<const uint8_t> packet_data) {
    SendFileIoEvent(/*version=*/0u, /*opcode=*/67u, thread_id, packet_data);
  }

  // Generates an ETW FileIo_Info event with `packet_data` as its payload and
  // sends it to the EtwConsumer for processing. If the EtwConsumer generates a
  // TracePacket containing a `FileIoInfoEtwEvent`, a new decoder is
  // constructed from it.
  void ProcessFileIoInfoEvent(uint32_t thread_id,
                              base::span<const uint8_t> packet_data) {
    SendFileIoEvent(/*version=*/0u, /*opcode=*/69u, thread_id, packet_data);
  }

  // Generates an ETW FileIo_SimpleOp event with `packet_data` as its payload
  // and sends it to the EtwConsumer for processing. If the EtwConsumer
  // generates a TracePacket containing a `FileIoSimpleOpEtwEvent`, a new
  // decoder is constructed from it.
  void ProcessFileIoSimpleOpEvent(uint32_t thread_id,
                                  base::span<const uint8_t> packet_data) {
    SendFileIoEvent(/*version=*/0u, /*opcode=*/65u, thread_id, packet_data);
  }

  // Generates an ETW FileIo_OpEnd event with `packet_data` as its payload and
  // sends it to the EtwConsumer for processing. If the EtwConsumer generates a
  // TracePacket containing a `FileIoOpEndEtwEvent`, a new decoder is
  // constructed from it.
  void ProcessFileIoOpEndEvent(uint32_t thread_id,
                               base::span<const uint8_t> packet_data) {
    SendFileIoEvent(/*version=*/0u, /*opcode=*/76u, thread_id, packet_data);
  }

  // Validates the TracePacket processed by `decoder` and populates
  // `file_io_create` with a decoder for the first ETW event contained therein.
  void ValidateAndDecodeFileIoCreate(
      const MessageAndDecoder& decoder,
      std::optional<perfetto::protos::pbzero::EtwTraceEvent::Decoder>& event,
      std::optional<perfetto::protos::pbzero::FileIoCreateEtwEvent::Decoder>&
          file_io_create) {
    ValidateAndDecodeEtwEvent(decoder, event);
    ASSERT_TRUE(event->has_file_io_create());
    file_io_create.emplace(event->file_io_create());
  }

  // Validates the TracePacket processed by `decoder` and populates
  // `file_io_dir_enum` with a decoder for the first ETW event contained
  // therein.
  void ValidateAndDecodeFileIoDirEnum(
      const MessageAndDecoder& decoder,
      std::optional<perfetto::protos::pbzero::EtwTraceEvent::Decoder>& event,
      std::optional<perfetto::protos::pbzero::FileIoDirEnumEtwEvent::Decoder>&
          file_io_dir_enum) {
    ValidateAndDecodeEtwEvent(decoder, event);
    ASSERT_TRUE(event->has_file_io_dir_enum());
    file_io_dir_enum.emplace(event->file_io_dir_enum());
  }

  // Validates the TracePacket processed by `decoder` and populates
  // `file_io_info` with a decoder for the first ETW event contained therein.
  void ValidateAndDecodeFileIoInfo(
      const MessageAndDecoder& decoder,
      std::optional<perfetto::protos::pbzero::EtwTraceEvent::Decoder>& event,
      std::optional<perfetto::protos::pbzero::FileIoInfoEtwEvent::Decoder>&
          file_io_info) {
    ValidateAndDecodeEtwEvent(decoder, event);
    ASSERT_TRUE(event->has_file_io_info());
    file_io_info.emplace(event->file_io_info());
  }

  // Validates the TracePacket processed by `decoder` and populates
  // `file_io_read_write` with a decoder for the first ETW event contained
  // therein.
  void ValidateAndDecodeFileIoReadWrite(
      const MessageAndDecoder& decoder,
      std::optional<perfetto::protos::pbzero::EtwTraceEvent::Decoder>& event,
      std::optional<perfetto::protos::pbzero::FileIoReadWriteEtwEvent::Decoder>&
          file_io_read_write) {
    ValidateAndDecodeEtwEvent(decoder, event);
    ASSERT_TRUE(event->has_file_io_read_write());
    file_io_read_write.emplace(event->file_io_read_write());
  }

  // Validates the TracePacket processed by `decoder` and populates
  // `file_io_simple_op` with a decoder for the first ETW event contained
  // therein.
  void ValidateAndDecodeFileIoSimpleOp(
      const MessageAndDecoder& decoder,
      std::optional<perfetto::protos::pbzero::EtwTraceEvent::Decoder>& event,
      std::optional<perfetto::protos::pbzero::FileIoSimpleOpEtwEvent::Decoder>&
          file_io_simple_op) {
    ValidateAndDecodeEtwEvent(decoder, event);
    ASSERT_TRUE(event->has_file_io_simple_op());
    file_io_simple_op.emplace(event->file_io_simple_op());
  }

  // Validates the TracePacket processed by `decoder` and populates
  // `file_io_op_end` with a decoder for the first ETW event contained therein.
  void ValidateAndDecodeFileIoOpEnd(
      const MessageAndDecoder& decoder,
      std::optional<perfetto::protos::pbzero::EtwTraceEvent::Decoder>& event,
      std::optional<perfetto::protos::pbzero::FileIoOpEndEtwEvent::Decoder>&
          file_io_op_end) {
    ValidateAndDecodeEtwEvent(decoder, event);
    ASSERT_TRUE(event->has_file_io_op_end());
    file_io_op_end.emplace(event->file_io_op_end());
  }

  void SendProcessStartEvent(base::span<const uint8_t> packet_data) {
    SendProcessEvent(/*version=*/4u, /*opcode=*/1u, kSystemTid, packet_data);
  }

  void SendProcessEndEvent(base::span<const uint8_t> packet_data) {
    SendProcessEvent(/*version=*/4u, /*opcode=*/2u, kSystemTid, packet_data);
  }

  void SendProcessDcStartEvent(base::span<const uint8_t> packet_data) {
    SendProcessEvent(/*version=*/4u, /*opcode=*/3u, kSystemTid, packet_data);
  }

  void SendProcessDcEndEvent(base::span<const uint8_t> packet_data) {
    SendProcessEvent(/*version=*/4u, /*opcode=*/4u, kSystemTid, packet_data);
  }

  void SendThreadStartEvent(base::span<const uint8_t> packet_data) {
    SendThreadEvent(/*version=*/4u, /*opcode=*/1u, kSystemTid, packet_data);
  }

  void SendThreadEndEvent(base::span<const uint8_t> packet_data) {
    SendThreadEvent(/*version=*/4u, /*opcode=*/2u, kSystemTid, packet_data);
  }

  void SendThreadDcStartEvent(base::span<const uint8_t> packet_data) {
    SendThreadEvent(/*version=*/4u, /*opcode=*/3u, kSystemTid, packet_data);
  }

  void SendThreadDcEndEvent(base::span<const uint8_t> packet_data) {
    SendThreadEvent(/*version=*/4u, /*opcode=*/4u, kSystemTid, packet_data);
  }

  void SendThreadSetName(uint32_t process_id,
                         uint32_t thread_id,
                         base::wcstring_view thread_name) {
    SendThreadEvent(/*version=*/2, /*opcode=*/72, kSystemTid,
                    EncodeThreadSetName(process_id, thread_id, thread_name));
  }

  const ActiveProcesses& active_processes() const {
    return etw_consumer_->active_processes();
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
                       uint32_t thread_id,
                       base::span<const uint8_t> packet_data) {
    ProcessEvent({0x3d6fa8d1,
                  0xfe05,
                  0x11d0,
                  {0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c}},
                 version, opcode, thread_id, packet_data);
  }

  // Generates an ETW Process event with `packet_data` as its payload and sends
  // it to the EtwConsumer for processing.
  void SendProcessEvent(uint8_t version,
                        uint8_t opcode,
                        uint32_t thread_id,
                        base::span<const uint8_t> packet_data) {
    ProcessEvent({0x3d6fa8d0,
                  0xfe05,
                  0x11d0,
                  {0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c}},
                 version, opcode, thread_id, packet_data);
  }

  // Generates an ETW FileIo event with `packet_data` as its payload and sends
  // it to the EtwConsumer for processing.
  void SendFileIoEvent(uint8_t version,
                       uint8_t opcode,
                       uint32_t thread_id,
                       base::span<const uint8_t> packet_data) {
    ProcessEvent({0x90cbdc39,
                  0x4a3e,
                  0x11d1,
                  {0x84, 0xf4, 0x00, 0x00, 0xf8, 0x04, 0x64, 0xe3}},
                 version, opcode, thread_id, packet_data);
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
                    uint32_t thread_id,
                    base::span<const uint8_t> packet_data) {
    EVENT_RECORD event_record = {
        .EventHeader = {.Flags = kEventHeaderFlags,
                        .ThreadId = thread_id,
                        .ProviderId = provider,
                        .EventDescriptor = {.Version = version,
                                            .Opcode = opcode}},
        .BufferContext = {.ProcessorIndex = kTestProcessorIndex},
        .UserDataLength = base::checked_cast<uint16_t>(packet_data.size()),
        .UserData = const_cast<uint8_t*>(packet_data.data()),
        .UserContext = etw_consumer_.get()};
    ::QueryPerformanceCounter(&event_record.EventHeader.TimeStamp);
    etw_consumer_->ProcessEventRecord(&event_record);

    EVENT_TRACE_LOGFILE event_trace_logfile = {.Context = etw_consumer_.get()};
    EXPECT_TRUE(etw_consumer_->ProcessBuffer(&event_trace_logfile));
  }

  // Called by FakeTraceWriter to process the message for a TracePacket.
  void OnPacket(std::vector<uint8_t> message) {
    decoders_.push_back(
        std::make_unique<MessageAndDecoder>(std::move(message)));
  }

  std::unique_ptr<EtwConsumer> etw_consumer_;

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

// Tests that no ReadyThreadEtwEvent is emitted for an empty ReadyThread ETW
// event.
TEST_F(EtwConsumerTest, ReadyThreadEventIsEmpty) {
  ProcessReadyThreadEvent(kClientTid, {});
  ASSERT_TRUE(decoders().empty());
}

// Tests that no ReadyThreadEtwEvent is emitted for a small ReadyThread ETW
// event.
TEST_F(EtwConsumerTest, ReadyThreadEventIsTooShort) {
  static constexpr uint8_t kData[] = {0x00, 23};
  ProcessReadyThreadEvent(kClientTid, {kData});
  ASSERT_TRUE(decoders().empty());
}

// Tests that ReadyThreadEtwEvent is emitted for a ReadyThread ETW event.
TEST_F(EtwConsumerTest, ReadyThreadEvent) {
  ProcessReadyThreadEvent(kClientTid,
                          EncodeReadyThread({.t_thread_id = kClientTid2}));
  ASSERT_EQ(decoders().size(), 1u);

  std::optional<perfetto::protos::pbzero::EtwTraceEvent::Decoder> event;
  std::optional<perfetto::protos::pbzero::ReadyThreadEtwEvent::Decoder>
      ready_thread;
  ASSERT_NO_FATAL_FAILURE(
      ValidateAndDecodeReadyThread(*decoders().front(), event, ready_thread));

  EXPECT_EQ(kClientTid, event->thread_id());
  EXPECT_EQ(kClientTid2, ready_thread->t_thread_id());
  EXPECT_EQ(0x01, ready_thread->adjust_reason_int());
  EXPECT_EQ(0, ready_thread->adjust_increment());
  EXPECT_EQ(0x01, ready_thread->flag_int());
}

// Tests that ReadyThread events have the thread IDs filtered as appropriate.
TEST_F(EtwConsumerTest, ReadyThreadFiltering) {
  // Target TID is masked if it doesn't belong to Chrome.
  ProcessReadyThreadEvent(kClientTid,
                          EncodeReadyThread({.t_thread_id = kSystemTid}));
  ASSERT_EQ(decoders().size(), 1u);
  std::optional<perfetto::protos::pbzero::EtwTraceEvent::Decoder> event;
  std::optional<perfetto::protos::pbzero::ReadyThreadEtwEvent::Decoder>
      ready_thread;
  ASSERT_NO_FATAL_FAILURE(
      ValidateAndDecodeReadyThread(*decoders().back(), event, ready_thread));
  EXPECT_TRUE(event->has_thread_id());
  EXPECT_FALSE(ready_thread->has_t_thread_id());

  // Both TID and target TID are masked if neither belongs to Chrome.
  ProcessReadyThreadEvent(kOtherTid,
                          EncodeReadyThread({.t_thread_id = kSystemTid}));
  ASSERT_EQ(decoders().size(), 2u);
  ASSERT_NO_FATAL_FAILURE(
      ValidateAndDecodeReadyThread(*decoders().back(), event, ready_thread));
  EXPECT_FALSE(event->has_thread_id());
  EXPECT_FALSE(ready_thread->has_t_thread_id());

  // TID is masked if it doesn't belong to Chrome.
  ProcessReadyThreadEvent(kOtherTid,
                          EncodeReadyThread({.t_thread_id = kClientTid}));
  ASSERT_EQ(decoders().size(), 3u);
  ASSERT_NO_FATAL_FAILURE(
      ValidateAndDecodeReadyThread(*decoders().back(), event, ready_thread));
  EXPECT_FALSE(event->has_thread_id());
  EXPECT_TRUE(ready_thread->has_t_thread_id());
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
    SendProcessEvent(/*version=*/version, /*opcode=*/1u, kSystemTid,
                     payload);  // Start
    ASSERT_EQ(active_processes().GetProcessImageFileName(kPid), "himom");
    SendProcessEvent(/*version=*/version, /*opcode=*/2u, kSystemTid,
                     payload);  // End
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
    SendThreadEvent(/*version=*/version, /*opcode=*/1u, kSystemTid,
                    payload);  // Start
    ASSERT_EQ(active_processes().GetThreadCategory(kTid),
              ActiveProcesses::Category::kClient);
    SendThreadEvent(/*version=*/version, /*opcode=*/2u, kSystemTid,
                    payload);  // End
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

// Tests that no FileIoCreateEtwEvent is emitted for an empty FileIo_Create ETW
// event.
TEST_F(EtwConsumerTest, FileIoCreateEventIsEmpty) {
  ProcessFileIoCreateEvent(kClientTid, {});
  EXPECT_TRUE(decoders().empty());
}

// Tests that no FileIoDirEnumEtwEvent is emitted for an empty FileIo_DirEnum
// ETW event.
TEST_F(EtwConsumerTest, FileIoDirEnumEventIsEmpty) {
  ProcessFileIoDirEnumEvent(kClientTid, {});
  EXPECT_TRUE(decoders().empty());
}

// Tests that no FileIoInfoEtwEvent is emitted for an empty FileIo_Info ETW
// event.
TEST_F(EtwConsumerTest, FileIoInfoEventIsEmpty) {
  ProcessFileIoInfoEvent(kClientTid, {});
  EXPECT_TRUE(decoders().empty());
}

// Tests that no FileIoReadWriteEtwEvent is emitted for an empty
// FileIo_ReadWrite ETW event.
TEST_F(EtwConsumerTest, FileIoReadWriteEventIsEmpty) {
  ProcessFileIoReadWriteEvent(kClientTid, {});
  EXPECT_TRUE(decoders().empty());
}

// Tests that no FileIoSimpleOpEtwEvent is emitted for an empty FileIo_SimpleOp
// ETW event.
TEST_F(EtwConsumerTest, FileIoSimpleOpEventIsEmpty) {
  ProcessFileIoSimpleOpEvent(kClientTid, {});
  EXPECT_TRUE(decoders().empty());
}

// Tests that no FileIoOpEndEtwEvent is emitted for an empty FileIo_OpEnd ETW
// event.
TEST_F(EtwConsumerTest, FileIoOpEndEventIsEmpty) {
  ProcessFileIoOpEndEvent(kClientTid, {});
  EXPECT_TRUE(decoders().empty());
}

// Tests that no FileIoCreateEtwEvent is emitted for a too-small FileIo_Create
// ETW event.
TEST_F(EtwConsumerTest, FileIoCreateEventIsTooShort) {
  static constexpr uint8_t kData[] = {0x00, 123};
  ProcessFileIoCreateEvent(kClientTid, {kData});
  EXPECT_TRUE(decoders().empty());
}

// Tests that no FileIoDirEnumEtwEvent is emitted for a too-small FileIo_DirEnum
// ETW event.
TEST_F(EtwConsumerTest, FileIoDirEnumEventIsTooShort) {
  static constexpr uint8_t kData[] = {0x00, 123};
  ProcessFileIoDirEnumEvent(kClientTid, {kData});
  EXPECT_TRUE(decoders().empty());
}

// Tests that no FileIoInfoEtwEvent is emitted for a too-small FileIo_Info ETW
// event.
TEST_F(EtwConsumerTest, FileIoInfoEventIsTooShort) {
  static constexpr uint8_t kData[] = {0x00, 123};
  ProcessFileIoInfoEvent(kClientTid, {kData});
  EXPECT_TRUE(decoders().empty());
}

// Tests that no FileIoReadWriteEtwEvent is emitted for a too-small
// FileIo_ReadWrite ETW event.
TEST_F(EtwConsumerTest, FileIoReadWriteEventIsTooShort) {
  static constexpr uint8_t kData[] = {0x00, 123};
  ProcessFileIoReadWriteEvent(kClientTid, {kData});
  EXPECT_TRUE(decoders().empty());
}

// Tests that no FileIoSimpleOpEtwEvent is emitted for a too-small
// FileIo_SimpleOp ETW event.
TEST_F(EtwConsumerTest, FileIoSimpleOpEventIsTooShort) {
  static constexpr uint8_t kData[] = {0x00, 123};
  ProcessFileIoSimpleOpEvent(kClientTid, {kData});
  EXPECT_TRUE(decoders().empty());
}

// Tests that no FileIoOpEndEtwEvent is emitted for a too-small FileIo_OpEnd ETW
// event.
TEST_F(EtwConsumerTest, FileIoOpEndEventIsTooShort) {
  static constexpr uint8_t kData[] = {0x00, 123};
  ProcessFileIoOpEndEvent(kClientTid, {kData});
  EXPECT_TRUE(decoders().empty());
}

class EtwConsumerTestWithPrivacyFiltering
    : public EtwConsumerTest,
      public testing::WithParamInterface<bool> {
 protected:
  bool PrivacyFilteringEnabled() override { return GetParam(); }
};

// Tests that a FileIoCreateEtwEvent is emitted for a FileIo_Create ETW event.
TEST_P(EtwConsumerTestWithPrivacyFiltering, FileIoCreateEvent) {
  constexpr uint32_t kCreateOptions = 0x01230123;
  constexpr uint32_t kFileAttributes = 0x45674567;
  constexpr uint32_t kShareAccess = 0x89AB89AB;
  constexpr wchar_t kOpenPath[] = L"C:\\file.txt";
  ProcessFileIoCreateEvent(
      kClientTid, EncodeFileIoCreate(kClientTid, kCreateOptions,
                                     kFileAttributes, kShareAccess, kOpenPath));
  ASSERT_EQ(decoders().size(), 1u);

  std::optional<perfetto::protos::pbzero::EtwTraceEvent::Decoder> event;
  std::optional<perfetto::protos::pbzero::FileIoCreateEtwEvent::Decoder>
      file_io_create;
  ASSERT_NO_FATAL_FAILURE(
      ValidateAndDecodeFileIoCreate(*decoders().back(), event, file_io_create));

  EXPECT_EQ(event->thread_id(), kClientTid);
  EXPECT_TRUE(file_io_create->has_irp_ptr());
  EXPECT_TRUE(file_io_create->has_file_object());
  EXPECT_EQ(file_io_create->ttid(), kClientTid);
  EXPECT_EQ(file_io_create->create_options(), kCreateOptions);
  EXPECT_EQ(file_io_create->file_attributes(), kFileAttributes);
  EXPECT_EQ(file_io_create->share_access(), kShareAccess);
  EXPECT_EQ(file_io_create->has_open_path(), !PrivacyFilteringEnabled());
  if (!PrivacyFilteringEnabled()) {
    EXPECT_EQ(std::string(file_io_create->open_path().data,
                          file_io_create->open_path().size),
              base::WideToUTF8(kOpenPath));
  }
}

// Tests that a FileIoDirEnumEtwEvent is emitted for a FileIo_DirEnum ETW event.
TEST_P(EtwConsumerTestWithPrivacyFiltering, FileIoDirEnumEvent) {
  constexpr uint32_t kLength = 0x1111;
  constexpr uint32_t kInfoClass = 0x22;
  constexpr uint32_t kFileIndex = 0x33;
  constexpr wchar_t kFileName[] = L"some\\dir";
  ProcessFileIoDirEnumEvent(kClientTid,
                            EncodeFileIoDirEnum(kClientTid, kLength, kInfoClass,
                                                kFileIndex, kFileName));
  ASSERT_EQ(decoders().size(), 1u);

  std::optional<perfetto::protos::pbzero::EtwTraceEvent::Decoder> event;
  std::optional<perfetto::protos::pbzero::FileIoDirEnumEtwEvent::Decoder>
      file_io_dir_enum;
  ASSERT_NO_FATAL_FAILURE(ValidateAndDecodeFileIoDirEnum(
      *decoders().back(), event, file_io_dir_enum));

  EXPECT_EQ(event->thread_id(), kClientTid);
  EXPECT_TRUE(file_io_dir_enum->has_irp_ptr());
  EXPECT_TRUE(file_io_dir_enum->has_file_object());
  EXPECT_TRUE(file_io_dir_enum->has_file_key());
  EXPECT_EQ(file_io_dir_enum->ttid(), kClientTid);
  EXPECT_EQ(file_io_dir_enum->length(), kLength);
  EXPECT_EQ(file_io_dir_enum->info_class(), kInfoClass);
  EXPECT_EQ(file_io_dir_enum->file_index(), kFileIndex);
  EXPECT_EQ(file_io_dir_enum->has_file_name(), !PrivacyFilteringEnabled());
  if (!PrivacyFilteringEnabled()) {
    EXPECT_EQ(std::string(file_io_dir_enum->file_name().data,
                          file_io_dir_enum->file_name().size),
              base::WideToUTF8(kFileName));
  }
}

// Tests that a FileIoInfoEtwEvent is emitted for a FileIo_Info ETW event.
TEST_P(EtwConsumerTestWithPrivacyFiltering, FileIoInfoEvent) {
  constexpr uint32_t kInfoClass = 0xABC;
  ProcessFileIoInfoEvent(kClientTid, EncodeFileIoInfo(kClientTid, kInfoClass));
  ASSERT_EQ(decoders().size(), 1u);

  std::optional<perfetto::protos::pbzero::EtwTraceEvent::Decoder> event;
  std::optional<perfetto::protos::pbzero::FileIoInfoEtwEvent::Decoder>
      file_io_info;
  ASSERT_NO_FATAL_FAILURE(
      ValidateAndDecodeFileIoInfo(*decoders().back(), event, file_io_info));

  EXPECT_EQ(event->thread_id(), kClientTid);
  EXPECT_TRUE(file_io_info->has_irp_ptr());
  EXPECT_TRUE(file_io_info->has_file_object());
  EXPECT_TRUE(file_io_info->has_file_key());
  EXPECT_TRUE(file_io_info->has_extra_info());
  EXPECT_EQ(file_io_info->ttid(), kClientTid);
  EXPECT_EQ(file_io_info->info_class(), kInfoClass);
}

// Tests that a FileIoReadWriteEtwEvent is emitted for a FileIo_ReadWrite ETW
// event.
TEST_P(EtwConsumerTestWithPrivacyFiltering, FileIoReadWriteEvent) {
  constexpr uint64_t kOffset = 0xAAA;
  constexpr uint32_t kIoSize = 0xBBB;
  constexpr uint32_t kIoFlags = 0xCCC;
  ProcessFileIoReadWriteEvent(
      kClientTid,
      EncodeFileIoReadWrite(kClientTid, kOffset, kIoSize, kIoFlags));
  ASSERT_EQ(decoders().size(), 1u);

  std::optional<perfetto::protos::pbzero::EtwTraceEvent::Decoder> event;
  std::optional<perfetto::protos::pbzero::FileIoReadWriteEtwEvent::Decoder>
      file_io_read_write;
  ASSERT_NO_FATAL_FAILURE(ValidateAndDecodeFileIoReadWrite(
      *decoders().back(), event, file_io_read_write));

  EXPECT_EQ(event->thread_id(), kClientTid);
  EXPECT_EQ(file_io_read_write->offset(), kOffset);
  EXPECT_TRUE(file_io_read_write->has_irp_ptr());
  EXPECT_TRUE(file_io_read_write->has_file_object());
  EXPECT_TRUE(file_io_read_write->has_file_key());
  EXPECT_EQ(file_io_read_write->ttid(), kClientTid);
  EXPECT_EQ(file_io_read_write->io_size(), kIoSize);
  EXPECT_EQ(file_io_read_write->io_flags(), kIoFlags);
}

// Tests that a FileIoSimpleOpEtwEvent is emitted for a FileIo_SimpleOp ETW
// event.
TEST_P(EtwConsumerTestWithPrivacyFiltering, FileIoSimpleOpEvent) {
  ProcessFileIoSimpleOpEvent(kClientTid, EncodeFileIoSimpleOp(kClientTid));
  ASSERT_EQ(decoders().size(), 1u);

  std::optional<perfetto::protos::pbzero::EtwTraceEvent::Decoder> event;
  std::optional<perfetto::protos::pbzero::FileIoSimpleOpEtwEvent::Decoder>
      file_io_simple_op;
  ASSERT_NO_FATAL_FAILURE(ValidateAndDecodeFileIoSimpleOp(
      *decoders().back(), event, file_io_simple_op));

  EXPECT_EQ(event->thread_id(), kClientTid);
  EXPECT_TRUE(file_io_simple_op->has_irp_ptr());
  EXPECT_TRUE(file_io_simple_op->has_file_object());
  EXPECT_TRUE(file_io_simple_op->has_file_key());
  EXPECT_EQ(file_io_simple_op->ttid(), kClientTid);
}

// Tests that a FileIoOpEndEtwEvent is emitted for a FileIo_OpEnd ETW event.
TEST_P(EtwConsumerTestWithPrivacyFiltering, FileIoOpEndEvent) {
  constexpr uint32_t kNtStatus = 0x777;
  ProcessFileIoOpEndEvent(kClientTid, EncodeFileIoOpEnd(kClientTid, kNtStatus));
  ASSERT_EQ(decoders().size(), 1u);

  std::optional<perfetto::protos::pbzero::EtwTraceEvent::Decoder> event;
  std::optional<perfetto::protos::pbzero::FileIoOpEndEtwEvent::Decoder>
      file_io_op_end;
  ASSERT_NO_FATAL_FAILURE(
      ValidateAndDecodeFileIoOpEnd(*decoders().back(), event, file_io_op_end));

  EXPECT_EQ(event->thread_id(), kClientTid);
  EXPECT_TRUE(file_io_op_end->has_irp_ptr());
  EXPECT_TRUE(file_io_op_end->has_extra_info());
  EXPECT_EQ(file_io_op_end->nt_status(), kNtStatus);
}

// Tests that FileIo_Create events are only recorded for Chrome processes.
TEST_P(EtwConsumerTestWithPrivacyFiltering, FileIoCreateFiltering) {
  // An event is recorded if it belongs to Chrome.
  ProcessFileIoCreateEvent(
      kClientTid,
      EncodeFileIoCreate(kClientTid, 0x11, 0x22, 0x33, L"C:\\client.txt"));
  EXPECT_EQ(decoders().size(), 1u);

  // An event is not recorded if it doesn't belong to Chrome.
  ProcessFileIoCreateEvent(
      kSystemTid,
      EncodeFileIoCreate(kSystemTid, 0x44, 0x55, 0x66, L"C:\\system.txt"));
  EXPECT_EQ(decoders().size(), 1u);
  ProcessFileIoCreateEvent(
      kOtherTid,
      EncodeFileIoCreate(kOtherTid, 0x77, 0x88, 0x99, L"C:\\other.txt"));
  EXPECT_EQ(decoders().size(), 1u);
}

// Tests that FileIo_DirEnum events are only recorded for Chrome processes.
TEST_P(EtwConsumerTestWithPrivacyFiltering, FileIoDirEnumFiltering) {
  // An event is recorded if it belongs to Chrome.
  ProcessFileIoDirEnumEvent(
      kClientTid,
      EncodeFileIoDirEnum(kClientTid, 0x11, 0x22, 0x33, L"C:\\client.txt"));
  EXPECT_EQ(decoders().size(), 1u);

  // An event is not recorded if it doesn't belong to Chrome.
  ProcessFileIoDirEnumEvent(
      kSystemTid,
      EncodeFileIoDirEnum(kSystemTid, 0x44, 0x55, 0x66, L"C:\\system.txt"));
  EXPECT_EQ(decoders().size(), 1u);
  ProcessFileIoDirEnumEvent(
      kOtherTid,
      EncodeFileIoDirEnum(kOtherTid, 0x77, 0x88, 0x99, L"C:\\other.txt"));
  EXPECT_EQ(decoders().size(), 1u);
}

// Tests that FileIo_Info events are only recorded for Chrome processes.
TEST_P(EtwConsumerTestWithPrivacyFiltering, FileIoInfoFiltering) {
  // An event is recorded if it belongs to Chrome.
  ProcessFileIoInfoEvent(kClientTid, EncodeFileIoInfo(kClientTid, 0xABC));
  EXPECT_EQ(decoders().size(), 1u);

  // An event is not recorded if it doesn't belong to Chrome.
  ProcessFileIoInfoEvent(kSystemTid, EncodeFileIoInfo(kSystemTid, 0xDEF));
  EXPECT_EQ(decoders().size(), 1u);
  ProcessFileIoInfoEvent(kOtherTid, EncodeFileIoInfo(kOtherTid, 0x123));
  EXPECT_EQ(decoders().size(), 1u);
}

// Tests that FileIo_ReadWrite events are only recorded for Chrome processes.
TEST_P(EtwConsumerTestWithPrivacyFiltering, FileIoReadWriteFiltering) {
  // An event is recorded if it belongs to Chrome.
  ProcessFileIoReadWriteEvent(
      kClientTid, EncodeFileIoReadWrite(kClientTid, 0x11, 0x22, 0x33));
  EXPECT_EQ(decoders().size(), 1u);

  // An event is not recorded if it doesn't belong to Chrome.
  ProcessFileIoReadWriteEvent(
      kSystemTid, EncodeFileIoReadWrite(kSystemTid, 0x44, 0x55, 0x66));
  EXPECT_EQ(decoders().size(), 1u);
  ProcessFileIoReadWriteEvent(
      kOtherTid, EncodeFileIoReadWrite(kOtherTid, 0x77, 0x88, 0x99));
  EXPECT_EQ(decoders().size(), 1u);
}

// Tests that FileIo_SimpleOp events are only recorded for Chrome processes.
TEST_P(EtwConsumerTestWithPrivacyFiltering, FileIoSimpleOpFiltering) {
  // An event is recorded if it belongs to Chrome.
  ProcessFileIoSimpleOpEvent(kClientTid, EncodeFileIoSimpleOp(kClientTid));
  EXPECT_EQ(decoders().size(), 1u);

  // An event is not recorded if it doesn't belong to Chrome.
  ProcessFileIoSimpleOpEvent(kSystemTid, EncodeFileIoSimpleOp(kSystemTid));
  EXPECT_EQ(decoders().size(), 1u);
  ProcessFileIoSimpleOpEvent(kOtherTid, EncodeFileIoSimpleOp(kOtherTid));
  EXPECT_EQ(decoders().size(), 1u);
}

// Tests that FileIo_OpEnd events are only recorded for Chrome processes.
TEST_P(EtwConsumerTestWithPrivacyFiltering, FileIoOpEndFiltering) {
  // An event is recorded if it belongs to Chrome.
  ProcessFileIoOpEndEvent(kClientTid, EncodeFileIoOpEnd(kClientTid, 0x123));
  EXPECT_EQ(decoders().size(), 1u);

  // An event is not recorded if it doesn't belong to Chrome.
  ProcessFileIoOpEndEvent(kSystemTid, EncodeFileIoOpEnd(kSystemTid, 0x456));
  EXPECT_EQ(decoders().size(), 1u);
  ProcessFileIoOpEndEvent(kOtherTid, EncodeFileIoOpEnd(kOtherTid, 0x789));
  EXPECT_EQ(decoders().size(), 1u);
}

INSTANTIATE_TEST_SUITE_P(All,
                         EtwConsumerTestWithPrivacyFiltering,
                         testing::Bool());

}  // namespace tracing
