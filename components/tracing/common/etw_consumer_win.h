// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_ETW_CONSUMER_WIN_H_
#define COMPONENTS_TRACING_COMMON_ETW_CONSUMER_WIN_H_

#include <stdint.h>

#include <memory>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/win/event_trace_consumer.h"
#include "components/tracing/common/active_processes_win.h"
#include "components/tracing/common/inclusion_policy_win.h"
#include "components/tracing/tracing_export.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/tracing/trace_writer_base.h"

namespace perfetto::protos::pbzero {
class EtwTraceEvent;
class EtwTraceEventBundle;
}  // namespace perfetto::protos::pbzero

namespace tracing {

// A consumer of events from the Windows system trace provider that emits
// corresponding Perfetto trace events. An instance may be constructed on any
// sequence. Its `ConsumeEvents()` method and its destructor must be called on
// the same sequence.
class TRACING_EXPORT EtwConsumer
    : public base::win::EtwTraceConsumerBase<EtwConsumer> {
 public:
  // Receive events in the new EVENT_RECORD format.
  static constexpr bool kEnableRecordMode = true;
  // Do not convert timestampts to system time.
  static constexpr bool kRawTimestamp = true;

  // Constructs an instance that will consume ETW events on behalf of the client
  // process identified by `client_pid` and emit Perfetto events via
  // `trace_writer`.
  EtwConsumer(base::ProcessId client_pid,
              std::unique_ptr<perfetto::TraceWriterBase> trace_writer);
  EtwConsumer(const EtwConsumer&) = delete;
  EtwConsumer& operator=(const EtwConsumer&) = delete;
  ~EtwConsumer();

  // Consumes ETW events; blocking the calling thread. Returns when the ETW
  // trace session is stopped.
  void ConsumeEvents();

  // base::win::EtwTraceConsumerBase<>:
  static void ProcessEventRecord(EVENT_RECORD* event_record);
  static bool ProcessBuffer(EVENT_TRACE_LOGFILE* buffer);

 private:
  friend class EtwConsumerTest;

  // The type of a member function that handles an event originating from a
  // specific provider.
  using EventHandlerFunction =
      void (EtwConsumer::*)(const EVENT_HEADER& header,
                            const ETW_BUFFER_CONTEXT& buffer_context,
                            size_t pointer_size,
                            base::span<const uint8_t> packet_data);

  // Returns the size, in bytes, of a pointer-sized value in an event based on
  // the `Flags` member of an event's `EVENT_HEADER`.
  static size_t GetPointerSize(uint16_t event_header_flags);

  // Per-provider event handlers. `ProcessEventRecord` dispatches to these based
  // on the ProviderId in the record's EventHeader.
  void HandleProcessEvent(const EVENT_HEADER& header,
                          const ETW_BUFFER_CONTEXT& buffer_context,
                          size_t pointer_size,
                          base::span<const uint8_t> packet_data)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void HandleThreadEvent(const EVENT_HEADER& header,
                         const ETW_BUFFER_CONTEXT& buffer_context,
                         size_t pointer_size,
                         base::span<const uint8_t> packet_data)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void HandleLostEvent(const EVENT_HEADER& header,
                       const ETW_BUFFER_CONTEXT& buffer_context,
                       size_t pointer_size,
                       base::span<const uint8_t> packet_data)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  void OnProcessStart(const EVENT_HEADER& header,
                      const ETW_BUFFER_CONTEXT& buffer_context,
                      size_t pointer_size,
                      base::span<const uint8_t> packet_data)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void OnProcessEnd(const EVENT_HEADER& header,
                    const ETW_BUFFER_CONTEXT& buffer_context,
                    size_t pointer_size,
                    base::span<const uint8_t> packet_data)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void OnThreadStart(const EVENT_HEADER& header,
                     const ETW_BUFFER_CONTEXT& buffer_context,
                     size_t pointer_size,
                     base::span<const uint8_t> packet_data)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void OnThreadEnd(const EVENT_HEADER& header,
                   const ETW_BUFFER_CONTEXT& buffer_context,
                   size_t pointer_size,
                   base::span<const uint8_t> packet_data)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void OnThreadSetName(const EVENT_HEADER& header,
                       const ETW_BUFFER_CONTEXT& buffer_context,
                       base::span<const uint8_t> packet_data)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Decodes a CSwitch Event and emits a Perfetto trace event; see
  // https://learn.microsoft.com/en-us/windows/win32/etw/cswitch.
  // Returns true on success, or false if `packet_data` is invalid.
  bool DecodeCSwitchEvent(const EVENT_HEADER& header,
                          const ETW_BUFFER_CONTEXT& buffer_context,
                          base::span<const uint8_t> packet_data)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Returns a new perfetto trace event to be emitted for an ETW event with a
  // given event header. The timestamp and cpu fields of the returned event are
  // prepopulated.
  perfetto::protos::pbzero::EtwTraceEvent* MakeNextEvent(
      const EVENT_HEADER& header,
      const ETW_BUFFER_CONTEXT& buffer_context)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  const ActiveProcesses& active_processes() const { return active_processes_; }

  ActiveProcesses active_processes_ GUARDED_BY_CONTEXT(sequence_checker_);
  InclusionPolicy inclusion_policy_ GUARDED_BY_CONTEXT(sequence_checker_){
      active_processes_};
  std::unique_ptr<perfetto::TraceWriterBase> trace_writer_
      GUARDED_BY_CONTEXT(sequence_checker_);
  perfetto::TraceWriter::TracePacketHandle packet_handle_
      GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<perfetto::protos::pbzero::EtwTraceEventBundle> etw_events_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_ETW_CONSUMER_WIN_H_
