// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/etw_consumer_win.h"

#include <windows.h>

#include <functional>
#include <tuple>
#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/buffer_iterator.h"
#include "base/containers/fixed_flat_map.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/tracing/common/system_log_event_utils_win.h"
#include "third_party/perfetto/protos/perfetto/trace/etw/etw.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/etw/etw_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/etw/etw_event_bundle.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace tracing {

namespace {

// A function object that returns true if one GUID is "less than" another.
struct IsGuidLess {
  constexpr bool operator()(const GUID& a, const GUID& b) const {
    if (auto result = std::tie(a.Data1, a.Data2, a.Data3) <=>
                      std::tie(b.Data1, b.Data2, b.Data3);
        result < 0) {
      return true;
    } else if (result > 0) {
      return false;
    }
    return base::span(a.Data4) < base::span(b.Data4);
  }
};

}  // namespace

EtwConsumer::EtwConsumer(
    base::ProcessId client_pid,
    std::unique_ptr<perfetto::TraceWriterBase> trace_writer)
    : active_processes_(client_pid), trace_writer_(std::move(trace_writer)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

EtwConsumer::~EtwConsumer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void EtwConsumer::ConsumeEvents() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedBlockingCall scoped_blocking(FROM_HERE,
                                           base::BlockingType::MAY_BLOCK);
  Consume();
}

// static
void EtwConsumer::ProcessEventRecord(EVENT_RECORD* event_record) {
  // https://learn.microsoft.com/en-us/windows/win32/etw/nt-kernel-logger-constants
  // LostEventGuid, 6a399ae0-4bc6-4de9-870b-3657f8947e7e
  static constexpr GUID kLostEventGuid = {
      0x6a399ae0,
      0x4bc6,
      0x4de9,
      {0x87, 0x0b, 0x36, 0x57, 0xf8, 0x94, 0x7e, 0x7e}};
  // ProcessGuid, 3d6fa8d0-fe05-11d0-9dda-00c04fd7ba7c
  static constexpr GUID kProcessGuid = {
      0x3d6fa8d0,
      0xfe05,
      0x11d0,
      {0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c}};
  // ThreadGuid, 3d6fa8d1-fe05-11d0-9dda-00c04fd7ba7c
  static constexpr GUID kThreadGuid = {
      0x3d6fa8d1,
      0xfe05,
      0x11d0,
      {0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c}};

  // A mapping of provider GUIDs to handler member functions.
  static constexpr auto kGuidToProvider =
      base::MakeFixedFlatMap<std::reference_wrapper<const GUID>,
                             EventHandlerFunction, IsGuidLess>(
          {{kProcessGuid, &EtwConsumer::HandleProcessEvent},
           {kThreadGuid, &EtwConsumer::HandleThreadEvent},
           {kLostEventGuid, &EtwConsumer::HandleLostEvent}});

  auto* const self = reinterpret_cast<EtwConsumer*>(event_record->UserContext);
  DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);

  if (auto iter = kGuidToProvider.find(event_record->EventHeader.ProviderId);
      iter != kGuidToProvider.end()) {
    // Dispatch to the handler function for the record's provider. To understand
    // the encoded format of messages from the Windows system trace provider,
    // see the "MOF class definitions" and "MOF class qualifiers" documents at
    // https://learn.microsoft.com/windows/win32/etw/event-tracing-reference.
    (self->*iter->second)(
        event_record->EventHeader, event_record->BufferContext,
        GetPointerSize(event_record->EventHeader.Flags),
        // SAFETY: The pointer and length originate from ETW.
        UNSAFE_BUFFERS({static_cast<uint8_t*>(event_record->UserData),
                        event_record->UserDataLength}));
  }
  // The following providers are always enabled. There is not yet a need to
  // handle any events originating from them:
  // - EventTraceGuid: 68fdd900-4a3e-11d1-84f4-0000f80464e3
  //   - Opcode 32: EndExtension / Event Trace Header Extension
  //   - Opcode 5: Extension / Event Trace Header Extension
  //   - Opcode 8: RDComplete / Event Trace Rundown Complete
  // - EventTraceConfigGuid: 01853a65-418f-4f36-aefc-dc0f1d2fd235
  //   - Various hardware configuration events.
}

// static
bool EtwConsumer::ProcessBuffer(EVENT_TRACE_LOGFILE* buffer) {
  auto* const self = reinterpret_cast<EtwConsumer*>(buffer->Context);
  DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
  self->etw_events_ = nullptr;
  // Release the handle to finalize the previous message.
  self->packet_handle_ = {};
  return true;  // Continue processing events.
}

