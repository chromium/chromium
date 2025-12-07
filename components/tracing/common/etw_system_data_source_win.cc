// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/etw_system_data_source_win.h"

#include <windows.h>

#include <evntcons.h>
#include <evntrace.h>

#include <ios>

#include "base/check.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "components/tracing/common/etw_consumer_win.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_descriptor.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/chrome_config.gen.h"
#include "third_party/perfetto/protos/perfetto/config/etw/etw_config.gen.h"

namespace tracing {

namespace {

ULONG EtwSystemFlagsFromSchedulerProvider(std::string_view keyword) {
  if (keyword == "CONTEXT_SWITCH") {
    return EVENT_TRACE_FLAG_CSWITCH;
  } else if (keyword == "DISPATCHER") {
    return EVENT_TRACE_FLAG_DISPATCHER;
  }
  return 0;
}

ULONG EtwSystemFlagsFromFileProvider(std::string_view keyword) {
  if (keyword == "FILE_IO") {
    return EVENT_TRACE_FLAG_FILE_IO | EVENT_TRACE_FLAG_FILE_IO_INIT;
  }
  return 0;
}

ULONG EtwMemoryProviderFlagFromKeyword(std::string_view keyword) {
  if (keyword == "MEMINFO") {
    return 0x20U;  // KERNEL_MEM_KEYWORD_MEMINFO
  }
  return 0;
}

}  // namespace

// static
void EtwSystemDataSource::Register(base::ProcessId client_pid) {
  perfetto::DataSourceDescriptor desc;
  desc.set_name("org.chromium.etw_system");
  perfetto::DataSource<EtwSystemDataSource>::Register(desc, client_pid);
}

EtwSystemDataSource::EtwSystemDataSource(base::ProcessId client_pid)
    : client_pid_(client_pid),
      consume_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(),
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      consumer_{nullptr, base::OnTaskRunnerDeleter(nullptr)} {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

EtwSystemDataSource::~EtwSystemDataSource() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void EtwSystemDataSource::OnSetup(const SetupArgs& args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_source_config_ = *args.config;
}

void EtwSystemDataSource::OnStart(const StartArgs&) {
  static constexpr wchar_t kEtwSystemSessionName[] = L"org.chromium.etw_system";

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
  // Enable process and thread events for categorization and filtering.
  p.EnableFlags = EVENT_TRACE_FLAG_PROCESS | EVENT_TRACE_FLAG_THREAD;

  for (const auto& keyword : etw_config.scheduler_provider_events()) {
    p.EnableFlags |= EtwSystemFlagsFromSchedulerProvider(keyword);
  }
  for (const auto& keyword : etw_config.file_provider_events()) {
    p.EnableFlags |= EtwSystemFlagsFromFileProvider(keyword);
  }

  // The ETW Session must be started (but not opened) before providers can be
  // enabled.
  hr = etw_controller_.Start(kEtwSystemSessionName, &prop);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to start system trace session: 0x" << std::hex << hr;
    return;
  }

  ULONG memory_provider_flags = 0;
  for (auto keyword : etw_config.memory_provider_events()) {
    memory_provider_flags |= EtwMemoryProviderFlagFromKeyword(keyword);
  }
  // There is a SystemMemoryProviderGuid in <evntrace.h>, however, that is not
  // the actual provider for the Microsoft-Windows-Kernel-Memory. This is the
  // real GUID.
  static constexpr GUID kKernelMemoryProviderGuid = {
      0xd1d93ef7,
      0xe1f2,
      0x4f45,
      {0x99, 0x43, 0x03, 0xd2, 0x45, 0xfe, 0x6c, 0x00}};
  etw_controller_.EnableProvider(kKernelMemoryProviderGuid,
                                 TRACE_LEVEL_INFORMATION,
                                 memory_provider_flags);

  bool privacy_filtering_enabled =
      data_source_config_.chrome_config().privacy_filtering_enabled();
  consumer_ = {new EtwConsumer(client_pid_, CreateTraceWriter(),
                               privacy_filtering_enabled),
               base::OnTaskRunnerDeleter(consume_task_runner_)};
  hr = consumer_->OpenRealtimeSession(kEtwSystemSessionName);
  if (FAILED(hr)) {
    etw_controller_.Stop(nullptr);
    consumer_.reset();
    DLOG(ERROR) << "Failed to open system trace session: 0x" << std::hex << hr;
    return;
  }
  consume_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&EtwConsumer::ConsumeEvents,
                                base::Unretained(consumer_.get())));
}

void EtwSystemDataSource::OnStop(const StopArgs& args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (consumer_) {
    consume_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&EtwConsumer::Flush, base::Unretained(consumer_.get()),
                       args.HandleStopAsynchronously()));
    consumer_.reset();
  }
  etw_controller_.Stop(nullptr);
}

}  // namespace tracing

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS(
    TRACING_EXPORT,
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
