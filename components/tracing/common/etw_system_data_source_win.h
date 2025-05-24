// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_ETW_SYSTEM_DATA_SOURCE_WIN_H_
#define COMPONENTS_TRACING_COMMON_ETW_SYSTEM_DATA_SOURCE_WIN_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/win/event_trace_controller.h"
#include "components/tracing/tracing_export.h"
#include "third_party/perfetto/include/perfetto/tracing/data_source.h"

namespace perfetto {
class TraceWriterBase;
}

namespace tracing {

class EtwConsumer;

class TRACING_EXPORT EtwSystemDataSource
    : public perfetto::DataSource<EtwSystemDataSource> {
 public:
  static constexpr bool kSupportsMultipleInstances = false;

  // Registers the data source. `client_pid` identifies the client process on
  // behalf of which traces are collected. If specified, events are filtered to
  // exclude certain details from non-Chrome processes. No filtering is
  // performed when a client pid is not provided.
  static void Register(base::ProcessId client_pid = base::kNullProcessId);

  explicit EtwSystemDataSource(base::ProcessId client_pid);
  EtwSystemDataSource(const EtwSystemDataSource&) = delete;
  EtwSystemDataSource& operator=(const EtwSystemDataSource&) = delete;
  ~EtwSystemDataSource() override;

  // perfetto::DataSource:
  void OnSetup(const SetupArgs&) override;
  void OnStart(const StartArgs&) override;
  void OnStop(const StopArgs&) override;

 private:
  std::unique_ptr<perfetto::TraceWriterBase> CreateTraceWriter();

  // Identifier of the process on behalf of which traces are collected.
  const base::ProcessId client_pid_;
  base::win::EtwTraceController etw_controller_
      GUARDED_BY_CONTEXT(sequence_checker_);
  scoped_refptr<base::SequencedTaskRunner> consume_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<EtwConsumer, base::OnTaskRunnerDeleter> consumer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  perfetto::DataSourceConfig data_source_config_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_ETW_SYSTEM_DATA_SOURCE_WIN_H_