// static
size_t EtwConsumer::GetPointerSize(uint16_t event_header_flags) {
  // Default to the native pointer size with the expectation that, in the
  // general case, the bitness of this binary matches the bitness of the OS.
#if defined(ARCH_CPU_64_BITS)
  static constexpr size_t kThisPointerSize = 8;
  static constexpr size_t kOtherPointerSize = 4;
  static constexpr uint16_t kOtherSizeFlag = EVENT_HEADER_FLAG_32_BIT_HEADER;
#elif defined(ARCH_CPU_32_BITS)
  static constexpr size_t kThisPointerSize = 4;
  static constexpr size_t kOtherPointerSize = 8;
  static constexpr uint16_t kOtherSizeFlag = EVENT_HEADER_FLAG_64_BIT_HEADER;
#else
#error Unsupported architecture
#endif
  return (event_header_flags & kOtherSizeFlag) == kOtherSizeFlag
             ? kOtherPointerSize
             : kThisPointerSize;
}

void EtwConsumer::HandleProcessEvent(const EVENT_HEADER& header,
                                     const ETW_BUFFER_CONTEXT& buffer_context,
                                     size_t pointer_size,
                                     base::span<const uint8_t> packet_data) {
  switch (header.EventDescriptor.Opcode) {
    case EVENT_TRACE_TYPE_START:
    case EVENT_TRACE_TYPE_DC_START:
      OnProcessStart(header, buffer_context, pointer_size, packet_data);
      break;
    case EVENT_TRACE_TYPE_END:
    case EVENT_TRACE_TYPE_DC_END:
      OnProcessEnd(header, buffer_context, pointer_size, packet_data);
      break;
    default:
      // 32: PerfCtr
      // 33: PerfCtrRundown
      // 39: Defunct
      break;
  }
}

void EtwConsumer::HandleThreadEvent(const EVENT_HEADER& header,
                                    const ETW_BUFFER_CONTEXT& buffer_context,
                                    size_t pointer_size,
                                    base::span<const uint8_t> packet_data) {
  switch (header.EventDescriptor.Opcode) {
    case EVENT_TRACE_TYPE_START:
    case EVENT_TRACE_TYPE_DC_START:
      OnThreadStart(header, buffer_context, pointer_size, packet_data);
      break;
    case EVENT_TRACE_TYPE_END:
    case EVENT_TRACE_TYPE_DC_END:
      OnThreadEnd(header, buffer_context, pointer_size, packet_data);
      break;
    case 36:  // CSwitch
      if (!DecodeCSwitchEvent(header, buffer_context, packet_data)) {
        DLOG(ERROR) << "Error decoding CSwitch Event";
      }
      break;
    case 72:  // ThreadSetName (v2)
      OnThreadSetName(header, buffer_context, packet_data);
      break;
    default:
      // 50: ReadyThread
      break;
  }
}

void EtwConsumer::HandleLostEvent(const EVENT_HEADER& header,
                                  const ETW_BUFFER_CONTEXT& buffer_context,
                                  size_t pointer_size,
                                  base::span<const uint8_t> packet_data) {
  switch (header.EventDescriptor.Opcode) {
    case 32:  // RTLostEvent
      // TODO: Emit a Perfetto event for this?
      DLOG(ERROR) << "One or more events lost during trace capture";
      break;
    case 33:  // RTLostBuffer
      // TODO: Emit a Perfetto event for this?
      DLOG(ERROR) << "One or more buffers lost during trace capture";
      break;
    default:
      // 34:  // RTLostFile
      break;
  }
}

