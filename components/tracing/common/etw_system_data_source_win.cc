// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/tracing/common/etw_system_data_source_win.h"

#include <windows.h>

#include <evntcons.h>
#include <evntrace.h>

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/buffer_iterator.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/win/event_trace_consumer.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_descriptor.h"
#include "third_party/perfetto/protos/perfetto/config/data_source_config.gen.h"
#include "third_party/perfetto/protos/perfetto/config/etw/etw_config.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/etw/etw.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/etw/etw_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/etw/etw_event_bundle.pbzero.h"

namespace tracing {

namespace {

constexpr wchar_t kEtwSystemSessionName[] = L"org.chromium.etw_system";
constexpr uint8_t kCSwitchEventOpcode = 36;

ULONG EtwSystemFlagsFromEnum(
    perfetto::protos::gen::EtwConfig::KernelFlag flag) {
  switch (flag) {
    case perfetto::protos::gen::EtwConfig::CSWITCH:
      return EVENT_TRACE_FLAG_CSWITCH;
    case perfetto::protos::gen::EtwConfig::DISPATCHER:
      return EVENT_TRACE_FLAG_DISPATCHER;
  }
}

}  // namespace

class EtwSystemDataSource::Consumer
    : public base::win::EtwTraceConsumerBase<Consumer> {
 public:
  static constexpr bool kEnableRecordMode = true;
  static constexpr bool kRawTimestamp = true;

  explicit Consumer(std::unique_ptr<perfetto::TraceWriterBase>);
  ~Consumer();

  static void ProcessEventRecord(EVENT_RECORD* event_record);
  static bool ProcessBuffer(EVENT_TRACE_LOGFILE* buffer);

  void ConsumeEvents();

 private:
  void ProcessEventRecordImpl(const EVENT_HEADER* header,
                              const ETW_BUFFER_CONTEXT* buffer_context,
                              base::span<uint8_t> packet_data)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  std::unique_ptr<perfetto::TraceWriterBase> trace_writer_
      GUARDED_BY_CONTEXT(sequence_checker_);
  perfetto::TraceWriter::TracePacketHandle packet_handle_
      GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<perfetto::protos::pbzero::EtwTraceEventBundle> etw_events_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

void EtwSystemDataSource::Register() {
  perfetto::DataSourceDescriptor desc;
  desc.set_name("org.chromium.etw_system");
  perfetto::DataSource<EtwSystemDataSource>::Register(desc);
}

EtwSystemDataSource::EtwSystemDataSource()
    : consume_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(),
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      consumer_{nullptr, base::OnTaskRunnerDeleter(nullptr)} {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

EtwSystemDataSource::~EtwSystemDataSource() = default;

void EtwSystemDataSource::OnSetup(const SetupArgs& args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_source_config_ = *args.config;
}

void EtwSystemDataSource::OnStart(const StartArgs&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (data_source_config_.etw_config_raw().empty()) {
    DLOG(ERROR) << "Skipping empty etw_config";
    return;
  }
  perfetto::protos::gen::EtwConfig etw_config;
  if (!etw_config.ParseFromString(data_source_config_.etw_config_raw())) {
    DLOG(ERROR) << "Failed to parse etw_config";
    return;
  }

  base::win::EtwTraceProperties ignore;
  HRESULT hr =
      base::win::EtwTraceController::Stop(kEtwSystemSessionName, &ignore);
  if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_WMI_INSTANCE_NOT_FOUND)) {
    DLOG(ERROR) << "Failed to stop previous trace session: 0x" << std::hex
                << hr;
    return;
  }

  // MD5 hash of "org.chromium.etw_system".
  // 696e5ace-ff06-bfc6-4e63-0198f5b3bc99
  static constexpr GUID kChromeEtwSystemGuid = {
      0x696e5ace,
      0xff06,
      0xbfc6,
      {0x4e, 0x63, 0x01, 0x98, 0xf5, 0xb3, 0xbc, 0x99}};

  base::win::EtwTraceProperties prop;
  EVENT_TRACE_PROPERTIES& p = *prop.get();

  // QPC timer accuracy.
  // https://learn.microsoft.com/en-us/windows/win32/etw/wnode-header
  p.Wnode.ClientContext = 1;
  // Windows 8 and later supports SystemTraceProvider in multiple
  // private session that's not NT Kernel Logger.
  // https://learn.microsoft.com/en-us/windows/win32/etw/configuring-and-starting-a-systemtraceprovider-session#enable-a-systemtraceprovider-session
  prop.SetLoggerName(kEtwSystemSessionName);
  p.Wnode.Guid = kChromeEtwSystemGuid;
  p.LogFileMode = EVENT_TRACE_REAL_TIME_MODE | EVENT_TRACE_SYSTEM_LOGGER_MODE;
  p.MinimumBuffers = 16;
  p.BufferSize = 16;
  p.FlushTimer = 1;  // flush every second.

  for (auto flag : etw_config.kernel_flags()) {
    p.EnableFlags |= EtwSystemFlagsFromEnum(flag);
  }

  hr = etw_controller_.Start(kEtwSystemSessionName, &prop);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to start system trace session: 0x" << std::hex << hr;
    return;
  }

