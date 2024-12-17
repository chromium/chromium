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

#include <ios>

#include "base/check.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "components/tracing/common/etw_consumer_win.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_descriptor.h"
#include "third_party/perfetto/protos/perfetto/config/etw/etw_config.gen.h"

namespace tracing {

namespace {

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

  for (auto flag : etw_config.kernel_flags()) {
    p.EnableFlags |= EtwSystemFlagsFromEnum(flag);
  }

  hr = etw_controller_.Start(kEtwSystemSessionName, &prop);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to start system trace session: 0x" << std::hex << hr;
    return;
  }

  consumer_ = {new EtwConsumer(client_pid_, CreateTraceWriter()),
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

void EtwSystemDataSource::OnStop(const StopArgs&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  etw_controller_.Stop(nullptr);
  consumer_.reset();
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