void EtwConsumer::OnProcessStart(const EVENT_HEADER& header,
                                 const ETW_BUFFER_CONTEXT& buffer_context,
                                 size_t pointer_size,
                                 base::span<const uint8_t> packet_data) {
  const auto event_version = header.EventDescriptor.Version;

  base::BufferIterator<const uint8_t> iterator(packet_data);

  if (event_version >= 1) {
    // Skip PageDirectoryBase (v1) or UniqueProcessKey (higher)
    (void)iterator.Span<uint8_t>(pointer_size);
  }

  uint32_t pid;
  uint32_t parent_pid;
  if (event_version == 0) {
    // V0 begins with pointer-sized ProcessId and ParentId.
    const size_t kMinimumSize = pointer_size * 2;
    if (iterator.total_size() - iterator.position() < kMinimumSize) {
      return;
    }
    if (pointer_size == sizeof(uint32_t)) {
      pid = *iterator.CopyObject<uint32_t>();
      parent_pid = *iterator.CopyObject<uint32_t>();
    } else {
      pid = base::checked_cast<uint32_t>(*iterator.CopyObject<uint64_t>());
      parent_pid =
          base::checked_cast<uint32_t>(*iterator.CopyObject<uint64_t>());
    }
  } else {
    // All other versions have 32-bit ProcessId and ParentId.
    static constexpr size_t kMinimumSize = 8;
    if (iterator.total_size() - iterator.position() < kMinimumSize) {
      return;
    }
    pid = *iterator.CopyObject<uint32_t>();
    parent_pid = *iterator.CopyObject<uint32_t>();
  }

  uint32_t session_id = 0;
  if (event_version >= 1) {
    if (auto value = iterator.CopyObject<uint32_t>(); value.has_value()) {
      session_id = *value;
    } else {
      return;  // Ran out of data prematurely.
    }
    (void)iterator.Object<int32_t>();  // ExitStatus
  }
  if (event_version >= 3) {
    (void)iterator.Span<uint8_t>(pointer_size);  // DirectoryTableBase
  }

  auto user_sid = CopySid(pointer_size, iterator);  // UserSID
  if (!user_sid.has_value()) {
    return;  // Malformed SID or ran out of data.
  }

  std::string image_file_name;
  if (auto value = CopyString(iterator); value.has_value()) {  // ImageFileName
    image_file_name = *std::move(value);
  } else {
    return;  // Malformed or ran out of data.
  }

  std::wstring command_line;
  if (event_version >= 2) {
    if (auto value = CopyWString(iterator); value.has_value()) {  // CommandLine
      command_line = *std::move(value);
    } else {
      return;  // Malformed or ran out of data.
    }
  }

  active_processes_.AddProcess(pid, parent_pid, session_id, std::move(user_sid),
                               std::move(image_file_name),
                               std::move(command_line));
}

void EtwConsumer::OnProcessEnd(const EVENT_HEADER& header,
                               const ETW_BUFFER_CONTEXT& buffer_context,
                               size_t pointer_size,
                               base::span<const uint8_t> packet_data) {
  uint32_t process_id;

  base::BufferIterator<const uint8_t> iterator(packet_data);
  if (header.EventDescriptor.Version == 0) {
    // V0 begins with a pointer-sized ProcessId.
    const size_t kMinimumSize = pointer_size;
    if (packet_data.size() < kMinimumSize) {
      return;
    }
    if (pointer_size == sizeof(uint32_t)) {
      process_id = *iterator.CopyObject<uint32_t>();
    } else {
      process_id =
          base::checked_cast<uint32_t>(*iterator.CopyObject<uint64_t>());
    }
  } else {
    // All other versions have the 32-bit pid after a pointer-sized value.
    const size_t kMinimumSize = pointer_size + 4;
    if (packet_data.size() < kMinimumSize) {
      return;
    }
    // Skip PageDirectoryBase (v1) or UniqueProcessKey (others)
    (void)iterator.Span<uint8_t>(pointer_size);
    process_id = *iterator.CopyObject<uint32_t>();
  }

  active_processes_.RemoveProcess(process_id);
}

void EtwConsumer::OnThreadStart(const EVENT_HEADER& header,
                                const ETW_BUFFER_CONTEXT& buffer_context,
                                size_t pointer_size,
                                base::span<const uint8_t> packet_data) {
  static constexpr size_t kMinimumSize = 2 * 4;  // Two 32-bit ints.
  if (packet_data.size() < kMinimumSize) {
    return;
  }

  uint32_t pid = 0;
  uint32_t tid = 0;
  std::wstring thread_name;

  base::BufferIterator<const uint8_t> iterator(packet_data);
  if (header.EventDescriptor.Version == 0) {
    tid = *iterator.CopyObject<uint32_t>();
    pid = *iterator.CopyObject<uint32_t>();
  } else {
    pid = *iterator.CopyObject<uint32_t>();
    tid = *iterator.CopyObject<uint32_t>();

    // A v4 Thread event will have the thread name after seven pointers,
    // one 32-bit int, and four 8-bit ints.
    const size_t kSkipV4Fields = 7 * pointer_size + 8;
    (void)iterator.Span<uint8_t>(kSkipV4Fields);
    // Read the name if there is room remaining for at least a wide terminator.
    if (iterator.total_size() - iterator.position() > 2) {
      thread_name = *CopyWString(iterator);
    }
  }

  active_processes_.AddThread(pid, tid, std::move(thread_name));
}

void EtwConsumer::OnThreadEnd(const EVENT_HEADER& header,
                              const ETW_BUFFER_CONTEXT& buffer_context,
                              size_t pointer_size,
                              base::span<const uint8_t> packet_data) {
  static constexpr size_t kMinimumSize = 2 * 4;  // Two 32-bit ints.
  if (packet_data.size() < kMinimumSize) {
    return;
  }

  uint32_t pid = 0;
  uint32_t tid = 0;

  base::BufferIterator<const uint8_t> iterator(packet_data);
  if (header.EventDescriptor.Version == 0) {
    tid = *iterator.CopyObject<uint32_t>();
    pid = *iterator.CopyObject<uint32_t>();
  } else {
    pid = *iterator.CopyObject<uint32_t>();
    tid = *iterator.CopyObject<uint32_t>();
  }

  active_processes_.RemoveThread(pid, tid);
}

