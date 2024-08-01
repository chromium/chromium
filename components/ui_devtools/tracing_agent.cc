// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/ui_devtools/tracing_agent.h"

#include <algorithm>
#include <memory>
#include <unordered_set>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/ui_devtools/connector_delegate.h"
#include "components/ui_devtools/devtools_base_agent.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/inspector_protocol/crdtp/json.h"

namespace ui_devtools {

using ui_devtools::protocol::Response;

namespace {

// Minimum reporting interval for the buffer usage status.
const double kMinimumReportingInterval = 250.0;

// We currently don't support concurrent tracing sessions, but are planning to.
// For the time being, we're using this flag as a workaround to prevent devtools
// users from accidentally starting two concurrent sessions.
// TODO(eseckler): Remove once we add support for concurrent sessions to the
// perfetto backend.
static bool g_any_agent_tracing = false;

base::trace_event::TraceConfig::ProcessFilterConfig CreateProcessFilterConfig(
    base::ProcessId gpu_pid) {
  base::ProcessId browser_pid = base::Process::Current().Pid();
  std::unordered_set<base::ProcessId> included_process_ids({browser_pid});

  if (gpu_pid != base::kNullProcessId)
    included_process_ids.insert(gpu_pid);

  return base::trace_event::TraceConfig::ProcessFilterConfig(
      included_process_ids);
}

}  // namespace

// This class is passed to StopTracing() and receives the trace data followed by
// a notification that data collection is over.
class TracingAgent::DevToolsTraceEndpointProxy
    : public base::RefCountedThreadSafe<DevToolsTraceEndpointProxy> {
 public:
  explicit DevToolsTraceEndpointProxy(base::WeakPtr<TracingAgent> tracing_agent)
      : tracing_agent_(tracing_agent) {}

  void ReceiveTraceChunk(std::unique_ptr<std::string> chunk) {
    if (TracingAgent* h = tracing_agent_.get())
      h->OnTraceDataCollected(std::move(chunk));
  }
  void ReceiveTraceFinalContents() {
    if (TracingAgent* h = tracing_agent_.get())
      h->OnTraceComplete();
  }

 protected:
  friend class base::RefCountedThreadSafe<DevToolsTraceEndpointProxy>;

 private:
  ~DevToolsTraceEndpointProxy() = default;
  base::WeakPtr<TracingAgent> tracing_agent_;
};

// Class used to communicate with the Perfetto Consumer interface.
class TracingAgent::PerfettoTracingSession
    : public tracing::mojom::TracingSessionClient,
      public mojo::DataPipeDrainer::Client {
 public:
  explicit PerfettoTracingSession(ConnectorDelegate* connector)
      : connector_(connector) {}

  ~PerfettoTracingSession() override { DCHECK(!tracing_active_); }

  // TracingAgent::TracingSession implementation:
  // Make request and set up perfetto for tracing.
  void EnableTracing(const base::trace_event::TraceConfig& chrome_config,
                     base::OnceClosure on_recording_enabled_callback) {
    DCHECK(!tracing_session_host_);
    DCHECK(!tracing_active_);
    tracing_active_ = true;
    connector_->BindTracingConsumerHost(
        consumer_host_.BindNewPipeAndPassReceiver());
    DCHECK(consumer_host_);
    perfetto::TraceConfig perfetto_config =
        CreatePerfettoConfiguration(chrome_config);

    mojo::PendingRemote<tracing::mojom::TracingSessionClient>
        tracing_session_client;
    receiver_.Bind(tracing_session_client.InitWithNewPipeAndPassReceiver());
    receiver_.set_disconnect_handler(
        base::BindOnce(&PerfettoTracingSession::OnTracingSessionFailed,
                       base::Unretained(this)));

    on_recording_enabled_callback_ = std::move(on_recording_enabled_callback);
    consumer_host_->EnableTracing(
        tracing_session_host_.BindNewPipeAndPassReceiver(),
        std::move(tracing_session_client), std::move(perfetto_config),
        base::File());

    tracing_session_host_.set_disconnect_handler(
        base::BindOnce(&PerfettoTracingSession::OnTracingSessionFailed,
                       base::Unretained(this)));
  }

  void OnTracingEnabled() override {
    if (on_recording_enabled_callback_) {
      std::move(on_recording_enabled_callback_).Run();
    }
  }

  void OnTracingDisabled(bool) override {
    // Since we're converting the tracing data to JSON, we will receive the
    // tracing data via ConsumerHost::DisableTracingAndEmitJson().
  }

  void DisableTracing(
      const std::string& agent_label,
      const scoped_refptr<DevToolsTraceEndpointProxy>& endpoint) {
    agent_label_ = agent_label;
    endpoint_ = endpoint;
    tracing_active_ = false;

    if (!tracing_session_host_) {
      if (endpoint_) {
        // Will delete |this|.
        endpoint_->ReceiveTraceFinalContents();
      }
      return;
    }

    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;

    MojoResult result =
        mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle);
    if (result != MOJO_RESULT_OK) {
      OnTracingSessionFailed();
      return;
    }
    drainer_ = std::make_unique<mojo::DataPipeDrainer>(
        this, std::move(consumer_handle));
    tracing_session_host_->DisableTracingAndEmitJson(
        agent_label_, std::move(producer_handle),
        /*privacy_filtering_enabled=*/false,
        base::BindOnce(&PerfettoTracingSession::OnReadBuffersComplete,
                       base::Unretained(this)));
  }

