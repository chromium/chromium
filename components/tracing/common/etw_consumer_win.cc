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
#include "base/containers/span.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/numerics/byte_conversions.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/tracing/common/system_log_event_utils_win.h"
#include "crypto/hmac.h"
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

// Returns a `uint64_t` pointer from the next 64 bits of `iterator` on 64-bit
// systems, or the next 32 bits on 32-bit systems (zero-extending the result).
uint64_t CopyPointer(base::BufferIterator<const uint8_t>& iterator,
                     size_t pointer_size) {
  if (pointer_size == sizeof(uint32_t)) {
    return static_cast<uint64_t>(*iterator.CopyObject<uint32_t>());
  }
  return *iterator.CopyObject<uint64_t>();
}

// Reads a `uint64_t` pointer from the next 64 bits of `iterator` on 64-bit
// systems, or the next 32 bits on 32-bit systems (zero-extending the result).
// Hashes the pointer and returns the hash. A given pointer will result in the
// same hash within a session, but not between sessions.
uint64_t CopyPointerHash(base::BufferIterator<const uint8_t>& iterator,
                         size_t pointer_size) {
  const uint64_t pointer = CopyPointer(iterator, pointer_size);

  // Hash `pointer` using a random key so that the actual pointer value is
  // obscured and not reversible.
  static const base::NoDestructor<std::vector<uint8_t>> key(
      base::RandBytesAsVector(sizeof(pointer)));
  auto hash = crypto::hmac::SignSha256(*key, base::byte_span_from_ref(pointer));

  // Return the first 64 bits of `hash` as a `uint64_t`.
  return base::U64FromNativeEndian(base::span(hash).first<8u>());
}

}  // namespace

EtwConsumer::EtwConsumer(
    base::ProcessId client_pid,
    std::unique_ptr<perfetto::TraceWriterBase> trace_writer,
    bool privacy_filtering_enabled)
    : active_processes_(client_pid),
      trace_writer_(std::move(trace_writer)),
      privacy_filtering_enabled_(privacy_filtering_enabled) {
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

void EtwConsumer::Flush(std::function<void()> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  trace_writer_->Flush(std::move(callback));
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

  // Not listed in the NT kernel logger constants. GUID was obtained through
  // dumping the trace format via `tracerpt -o`. The xperf kernel provider is
  // MEMINFO, or Microsoft-Windows-Kernel-Memory. It has a GUID of
  // d1d93ef7-e1f2-4f45-9943-03d245fe6c00. Also verified via EtwExplorer.
  static constexpr GUID kMemInfoGuid = {
      0xd1d93ef7,
      0xe1f2,
      0x4f45,
      {0x99, 0x43, 0x03, 0xd2, 0x45, 0xfe, 0x6c, 0x00}};

  // FileIoGuid, 90cbdc39-4a3e-11d1-84f4-0000f80464e3
  static constexpr GUID kFileIoGuid = {
      0x90cbdc39,
      0x4a3e,
      0x11d1,
      {0x84, 0xf4, 0x00, 0x00, 0xf8, 0x04, 0x64, 0xe3}};

  // A mapping of provider GUIDs to handler member functions.
  static constexpr auto kGuidToProvider =
      base::MakeFixedFlatMap<std::reference_wrapper<const GUID>,
                             EventHandlerFunction, IsGuidLess>(
          {{kProcessGuid, &EtwConsumer::HandleProcessEvent},
           {kThreadGuid, &EtwConsumer::HandleThreadEvent},
           {kLostEventGuid, &EtwConsumer::HandleLostEvent},
           {kMemInfoGuid, &EtwConsumer::HandleMemInfoEvent},
           {kFileIoGuid, &EtwConsumer::HandleFileIoEvent}});

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
    case 50:  // ReadyThread
      if (!DecodeReadyThreadEvent(header, buffer_context, packet_data)) {
        DLOG(ERROR) << "Error decoding ReadyThread Event";
      }
      break;
    case 72:  // ThreadSetName (v2)
      OnThreadSetName(header, buffer_context, packet_data);
      break;
    default:
      break;
  }
}