void EtwConsumer::OnThreadSetName(const EVENT_HEADER& header,
                                  const ETW_BUFFER_CONTEXT& buffer_context,
                                  base::span<const uint8_t> packet_data) {
  // Two 32-bit ints plus a wide string terminator.
  static constexpr size_t kMinimumSize = 2 * 4 + 2;
  if (packet_data.size() < kMinimumSize) {
    return;
  }

  base::BufferIterator<const uint8_t> iterator(packet_data);
  auto pid = *iterator.CopyObject<uint32_t>();
  auto tid = *iterator.CopyObject<uint32_t>();
  active_processes_.SetThreadName(pid, tid, *CopyWString(iterator));
}

bool EtwConsumer::DecodeCSwitchEvent(const EVENT_HEADER& header,
                                     const ETW_BUFFER_CONTEXT& buffer_context,
                                     base::span<const uint8_t> packet_data) {
  using perfetto::protos::pbzero::CSwitchEtwEvent;

  // Size of CSwitch v2 in bytes (4 x 32-bit plus 8 x 8-bit).
  static constexpr size_t kMinimumCSwitchLength = 4 * 4 + 8;
  if (packet_data.size() < kMinimumCSwitchLength) {
    return false;
  }

  // Read and validate the contents of `packet_data`.
  base::BufferIterator<const uint8_t> iterator{packet_data};
  auto new_thread_id = *iterator.CopyObject<uint32_t>();
  auto old_thread_id = *iterator.CopyObject<uint32_t>();
  auto new_thread_priority = *iterator.Object<int8_t>();
  auto old_thread_priority = *iterator.Object<int8_t>();
  auto previous_c_state = *iterator.Object<uint8_t>();
  (void)iterator.Object<int8_t>();  // SpareByte
  auto old_thread_wait_reason = *iterator.Object<int8_t>();
  auto old_thread_wait_mode = *iterator.Object<int8_t>();
  auto old_thread_state = *iterator.Object<int8_t>();
  auto old_thread_wait_ideal_processor = *iterator.Object<int8_t>();
  auto new_thread_wait_time = *iterator.CopyObject<uint32_t>();
  (void)iterator.Object<uint32_t>();  // Reserved

  // Generate a CSwitchEtwEvent.
  auto* c_switch = MakeNextEvent(header, buffer_context)->set_c_switch();
  if (inclusion_policy_.ShouldIncludeThreadId(new_thread_id)) {
    c_switch->set_new_thread_id(new_thread_id);
  }
  if (inclusion_policy_.ShouldIncludeThreadId(old_thread_id)) {
    c_switch->set_old_thread_id(old_thread_id);
  }
  c_switch->set_new_thread_priority(new_thread_priority);
  c_switch->set_old_thread_priority(old_thread_priority);
  c_switch->set_previous_c_state(previous_c_state);
  c_switch->set_old_thread_wait_reason_int(old_thread_wait_reason);
  c_switch->set_old_thread_wait_mode_int(old_thread_wait_mode);
  c_switch->set_old_thread_state_int(old_thread_state);
  c_switch->set_old_thread_wait_ideal_processor(
      old_thread_wait_ideal_processor);
  c_switch->set_new_thread_wait_time(new_thread_wait_time);

  return true;
}

perfetto::protos::pbzero::EtwTraceEvent* EtwConsumer::MakeNextEvent(
    const EVENT_HEADER& header,
    const ETW_BUFFER_CONTEXT& buffer_context) {
  static const double qpc_ticks_per_second = []() {
    LARGE_INTEGER perf_counter_frequency = {};
    ::QueryPerformanceFrequency(&perf_counter_frequency);
    double frequency = static_cast<double>(perf_counter_frequency.QuadPart);
    CHECK_GT(frequency, 0.0);
    return frequency;
  }();

  uint64_t now = static_cast<uint64_t>(
      base::Time::kNanosecondsPerSecond *
      static_cast<double>(header.TimeStamp.QuadPart) / qpc_ticks_per_second);
  if (!etw_events_) {
    // Resetting the `packet_handle_` finalizes previous data.
    packet_handle_ = trace_writer_->NewTracePacket();
    packet_handle_->set_timestamp(now);
    etw_events_ = packet_handle_->set_etw_events();
  }

  auto* event = etw_events_->add_event();
  event->set_timestamp(now);
  event->set_cpu(buffer_context.ProcessorIndex);
  return event;
}

}  // namespace tracing