  void GetBufferUsage(base::OnceCallback<void(float percent_full,
                                              size_t approximate_event_count)>
                          on_buffer_usage_callback) {
    if (!tracing_session_host_) {
      std::move(on_buffer_usage_callback).Run(0.0f, 0);
      return;
    }
    DCHECK(on_buffer_usage_callback);
    tracing_session_host_->RequestBufferUsage(base::BindOnce(
        &PerfettoTracingSession::OnBufferUsage, base::Unretained(this),
        std::move(on_buffer_usage_callback)));
  }

  bool HasTracingFailed() { return tracing_active_ && !tracing_session_host_; }

  bool HasDataLossOccurred() { return data_loss_; }

 private:
  perfetto::TraceConfig CreatePerfettoConfiguration(
      const base::trace_event::TraceConfig& chrome_config) {
#if DCHECK_IS_ON()
    base::trace_event::TraceConfig processfilter_stripped_config(chrome_config);
    processfilter_stripped_config.SetProcessFilterConfig(
        base::trace_event::TraceConfig::ProcessFilterConfig());

    // Ensure that the process filter is the only thing that gets changed
    // in a configuration during a tracing session.
    DCHECK((last_config_for_perfetto_.ToString() ==
            base::trace_event::TraceConfig().ToString()) ||
           (last_config_for_perfetto_.ToString() ==
            processfilter_stripped_config.ToString()));
    last_config_for_perfetto_ = std::move(processfilter_stripped_config);
#endif

    return tracing::GetDefaultPerfettoConfig(
        chrome_config,
        /*privacy_filtering_enabled=*/false,
        /*convert_to_legacy_json=*/false,
        perfetto::protos::gen::ChromeConfig::USER_INITIATED);
  }

  void OnTracingSessionFailed() {
    tracing_session_host_.reset();
    receiver_.reset();
    drainer_.reset();

    if (on_recording_enabled_callback_)
      std::move(on_recording_enabled_callback_).Run();

    if (pending_disable_tracing_task_)
      std::move(pending_disable_tracing_task_).Run();

    if (endpoint_) {
      endpoint_->ReceiveTraceFinalContents();
    }
  }

  // Notify client about data loss during tracing.
  void OnBufferUsage(base::OnceCallback<void(float percent_full,
                                             size_t approximate_event_count)>
                         on_buffer_usage_callback,
                     bool success,
                     float percent_full,
                     bool data_loss) {
    if (!success) {
      std::move(on_buffer_usage_callback).Run(0.0f, 0);
      return;
    }
    data_loss_ |= data_loss;
    std::move(on_buffer_usage_callback).Run(percent_full, 0);
  }