void EtwConsumer::HandleFileIoEvent(const EVENT_HEADER& header,
                                    const ETW_BUFFER_CONTEXT& buffer_context,
                                    size_t pointer_size,
                                    base::span<const uint8_t> packet_data) {
  if (!inclusion_policy_.ShouldRecordFileIoEvents(header.ThreadId)) {
    return;
  }

  switch (header.EventDescriptor.Opcode) {
    case 64:  // FileIo_Create
      if (!DecodeFileIoCreateEvent(header, buffer_context, pointer_size,
                                   packet_data)) {
        DLOG(ERROR) << "Error decoding FileIo_Create event";
      }
      break;
    case 72:
    case 77:  // FileIo_DirEnum
      if (!DecodeFileIoDirEnumEvent(header, buffer_context, pointer_size,
                                    packet_data)) {
        DLOG(ERROR) << "Error decoding FileIo_DirEnum event";
      }
      break;
    case 67:
    case 68:  // FileIo_ReadWrite
      if (!DecodeFileIoReadWriteEvent(header, buffer_context, pointer_size,
                                      packet_data)) {
        DLOG(ERROR) << "Error decoding FileIo_ReadWrite event";
      }
      break;
    case 69:
    case 70:
    case 71:
    case 74:
    case 75:  // FileIo_Info
      if (!DecodeFileIoInfoEvent(header, buffer_context, pointer_size,
                                 packet_data)) {
        DLOG(ERROR) << "Error decoding FileIo_Info event";
      }
      break;
    case 65:
    case 66:
    case 73:  // FileIo_SimpleOp
      if (!DecodeFileIoSimpleOpEvent(header, buffer_context, pointer_size,
                                     packet_data)) {
        DLOG(ERROR) << "Error decoding FileIo_SimpleOp event";
      }
      break;
    case 76:  // FileIo_OpEnd
      if (!DecodeFileIoOpEndEvent(header, buffer_context, pointer_size,
                                  packet_data)) {
        DLOG(ERROR) << "Error decoding FileIo_OpEnd event";
      }
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

void EtwConsumer::HandleMemInfoEvent(const EVENT_HEADER& header,
                                     const ETW_BUFFER_CONTEXT& buffer_context,
                                     size_t pointer_size,
                                     base::span<const uint8_t> packet_data) {
  switch (header.EventDescriptor.Opcode) {
    case EVENT_TRACE_TYPE_INFO:
      if (header.EventDescriptor.Id == 1) {
        // MemInfo_V1
        OnMemoryCounters(header, buffer_context, pointer_size, packet_data);
      }
      break;
    default:
      break;
  }
}

void EtwConsumer::OnMemoryCounters(const EVENT_HEADER& header,
                                   const ETW_BUFFER_CONTEXT& buffer_context,
                                   size_t pointer_size,
                                   base::span<const uint8_t> packet_data) {
  // This parses a MemInfoArgs_V1 struct.

  base::BufferIterator<const uint8_t> iterator(packet_data);
  std::optional<uint8_t> priority_levels_count_parsed =
      iterator.CopyObject<uint8_t>();
  if (!priority_levels_count_parsed.has_value()) {
    return;
  }
  uint8_t priority_levels_count = *priority_levels_count_parsed;

  // The true number of pointers of the struct is 10 + (2 *
  // `priority_levels_count`).
  if (packet_data.size() <
      pointer_size * (10 + (2 * priority_levels_count)) + 1) {
    return;
  }

  // Generate a memory counter event.
  perfetto::protos::pbzero::MemInfoEtwEvent* meminfo_event =
      MakeNextEvent(header, buffer_context)->set_mem_info();
  meminfo_event->set_priority_levels(priority_levels_count);
  meminfo_event->set_zero_page_count(CopyPointer(iterator, pointer_size));
  meminfo_event->set_free_page_count(CopyPointer(iterator, pointer_size));
  meminfo_event->set_modified_page_count(CopyPointer(iterator, pointer_size));
  meminfo_event->set_modified_no_write_page_count(
      CopyPointer(iterator, pointer_size));
  meminfo_event->set_bad_page_count(CopyPointer(iterator, pointer_size));
  for (int i = 0; i < priority_levels_count; ++i) {
    meminfo_event->add_standby_page_counts(CopyPointer(iterator, pointer_size));
  }
  for (int i = 0; i < priority_levels_count; ++i) {
    meminfo_event->add_repurposed_page_counts(
        CopyPointer(iterator, pointer_size));
  }
  meminfo_event->set_modified_page_count_page_file(
      CopyPointer(iterator, pointer_size));
  meminfo_event->set_paged_pool_page_count(CopyPointer(iterator, pointer_size));
  meminfo_event->set_non_paged_pool_page_count(
      CopyPointer(iterator, pointer_size));
  meminfo_event->set_mdl_page_count(CopyPointer(iterator, pointer_size));
  meminfo_event->set_commit_page_count(CopyPointer(iterator, pointer_size));
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

bool EtwConsumer::DecodeReadyThreadEvent(
    const EVENT_HEADER& header,
    const ETW_BUFFER_CONTEXT& buffer_context,
    base::span<const uint8_t> packet_data) {
  using perfetto::protos::pbzero::ReadyThreadEtwEvent;

  // Size of ReadyThread v2 in bytes (1 x 32-bit plus 4 x 8-bit).
  static constexpr size_t kMinimumReadyThreadLength = 1 * 4 + 4;
  if (packet_data.size() < kMinimumReadyThreadLength) {
    return false;
  }

  // Read and validate the contents of `packet_data`.
  base::BufferIterator<const uint8_t> iterator{packet_data};
  auto thread_id = *iterator.CopyObject<uint32_t>();
  auto adjust_reason = *iterator.Object<int8_t>();
  auto adjust_increment = *iterator.Object<int8_t>();
  auto flag = *iterator.Object<int8_t>();

  // Generate a ReadyThreadEtwEvent.
  auto* event = MakeNextEvent(header, buffer_context);
  if (inclusion_policy_.ShouldIncludeThreadId(header.ThreadId)) {
    event->set_thread_id(header.ThreadId);
  }
  auto* ready_thread = event->set_ready_thread();
  if (inclusion_policy_.ShouldIncludeThreadId(thread_id)) {
    ready_thread->set_t_thread_id(thread_id);
  }
  ready_thread->set_adjust_reason_int(adjust_reason);
  ready_thread->set_adjust_increment(adjust_increment);
  ready_thread->set_flag_int(flag);
  return true;
}

bool EtwConsumer::DecodeFileIoCreateEvent(
    const EVENT_HEADER& header,
    const ETW_BUFFER_CONTEXT& buffer_context,
    size_t pointer_size,
    base::span<const uint8_t> packet_data) {
  using perfetto::protos::pbzero::FileIoCreateEtwEvent;

  // Size of `FileIo_Create` event:
  //   2 pointers + 4 `uint32`s + wide string contents + wide string terminator.
  // Check that `packet_data` is large enough to hold at least the pointers,
  // integers, and wide string terminator.
  const size_t kMinimumSize =
      2 * pointer_size + 4 * sizeof(uint32_t) + sizeof(wchar_t);
  if (packet_data.size() < kMinimumSize) {
    return false;
  }

  // Read the contents of `packet_data` and generate a `FileIoCreate` event.
  base::BufferIterator<const uint8_t> iterator{packet_data};
  auto* event = MakeNextEvent(header, buffer_context);
  event->set_thread_id(header.ThreadId);
  auto* file_io_create = event->set_file_io_create();
  file_io_create->set_irp_ptr(CopyPointerHash(iterator, pointer_size));
  file_io_create->set_file_object(CopyPointerHash(iterator, pointer_size));
  file_io_create->set_ttid(*iterator.CopyObject<uint32_t>());
  file_io_create->set_create_options(*iterator.CopyObject<uint32_t>());
  file_io_create->set_file_attributes(*iterator.CopyObject<uint32_t>());
  file_io_create->set_share_access(*iterator.CopyObject<uint32_t>());
  if (!privacy_filtering_enabled_) {
    file_io_create->set_open_path(base::WideToUTF8(*CopyWString(iterator)));
  }

  return true;
}

bool EtwConsumer::DecodeFileIoDirEnumEvent(
    const EVENT_HEADER& header,
    const ETW_BUFFER_CONTEXT& buffer_context,
    size_t pointer_size,
    base::span<const uint8_t> packet_data) {
  using perfetto::protos::pbzero::FileIoDirEnumEtwEvent;

  // Size of `FileIo_DirEnum` event:
  //   3 pointers + 4 `uint32`s + wide string contents + wide string terminator.
  // Check that `packet_data` is large enough to hold at least the pointers,
  // integer, and wide string terminator.
  const size_t kMinimumSize =
      3 * pointer_size + 4 * sizeof(uint32_t) + sizeof(wchar_t);
  if (packet_data.size() < kMinimumSize) {
    return false;
  }

  // Read the contents of `packet_data` and generate a `FileIoDirEnum` event.
  base::BufferIterator<const uint8_t> iterator{packet_data};
  auto* event = MakeNextEvent(header, buffer_context);
  event->set_thread_id(header.ThreadId);
  auto* file_io_dir_enum = event->set_file_io_dir_enum();
  file_io_dir_enum->set_irp_ptr(CopyPointerHash(iterator, pointer_size));
  file_io_dir_enum->set_file_object(CopyPointerHash(iterator, pointer_size));
  file_io_dir_enum->set_file_key(CopyPointerHash(iterator, pointer_size));
  file_io_dir_enum->set_ttid(*iterator.CopyObject<uint32_t>());
  file_io_dir_enum->set_length(*iterator.CopyObject<uint32_t>());
  file_io_dir_enum->set_info_class(*iterator.CopyObject<uint32_t>());
  file_io_dir_enum->set_file_index(*iterator.CopyObject<uint32_t>());
  if (!privacy_filtering_enabled_) {
    file_io_dir_enum->set_file_name(base::WideToUTF8(*CopyWString(iterator)));
  }
  return true;
}

bool EtwConsumer::DecodeFileIoInfoEvent(
    const EVENT_HEADER& header,
    const ETW_BUFFER_CONTEXT& buffer_context,
    size_t pointer_size,
    base::span<const uint8_t> packet_data) {
  using perfetto::protos::pbzero::FileIoInfoEtwEvent;

  // Size of `FileIo_Info` event: 4 pointers + 2 `uint32`s.
  const size_t kMinimumSize = 4 * pointer_size + 2 * sizeof(uint32_t);
  if (packet_data.size() < kMinimumSize) {
    return false;
  }

  // Read the contents of `packet_data` and generate a `FileIoInfo` event.
  base::BufferIterator<const uint8_t> iterator{packet_data};
  auto* event = MakeNextEvent(header, buffer_context);
  event->set_thread_id(header.ThreadId);
  auto* file_io_info = event->set_file_io_info();
  file_io_info->set_irp_ptr(CopyPointerHash(iterator, pointer_size));
  file_io_info->set_file_object(CopyPointerHash(iterator, pointer_size));
  file_io_info->set_file_key(CopyPointerHash(iterator, pointer_size));
  file_io_info->set_extra_info(CopyPointer(iterator, pointer_size));
  file_io_info->set_ttid(*iterator.CopyObject<uint32_t>());
  file_io_info->set_info_class(*iterator.CopyObject<uint32_t>());
  return true;
}

bool EtwConsumer::DecodeFileIoReadWriteEvent(
    const EVENT_HEADER& header,
    const ETW_BUFFER_CONTEXT& buffer_context,
    size_t pointer_size,
    base::span<const uint8_t> packet_data) {
  using perfetto::protos::pbzero::FileIoReadWriteEtwEvent;

  // Size of `FileIo_ReadWrite` event: 1 uint64 + 3 pointers + 3 `uint32`s.
  const size_t kMinimumSize =
      sizeof(uint64_t) + 3 * pointer_size + 3 * sizeof(uint32_t);
  if (packet_data.size() < kMinimumSize) {
    return false;
  }

  // Read the contents of `packet_data` and generate a `FileIoReadWrite` event.
  base::BufferIterator<const uint8_t> iterator{packet_data};
  auto* event = MakeNextEvent(header, buffer_context);
  event->set_thread_id(header.ThreadId);
  auto* file_io_read_write = event->set_file_io_read_write();
  file_io_read_write->set_offset(*iterator.CopyObject<uint64_t>());
  file_io_read_write->set_irp_ptr(CopyPointerHash(iterator, pointer_size));
  file_io_read_write->set_file_object(CopyPointerHash(iterator, pointer_size));
  file_io_read_write->set_file_key(CopyPointerHash(iterator, pointer_size));
  file_io_read_write->set_ttid(*iterator.CopyObject<uint32_t>());
  file_io_read_write->set_io_size(*iterator.CopyObject<uint32_t>());
  file_io_read_write->set_io_flags(*iterator.CopyObject<uint32_t>());
  return true;
}

bool EtwConsumer::DecodeFileIoSimpleOpEvent(
    const EVENT_HEADER& header,
    const ETW_BUFFER_CONTEXT& buffer_context,
    size_t pointer_size,
    base::span<const uint8_t> packet_data) {
  using perfetto::protos::pbzero::FileIoSimpleOpEtwEvent;

  // Size of `FileIo_SimpleOp` event: 3 pointers + 1 uint32.
  const size_t kMinimumSize = 3 * pointer_size + sizeof(uint32_t);
  if (packet_data.size() < kMinimumSize) {
    return false;
  }

  // Read the contents of `packet_data` and generate a `FileIoSimpleOp` event.
  base::BufferIterator<const uint8_t> iterator{packet_data};
  auto* event = MakeNextEvent(header, buffer_context);
  event->set_thread_id(header.ThreadId);
  auto* file_io_simple_op = event->set_file_io_simple_op();
  file_io_simple_op->set_irp_ptr(CopyPointerHash(iterator, pointer_size));
  file_io_simple_op->set_file_object(CopyPointerHash(iterator, pointer_size));
  file_io_simple_op->set_file_key(CopyPointerHash(iterator, pointer_size));
  file_io_simple_op->set_ttid(*iterator.CopyObject<uint32_t>());
  return true;
}

bool EtwConsumer::DecodeFileIoOpEndEvent(
    const EVENT_HEADER& header,
    const ETW_BUFFER_CONTEXT& buffer_context,
    size_t pointer_size,
    base::span<const uint8_t> packet_data) {
  using perfetto::protos::pbzero::FileIoOpEndEtwEvent;

  // Size of `FileIo_OpEnd` event: 2 pointers + 1 uint32.
  const size_t kMinimumSize = 2 * pointer_size + sizeof(uint32_t);
  if (packet_data.size() < kMinimumSize) {
    return false;
  }

  // Read the contents of `packet_data` and generate a `FileIoOpEnd` event.
  base::BufferIterator<const uint8_t> iterator{packet_data};
  auto* event = MakeNextEvent(header, buffer_context);
  event->set_thread_id(header.ThreadId);
  auto* file_io_op_end = event->set_file_io_op_end();
  file_io_op_end->set_irp_ptr(CopyPointerHash(iterator, pointer_size));
  file_io_op_end->set_extra_info(CopyPointer(iterator, pointer_size));
  file_io_op_end->set_nt_status(*iterator.CopyObject<uint32_t>());
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