  consumer_ = {new Consumer(CreateTraceWriter()),
               base::OnTaskRunnerDeleter(consume_task_runner_)};
  hr = consumer_->OpenRealtimeSession(kEtwSystemSessionName);
  if (FAILED(hr)) {
    etw_controller_.Stop(nullptr);
    DLOG(ERROR) << "Failed to open system trace session: 0x" << std::hex << hr;
    return;
  }
  consume_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&EtwSystemDataSource::Consumer::ConsumeEvents,
                                base::Unretained(consumer_.get())));
}

void EtwSystemDataSource::OnStop(const StopArgs&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  etw_controller_.Stop(nullptr);
  consumer_.reset();
}

EtwSystemDataSource::Consumer::Consumer(
    std::unique_ptr<perfetto::TraceWriterBase> trace_writer)
    : trace_writer_(std::move(trace_writer)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

// Decodes a CSwitch Event.
// https://learn.microsoft.com/en-us/windows/win32/etw/cswitch
// Returns true on success, or false if `packet_data` is invalid.
bool EtwSystemDataSource::DecodeCSwitchEvent(
    base::span<uint8_t> packet_data,
    perfetto::protos::pbzero::EtwTraceEvent& event) {
  // Size of CSwitch v2 in bytes.
  static constexpr size_t kMinimumCSwitchLength = 24;
  if (packet_data.size() < kMinimumCSwitchLength) {
    return false;
  }
  base::BufferIterator<uint8_t> iterator{packet_data};
  auto* c_switch = event.set_c_switch();
  c_switch->set_new_thread_id(*iterator.Object<uint32_t>());
  c_switch->set_old_thread_id(*iterator.Object<uint32_t>());
  c_switch->set_new_thread_priority(*iterator.Object<int8_t>());
  c_switch->set_old_thread_priority(*iterator.Object<int8_t>());
  c_switch->set_previous_c_state(*iterator.Object<uint8_t>());

  // SpareByte
  std::ignore = iterator.Object<uint8_t>();

  const int8_t* old_thread_wait_reason = iterator.Object<int8_t>();
  if (!(*old_thread_wait_reason >= 0 &&
        *old_thread_wait_reason <
            perfetto::protos::pbzero::CSwitchEtwEvent::MAXIMUM_WAIT_REASON)) {
    return false;
  }
  c_switch->set_old_thread_wait_reason(
      static_cast<
          perfetto::protos::pbzero::CSwitchEtwEvent::OldThreadWaitReason>(
          *old_thread_wait_reason));

  const int8_t* old_thread_wait_mode = iterator.Object<int8_t>();
  if (!(*old_thread_wait_mode >= 0 &&
        *old_thread_wait_mode <=
            perfetto::protos::pbzero::CSwitchEtwEvent::USER_MODE)) {
    return false;
  }
  c_switch->set_old_thread_wait_mode(
      static_cast<perfetto::protos::pbzero::CSwitchEtwEvent::OldThreadWaitMode>(
          *old_thread_wait_mode));

  const int8_t* old_thread_state = iterator.Object<int8_t>();
  if (!(*old_thread_state >= 0 &&
        *old_thread_state <=
            perfetto::protos::pbzero::CSwitchEtwEvent::DEFERRED_READY)) {
    return false;
  }
  c_switch->set_old_thread_state(
      static_cast<perfetto::protos::pbzero::CSwitchEtwEvent::OldThreadState>(
          *old_thread_state));

  c_switch->set_old_thread_wait_ideal_processor(*iterator.Object<int8_t>());
  c_switch->set_new_thread_wait_time(*iterator.Object<uint32_t>());
  return true;
}

EtwSystemDataSource::Consumer::~Consumer() = default;

void EtwSystemDataSource::Consumer::ConsumeEvents() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedBlockingCall scoped_blocking(FROM_HERE,
                                           base::BlockingType::MAY_BLOCK);
  Consume();
}