  // mojo::DataPipeDrainer::Client implementation:
  void OnDataAvailable(base::span<const uint8_t> data) override {
    auto data_string =
        std::make_unique<std::string>(base::as_string_view(data));
    endpoint_->ReceiveTraceChunk(std::move(data_string));
  }

  void OnDataComplete() override {
    data_complete_ = true;
    MaybeTraceComplete();
  }

  void OnReadBuffersComplete() {
    read_buffers_complete_ = true;
    MaybeTraceComplete();
  }

  void MaybeTraceComplete() {
    if (read_buffers_complete_ && data_complete_) {
      // Request stats to check if data loss occurred.
      GetBufferUsage(base::BindOnce(&PerfettoTracingSession::OnFinalBufferUsage,
                                    base::Unretained(this)));
    }
  }

  void OnFinalBufferUsage(float percent_full, size_t approximate_event_count) {
    if (!endpoint_)
      return;
    // Will delete |this|.
    endpoint_->ReceiveTraceFinalContents();
  }

  mojo::Receiver<tracing::mojom::TracingSessionClient> receiver_{this};
  mojo::Remote<tracing::mojom::TracingSessionHost> tracing_session_host_;

  mojo::Remote<tracing::mojom::ConsumerHost> consumer_host_;
  raw_ptr<ConnectorDelegate> connector_;

  std::string agent_label_;
  base::OnceClosure on_recording_enabled_callback_;
  base::OnceClosure pending_disable_tracing_task_;
  scoped_refptr<DevToolsTraceEndpointProxy> endpoint_;
  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
  bool data_complete_ = false;
  bool read_buffers_complete_ = false;
  bool tracing_active_ = false;
  bool data_loss_ = false;

#if DCHECK_IS_ON()
  base::trace_event::TraceConfig last_config_for_perfetto_;
#endif
};

TracingAgent::TracingAgent(std::unique_ptr<ConnectorDelegate> connector)
    : connector_(std::move(connector)) {}

TracingAgent::~TracingAgent() = default;

namespace {
class TracingNotification : public crdtp::Serializable {
 public:
  explicit TracingNotification(std::string json) : json_(std::move(json)) {}

  void AppendSerialized(std::vector<uint8_t>* out) const override {
    crdtp::Status status =
        crdtp::json::ConvertJSONToCBOR(crdtp::SpanFrom(json_), out);
    DCHECK(status.ok()) << status.ToASCIIString();
  }

 private:
  std::string json_;
};
}  // namespace

void TracingAgent::OnTraceDataCollected(
    std::unique_ptr<std::string> trace_fragment) {
  const std::string valid_trace_fragment =
      UpdateTraceDataBuffer(*trace_fragment);
  if (valid_trace_fragment.empty())
    return;

  // Hand-craft protocol notification message so we can substitute JSON
  // that we already got as string as a bare object, not a quoted string.
  std::string message(
      "{ \"method\": \"Tracing.dataCollected\", \"params\": { \"value\": [");
  const size_t messageSuffixSize = 10;
  message.reserve(message.size() + valid_trace_fragment.size() +
                  messageSuffixSize - trace_data_buffer_state_.offset);
  message.append(valid_trace_fragment.c_str() +
                 trace_data_buffer_state_.offset);
  message += "] } }";
  frontend()->sendRawNotification(
      std::make_unique<TracingNotification>(std::move(message)));
}

void TracingAgent::OnTraceComplete() {
  if (!trace_data_buffer_state_.data.empty())
    OnTraceDataCollected(std::make_unique<std::string>(""));

  DCHECK(trace_data_buffer_state_.data.empty());
  DCHECK_EQ(0u, trace_data_buffer_state_.pos);
  DCHECK_EQ(0, trace_data_buffer_state_.open_braces);
  DCHECK(!trace_data_buffer_state_.in_string);
  DCHECK(!trace_data_buffer_state_.slashed);

  bool data_loss = perfetto_session_->HasDataLossOccurred();
  perfetto_session_.reset();
  frontend()->tracingComplete(data_loss);
}

