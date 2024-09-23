// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_ETW_SYSTEM_DATA_SOURCE_WIN_H_
#define COMPONENTS_TRACING_COMMON_ETW_SYSTEM_DATA_SOURCE_WIN_H_

#include <memory>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/win/event_trace_controller.h"
#include "components/tracing/tracing_export.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/tracing/core/forward_decls.h"
#include "third_party/perfetto/include/perfetto/tracing/data_source.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "third_party/perfetto/protos/perfetto/trace/etw/etw_event.pbzero.h"

namespace tracing {

class TRACING_EXPORT EtwSystemDataSource
    : public perfetto::DataSource<EtwSystemDataSource> {
 public:
  static constexpr bool kSupportsMultipleInstances = false;

  static void Register();

  EtwSystemDataSource();
  ~EtwSystemDataSource() override;

  void OnSetup(const SetupArgs&) override;
  void OnStart(const StartArgs&) override;
  void OnStop(const StopArgs&) override;

  static bool DecodeCSwitchEvent(
      base::span<uint8_t> packet_data,
      perfetto::protos::pbzero::EtwTraceEvent& event);

 private:
  class Consumer;

  std::unique_ptr<perfetto::TraceWriterBase> CreateTraceWriter();

  base::win::EtwTraceController etw_controller_
      GUARDED_BY_CONTEXT(sequence_checker_);
  scoped_refptr<base::SequencedTaskRunner> consume_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<Consumer, base::OnTaskRunnerDeleter> consumer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  perfetto::DataSourceConfig data_source_config_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_ETW_SYSTEM_DATA_SOURCE_WIN_H_