void EtwSystemDataSource::Consumer::ProcessEventRecord(
    EVENT_RECORD* event_record) {
  // ThreadGuid, 3d6fa8d1-fe05-11d0-9dda-00c04fd7ba7c
  // https://learn.microsoft.com/en-us/windows/win32/etw/nt-kernel-logger-constants
  static constexpr GUID kThreadGuid = {
      0x3d6fa8d1,
      0xfe05,
      0x11d0,
      {0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c}};

  Consumer* self = reinterpret_cast<Consumer*>(event_record->UserContext);
  DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);

  if (IsEqualGUID(event_record->EventHeader.ProviderId, kThreadGuid)) {
    self->ProcessEventRecordImpl(&event_record->EventHeader,
                                 &event_record->BufferContext,
                                 {static_cast<uint8_t*>(event_record->UserData),
                                  event_record->UserDataLength});
  }
}

void EtwSystemDataSource::Consumer::ProcessEventRecordImpl(
    const EVENT_HEADER* header,
    const ETW_BUFFER_CONTEXT* buffer_context,
    base::span<uint8_t> packet_data) {
  static const int64_t qpc_ticks_per_second = []() {
    LARGE_INTEGER perf_counter_frequency = {};
    ::QueryPerformanceFrequency(&perf_counter_frequency);
    CHECK_GT(perf_counter_frequency.QuadPart, 0);
    return perf_counter_frequency.QuadPart;
  }();

  uint64_t now =
      static_cast<uint64_t>(base::Time::kNanosecondsPerSecond *
                            static_cast<double>(header->TimeStamp.QuadPart) /
                            static_cast<double>(qpc_ticks_per_second));
  if (!etw_events_) {
    // Resetting the `packet_handle_` finalizes previous data.
    packet_handle_ = trace_writer_->NewTracePacket();
    packet_handle_->set_timestamp(now);
    etw_events_ = packet_handle_->set_etw_events();
  }

  auto* event = etw_events_->add_event();
  event->set_timestamp(now);
  event->set_cpu(buffer_context->ProcessorIndex);
  if (header->EventDescriptor.Opcode == kCSwitchEventOpcode) {
    bool result = DecodeCSwitchEvent(packet_data, *event);
    if (!result) {
      DLOG(ERROR) << "Error decoding CSwitch Event";
    }
  }
}

bool EtwSystemDataSource::Consumer::ProcessBuffer(EVENT_TRACE_LOGFILE* buffer) {
  Consumer* self = reinterpret_cast<Consumer*>(buffer->Context);
  DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
  self->etw_events_ = nullptr;
  self->packet_handle_ = {};
  return true;
}

}  // namespace tracing

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS(
    BASE_EXPORT,
    tracing::EtwSystemDataSource);

// This should go after PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS
// to avoid instantiation of type() template method before specialization.
std::unique_ptr<perfetto::TraceWriterBase>
tracing::EtwSystemDataSource::CreateTraceWriter() {
  perfetto::internal::DataSourceStaticState* static_state =
      perfetto::DataSourceHelper<EtwSystemDataSource>::type().static_state();
  // EtwSystemDataSource disallows multiple instances, so our instance will
  // always have index 0.
  perfetto::internal::DataSourceState* instance_state = static_state->TryGet(0);
  CHECK(instance_state);
  return perfetto::internal::TracingMuxer::Get()->CreateTraceWriter(
      static_state, data_source_config_.target_buffer(), instance_state,
      perfetto::BufferExhaustedPolicy::kDrop);
}