void TracingAgent::start(
    protocol::Maybe<std::string> categories,
    protocol::Maybe<std::string> options,
    protocol::Maybe<double> buffer_usage_reporting_interval,
    std::unique_ptr<StartCallback> callback) {
  if (g_any_agent_tracing) {
    callback->sendFailure(Response::ServerError("Tracing is already started"));
    return;
  }

  if (!categories.has_value() && !options.has_value()) {
    callback->sendFailure(
        Response::InvalidParams("categories+options should be specified."));
    return;
  }

  did_initiate_recording_ = true;
  buffer_usage_reporting_interval_ =
      buffer_usage_reporting_interval.value_or(0);

  // Since we want minimum changes to the devtools frontend, enable the
  // tracing categories for ui_devtools here.
  std::string ui_devtools_categories =
      "disabled-by-default-devtools.timeline,disabled-by-default-devtools."
      "timeline.frame,views,latency,toplevel,"
      "benchmark,cc,viz,input,latency,gpu,rail,viz,ui";
  trace_config_ = base::trace_event::TraceConfig(ui_devtools_categories,
                                                 options.value_or(""));
  StartTracing(std::move(callback));
}

Response TracingAgent::end() {
  if (!perfetto_session_)
    return Response::ServerError("Tracing is not started");

  if (perfetto_session_->HasTracingFailed())
    return Response::ServerError("Tracing failed");

  scoped_refptr<DevToolsTraceEndpointProxy> endpoint;
  // Reset the trace data buffer state.
  trace_data_buffer_state_ = TracingAgent::TraceDataBufferState();
  endpoint = new DevToolsTraceEndpointProxy(weak_factory_.GetWeakPtr());
  StopTracing(endpoint, tracing::mojom::kChromeTraceEventLabel);

  return Response::Success();
}

void TracingAgent::StartTracing(std::unique_ptr<StartCallback> callback) {
  trace_config_.SetProcessFilterConfig(CreateProcessFilterConfig(gpu_pid_));

  perfetto_session_ =
      std::make_unique<PerfettoTracingSession>(connector_.get());
  perfetto_session_->EnableTracing(
      trace_config_,
      base::BindOnce(&TracingAgent::OnRecordingEnabled,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  g_any_agent_tracing = true;
}

void TracingAgent::OnRecordingEnabled(std::unique_ptr<StartCallback> callback) {
  EditTraceDataForFrontend();
  callback->sendSuccess();
  SetupTimer(buffer_usage_reporting_interval_);
}

void TracingAgent::EditTraceDataForFrontend() {
  // TracingStartedInBrowser is used to enter _processInspectorTrace,
  // which is the logic flow that browser devtools uses for the performance
  // panel. Without this trace event, devtools will use the generic trace, which
  // does not handle fps logic or use the color scheme defined by the
  // category-event map. The processes that are of interest in ui_devtools needs
  // to be specified in the data[frame] property of TracingStartedInBrowser
  auto process_data = std::make_unique<base::trace_event::TracedValue>();
  process_data->SetBoolean("persistentIds", true);
  process_data->BeginArray("frames");

  process_data->BeginDictionary();
  process_data->SetString("frame", "ui_devtools_browser_frame");
  process_data->SetString("name", "Browser");
  process_data->SetInteger("processId", base::Process::Current().Pid());
  process_data->EndDictionary();

  process_data->BeginDictionary();
  process_data->SetString("frame", "ui_devtools_gpu_frame");
  process_data->SetString("name", "Gpu");
  process_data->SetInteger("processId", gpu_pid_);
  process_data->EndDictionary();

  process_data->EndArray();
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "TracingStartedInBrowser", TRACE_EVENT_SCOPE_THREAD,
                       "data", std::move(process_data));

  // Browser devtools make sure the SetLayerTreeId trace event has the same
  // layertreeid as the layertreeid from the frame trace events 'DrawFrame' and
  // 'BeginFrame'. There is only 1 layer tree in ui_devtools, so the layertreeid
  // for all frames in ui_devtools is 1. This is used to get the frames, which
  // is later used for the fps metrics.
  auto layer_tree_data = std::make_unique<base::trace_event::TracedValue>();
  layer_tree_data->SetString("frame", "ui_devtools_browser_frame");
  layer_tree_data->SetInteger("layerTreeId", 1);
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "SetLayerTreeId", TRACE_EVENT_SCOPE_THREAD, "data",
                       std::move(layer_tree_data));
}

