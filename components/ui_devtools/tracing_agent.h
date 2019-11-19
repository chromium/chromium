// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_TRACING_AGENT_H_
#define COMPONENTS_UI_DEVTOOLS_TRACING_AGENT_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/trace_config.h"
#include "components/ui_devtools/Tracing.h"
#include "components/ui_devtools/devtools_base_agent.h"

namespace base {
class RepeatingTimer;
}

namespace ui_devtools {

class ConnectorDelegate;

// This class is used for tracing in the ui_devtools performance panel.
// A lot of the code is based on TracingHandler from
// content/browser/devtools/protocol/tracing_handler.h.
class UI_DEVTOOLS_EXPORT TracingAgent
    : public UiDevToolsBaseAgent<protocol::Tracing::Metainfo> {
 public:
  explicit TracingAgent(std::unique_ptr<ConnectorDelegate> connector);
  ~TracingAgent() override;

  void set_gpu_pid(base::ProcessId pid) { gpu_pid_ = pid; }

  // Sends the Tracing JSON data in the form of CBOR to the frontend.
  void OnTraceDataCollected(std::unique_ptr<std::string> trace_fragment);

  // Signals that tracing is complete and notifies any data loss to the
  // frontend.
  void OnTraceComplete();

  // Tracing::Backend:
  void start(protocol::Maybe<std::string> categories,
             protocol::Maybe<std::string> options,
             protocol::Maybe<double> buffer_usage_reporting_interval,
             std::unique_ptr<StartCallback> callback) override;
  protocol::Response end() override;

 private:
  class DevToolsTraceEndpointProxy;
  class PerfettoTracingSession;

  struct TraceDataBufferState {
   public:
    std::string data;
    size_t pos = 0;
    int open_braces = 0;
    bool in_string = false;
    bool slashed = false;
    size_t offset = 0;
  };

  // Returns the longest prefix of |trace_fragment| that is a valid list and
  // stores the rest (tail) to be used in subsequent calls. This returns the
  // longest prefix of the tail prepended to |trace_fragment| next time. Assumes
  // that the input is a potentially incomplete string representation of a comma
  // separated list of JSON objects.
  std::string UpdateTraceDataBuffer(const std::string& trace_fragment);

  // Sets TraceConfig to only collect trace events for the specified processes
  // in this method. Currently, only the browser process is specified. Starts
  // tracing by attempting to enable tracing via perfetto.
  void StartTracing(std::unique_ptr<StartCallback>);

  // Called when we have successfully started tracing with Perfetto session.
  void OnRecordingEnabled(std::unique_ptr<StartCallback> callback);

  // Edits tracing data to use the normal devtools frontend logic to display
  // the performance metrics. Without this, it will use the devtools generic
  // trace logic to display the performance metrics.
  void EditTraceDataForFrontend();

  // Sets up repeating timer to request the trace buffer status from the
  // perfetto tracing session. If usage_reporting_interval is too small, it will
  // be clipped to a minimum value.
  void SetupTimer(double usage_reporting_interval);

  // Sends frontend information on buffer usage such as how much of the buffer
  // is used and the approximate number of events in the trace log.
  void OnBufferUsage(float percent_full, size_t approximate_event_count);

  // Gets updated buffer usage from the perfetto tracing session.
  void UpdateBufferUsage();

  // Resets buffer usage and passes the trace data to the trace data endpoint
  // consumer.
  void StopTracing(const scoped_refptr<DevToolsTraceEndpointProxy>& endpoint,
                   const std::string& agent_label);

  std::unique_ptr<base::RepeatingTimer> buffer_usage_poll_timer_;
  std::unique_ptr<ConnectorDelegate> connector_;
  base::ProcessId gpu_pid_ = base::kNullProcessId;
  bool did_initiate_recording_ = false;
  double buffer_usage_reporting_interval_ = 0;
  base::trace_event::TraceConfig trace_config_;
  std::unique_ptr<PerfettoTracingSession> perfetto_session_;
  TraceDataBufferState trace_data_buffer_state_;
  base::WeakPtrFactory<TracingAgent> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(TracingAgent);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_TRACING_AGENT_H_