void TracingAgent::SetupTimer(double usage_reporting_interval) {
  if (usage_reporting_interval == 0)
    return;

  if (usage_reporting_interval < kMinimumReportingInterval)
    usage_reporting_interval = kMinimumReportingInterval;

  base::TimeDelta interval =
      base::Milliseconds(std::ceil(usage_reporting_interval));
  buffer_usage_poll_timer_ = std::make_unique<base::RepeatingTimer>();
  buffer_usage_poll_timer_->Start(
      FROM_HERE, interval,
      base::BindRepeating(&TracingAgent::UpdateBufferUsage,
                          weak_factory_.GetWeakPtr()));
}

void TracingAgent::OnBufferUsage(float percent_full,
                                 size_t approximate_event_count) {
  if (!did_initiate_recording_)
    return;
  frontend()->bufferUsage(percent_full, approximate_event_count, percent_full);
}

void TracingAgent::UpdateBufferUsage() {
  perfetto_session_->GetBufferUsage(
      base::BindOnce(&TracingAgent::OnBufferUsage, weak_factory_.GetWeakPtr()));
}

void TracingAgent::StopTracing(
    const scoped_refptr<DevToolsTraceEndpointProxy>& endpoint,
    const std::string& agent_label) {
  DCHECK(perfetto_session_);
  buffer_usage_poll_timer_.reset();
  perfetto_session_->DisableTracing(agent_label, endpoint);
  did_initiate_recording_ = false;
  g_any_agent_tracing = false;
}

std::string TracingAgent::UpdateTraceDataBuffer(
    const std::string& trace_fragment) {
  size_t end = 0;
  size_t last_open = 0;
  TraceDataBufferState& state = trace_data_buffer_state_;
  state.offset = 0;
  bool update_offset = state.open_braces == 0;
  for (; state.pos < trace_fragment.size(); ++state.pos) {
    char c = trace_fragment[state.pos];
    switch (c) {
      case '{':
        if (!state.in_string && !state.slashed) {
          state.open_braces++;
          if (state.open_braces == 1) {
            last_open = state.data.size() + state.pos;
            if (update_offset) {
              state.offset = last_open;
              update_offset = false;
            }
          }
        }
        break;
      case '}':
        if (!state.in_string && !state.slashed) {
          DCHECK_GT(state.open_braces, 0);
          state.open_braces--;
          if (state.open_braces == 0)
            end = state.data.size() + state.pos + 1;
        }
        break;
      case '"':
        if (!state.slashed)
          state.in_string = !state.in_string;
        break;
      case 'u':
        if (state.slashed)
          state.pos += 4;
        break;
    }

    if (state.in_string && c == '\\') {
      state.slashed = !state.slashed;
    } else {
      state.slashed = false;
    }
  }

  // Next starting position is usually 0 except when we are in the middle of
  // processing a unicode character, i.e. \uxxxx.
  state.pos -= trace_fragment.size();

  std::string complete_str = state.data + trace_fragment;
  state.data = complete_str.substr(std::max(end, last_open));

  complete_str.resize(end);
  return complete_str;
}

}  // namespace ui_devtools
