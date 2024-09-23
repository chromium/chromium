// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/v8_detailed_memory_decorator.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/execution_context/execution_context_attached_data.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/public/v8_memory/v8_detailed_memory.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/process_type.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/tokens/tokens.h"

using blink::ExecutionContextToken;
using blink::mojom::PerContextCanvasMemoryUsagePtr;
using blink::mojom::PerContextV8MemoryUsagePtr;

namespace performance_manager {

namespace v8_memory {

class V8DetailedMemoryRequestQueue {
 public:
  V8DetailedMemoryRequestQueue() = default;

  ~V8DetailedMemoryRequestQueue();

  const V8DetailedMemoryRequest* GetNextRequest() const;
  const V8DetailedMemoryRequest* GetNextBoundedRequest() const;

  void AddMeasurementRequest(V8DetailedMemoryRequest* request);

  // Removes |request| if it is part of this queue, and returns the number of
  // elements removed (will be 0 or 1).
  size_t RemoveMeasurementRequest(V8DetailedMemoryRequest* request);

  void NotifyObserversOnMeasurementAvailable(
      const ProcessNode* process_node) const;

  void OnOwnerUnregistered();

  // Check the data invariant on the measurement request lists.
  void Validate();

 private:
  void ApplyToAllRequests(
      base::FunctionRef<void(V8DetailedMemoryRequest*)> func) const;

  // Lists of requests sorted by min_time_between_requests (lowest first).
  std::vector<raw_ptr<V8DetailedMemoryRequest, VectorExperimental>>
      bounded_measurement_requests_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::vector<raw_ptr<V8DetailedMemoryRequest, VectorExperimental>>
      lazy_measurement_requests_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

// This class is allowed to access
// V8DetailedMemoryDecorator::NotifyObserversOnMeasurementAvailable.
class V8DetailedMemoryDecorator::ObserverNotifier {
 public:
  void NotifyObserversOnMeasurementAvailable(const ProcessNode* process_node) {
    auto* decorator =
        V8DetailedMemoryDecorator::GetFromGraph(process_node->GetGraph());
    if (decorator)
      decorator->NotifyObserversOnMeasurementAvailable(
          base::PassKey<ObserverNotifier>(), process_node);
  }
};

namespace {

using MeasurementMode = V8DetailedMemoryRequest::MeasurementMode;

// Forwards the pending receiver to the RenderProcessHost and binds it on the
// UI thread.
void BindReceiverOnUIThread(
    mojo::PendingReceiver<blink::mojom::V8DetailedMemoryReporter>
        pending_receiver,
    RenderProcessHostProxy proxy) {
  auto* render_process_host = proxy.Get();
  if (render_process_host) {
    render_process_host->BindReceiver(std::move(pending_receiver));
  }
}

bool IsMeasurementBounded(MeasurementMode mode) {
  switch (mode) {
    case MeasurementMode::kLazy:
      return false;
    case MeasurementMode::kBounded:
      return true;
    case MeasurementMode::kEagerForTesting:
      return true;
  }
}

// Returns the higher priority request of |a| and |b|, either of which can be
// null, or nullptr if both are null.
const V8DetailedMemoryRequest* ChooseHigherPriorityRequest(
    const V8DetailedMemoryRequest* a,
    const V8DetailedMemoryRequest* b) {
  if (!a)
    return b;
  if (!b)
    return a;
  if (a->min_time_between_requests() < b->min_time_between_requests())
    return a;
  if (b->min_time_between_requests() < a->min_time_between_requests())
    return b;
  // Break ties by prioritizing bounded requests.
  if (IsMeasurementBounded(a->mode()))
    return a;
  return b;
}

// May only be used from the performance manager sequence.
internal::BindV8DetailedMemoryReporterCallback* g_test_bind_callback = nullptr;

// Per-frame memory measurement involves the following classes that live on the
// PM sequence:
//
// V8DetailedMemoryDecorator: Central rendezvous point. Coordinates
//     V8DetailedMemoryRequest and V8DetailedMemoryObserver objects. Owned by
//     the graph; created the first time
//     V8DetailedMemoryRequest::StartMeasurement is called.
//     TODO(b/1080672): Currently this lives forever; should be cleaned up when
//     there are no more measurements scheduled.
//
// V8DetailedMemoryRequest: Indicates that a caller wants memory to be measured
//     at a specific interval. Owned by the caller but must live on the PM
//     sequence. V8DetailedMemoryRequest objects register themselves with
//     V8DetailedMemoryDecorator on creation and unregister themselves on
//     deletion, which cancels the corresponding measurement.
//
// V8DetailedMemoryRequestQueue: A priority queue of memory requests. The
//     decorator will hold a global queue of requests that measure every
//     process, and each ProcessNode will have a queue of requests that measure
//     only that process.
//
// NodeAttachedProcessData: Private class that schedules measurements and holds
//     the results for an individual process. Owned by the ProcessNode; created
//     when measurements start.
//     TODO(b/1080672): Currently this lives forever; should be cleaned up when
//     there are no more measurements scheduled.
//
// V8DetailedMemoryProcessData: Public accessor to the measurement results held
//     in a NodeAttachedProcessData, which owns it.
//
// ExecutionContextAttachedData: Private class that holds the measurement
//     results for an execution context. Owned by the ExecutionContext; created
//     when a measurement result arrives.
//     TODO(b/1080672): Currently this lives forever; should be cleaned up when
//     there are no more measurements scheduled.
//
// V8DetailedMemoryExecutionContextData: Public accessor to the measurement
//     results held in a ExecutionContextAttachedData, which owns it.
//
// V8DetailedMemoryObserver: Callers can implement this and register with
//     V8DetailedMemoryDecorator::AddObserver() to be notified when
//     measurements are available for a process. Owned by the caller but must
//     live on the PM sequence.
//
// Additional wrapper classes can access these classes from other sequences:
//
// V8DetailedMemoryRequestAnySeq: Wraps V8DetailedMemoryRequest. Owned by the
//     caller and lives on any sequence.
//
// V8DetailedMemoryObserverAnySeq: Callers can implement this and register it
//     with V8DetailedMemoryRequestAnySeq::AddObserver() to be notified when
//     measurements are available for a process. Owned by the caller and lives
//     on the same sequence as the V8DetailedMemoryRequestAnySeq.

////////////////////////////////////////////////////////////////////////////////
// ExecutionContextAttachedData

class ExecutionContextAttachedData
    : public execution_context::ExecutionContextAttachedData<
          ExecutionContextAttachedData> {
 public:
  explicit ExecutionContextAttachedData(
      const execution_context::ExecutionContext* ec) {}
  ~ExecutionContextAttachedData() override = default;

  ExecutionContextAttachedData(const ExecutionContextAttachedData&) = delete;
  ExecutionContextAttachedData& operator=(const ExecutionContextAttachedData&) =
      delete;

  const V8DetailedMemoryExecutionContextData* data() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return data_available_ ? &data_ : nullptr;
  }

  static V8DetailedMemoryExecutionContextData*
  GetOrCreateForTesting(  // IN-TEST
      const execution_context::ExecutionContext* ec);

 private:
  friend class NodeAttachedProcessData;

  V8DetailedMemoryExecutionContextData data_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool data_available_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  SEQUENCE_CHECKER(sequence_checker_);
};

// static
V8DetailedMemoryExecutionContextData*
ExecutionContextAttachedData::GetOrCreateForTesting(
    const execution_context::ExecutionContext* ec) {
  auto* ec_data = ExecutionContextAttachedData::GetOrCreate(ec);
  DCHECK_CALLED_ON_VALID_SEQUENCE(ec_data->sequence_checker_);
  ec_data->data_available_ = true;
  return &ec_data->data_;
}

////////////////////////////////////////////////////////////////////////////////
// NodeAttachedProcessData

class NodeAttachedProcessData
    : public ExternalNodeAttachedDataImpl<NodeAttachedProcessData> {
 public:
  explicit NodeAttachedProcessData(const ProcessNode* process_node);
  ~NodeAttachedProcessData() override = default;

  NodeAttachedProcessData(const NodeAttachedProcessData&) = delete;
  NodeAttachedProcessData& operator=(const NodeAttachedProcessData&) = delete;

  // Runs the given `func` for every ProcessNode in `graph` with type
  // PROCESS_TYPE_RENDERER, passing the NodeAttachedProcessData attached to the
  // node.
  static void ApplyToAllRenderers(
      Graph* graph,
      base::FunctionRef<void(NodeAttachedProcessData*)> func);

  const V8DetailedMemoryProcessData* data() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return data_available_ ? &data_ : nullptr;
  }

  void ScheduleNextMeasurement();

  V8DetailedMemoryRequestQueue& process_measurement_requests() {
    return process_measurement_requests_;
  }

  static V8DetailedMemoryProcessData* GetOrCreateForTesting(  // IN-TEST
      const ProcessNode* process_node);

 private:
  // Sends a measurement request to the renderer process.
  void StartMeasurement(MeasurementMode mode);

  // Schedules a call to UpgradeToBoundedMeasurementIfNeeded() at the point
  // when the next measurement with mode kBounded would start, to ensure that
  // kBounded requests can be scheduled while kLazy requests are running.
  void ScheduleUpgradeToBoundedMeasurement();

  // If a measurement with mode kLazy is in progress, calls StartMeasurement()
  // with mode `bounded_mode` to override it. Otherwise do nothing to let
  // ScheduleNextMeasurement() start the bounded measurement.
  void UpgradeToBoundedMeasurementIfNeeded(MeasurementMode bounded_mode);

  void EnsureRemote();
  void OnV8MemoryUsage(blink::mojom::PerProcessV8MemoryUsagePtr result);

  const raw_ptr<const ProcessNode> process_node_;

  // Measurement requests that will be sent to this process only.
  V8DetailedMemoryRequestQueue process_measurement_requests_
      GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::Remote<blink::mojom::V8DetailedMemoryReporter> resource_usage_reporter_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // State transitions:
  //
  //   +-----------------------------------+
  //   |                                   |
  //   |               +-> MeasuringLazy +-+
  //   v               |         +
  // Idle +-> Waiting +>         |
  //   ^               |         v
  //   |               +-> MeasuringBounded +-+
  //   |                                      |
  //   +--------------------------------------+
  enum class State {
    kIdle,              // No measurements scheduled.
    kWaiting,           // Waiting to take a measurement.
    kMeasuringBounded,  // Waiting for results from a bounded measurement.
    kMeasuringLazy,     // Waiting for results from a lazy measurement.
  };
  State state_ GUARDED_BY_CONTEXT(sequence_checker_) = State::kIdle;

  // Used to schedule the next measurement.
  base::TimeTicks last_request_time_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::OneShotTimer request_timer_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::OneShotTimer bounded_upgrade_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  V8DetailedMemoryProcessData data_ GUARDED_BY_CONTEXT(sequence_checker_);
  bool data_available_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<NodeAttachedProcessData> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

NodeAttachedProcessData::NodeAttachedProcessData(
    const ProcessNode* process_node)
    : process_node_(process_node) {
  ScheduleNextMeasurement();
}

// static
void NodeAttachedProcessData::ApplyToAllRenderers(
    Graph* graph,
    base::FunctionRef<void(NodeAttachedProcessData*)> func) {
  for (const ProcessNode* node : graph->GetAllProcessNodes()) {
    if (node->GetProcessType() != content::PROCESS_TYPE_RENDERER) {
      continue;
    }

    NodeAttachedProcessData* process_data = NodeAttachedProcessData::Get(node);
    DCHECK(process_data);
    func(process_data);
  }
}

void NodeAttachedProcessData::ScheduleNextMeasurement() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  process_measurement_requests_.Validate();

  if (state_ == State::kMeasuringLazy) {
    // Upgrade to a bounded measurement if the lazy measurement is taking too
    // long. Otherwise do nothing until the current measurement finishes.
    // ScheduleNextMeasurement will be called again at that point.
    ScheduleUpgradeToBoundedMeasurement();
    return;
  }

  if (state_ == State::kMeasuringBounded) {
    // Don't restart the timer until the current measurement finishes.
    // ScheduleNextMeasurement will be called again at that point.
    return;
  }

  // Find the next request for this process, checking both the per-process
  // queue and the global queue.
  const V8DetailedMemoryRequest* next_process_request =
      process_measurement_requests_.GetNextRequest();
  const V8DetailedMemoryRequest* next_global_request = nullptr;
  auto* decorator =
      V8DetailedMemoryDecorator::GetFromGraph(process_node_->GetGraph());
  if (decorator) {
    next_global_request = decorator->GetNextRequest();
  }
  const V8DetailedMemoryRequest* next_request =
      ChooseHigherPriorityRequest(next_process_request, next_global_request);

  if (!next_request) {
    // All measurements have been cancelled, or decorator was removed from
    // graph.
    state_ = State::kIdle;
    request_timer_.Stop();
    bounded_upgrade_timer_.Stop();
    last_request_time_ = base::TimeTicks();
    return;
  }

  state_ = State::kWaiting;
  if (last_request_time_.is_null()) {
    // This is the first measurement. Perform it immediately.
    StartMeasurement(next_request->mode());
    return;
  }

  base::TimeTicks next_request_time =
      last_request_time_ + next_request->min_time_between_requests();
  request_timer_.Start(
      FROM_HERE, next_request_time - base::TimeTicks::Now(),
      base::BindOnce(&NodeAttachedProcessData::StartMeasurement,
                     base::Unretained(this), next_request->mode()));
}

void NodeAttachedProcessData::StartMeasurement(MeasurementMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsMeasurementBounded(mode)) {
    DCHECK(state_ == State::kWaiting || state_ == State::kMeasuringLazy);
    state_ = State::kMeasuringBounded;
  } else {
    DCHECK_EQ(state_, State::kWaiting);
    state_ = State::kMeasuringLazy;
    // Ensure this lazy measurement doesn't starve any bounded measurements in
    // the queue.
    ScheduleUpgradeToBoundedMeasurement();
  }

  last_request_time_ = base::TimeTicks::Now();

  EnsureRemote();

  // TODO(b/1080672): WeakPtr is used in case NodeAttachedProcessData is
  // cleaned up while a request to a renderer is outstanding. Currently this
  // never actually happens (it is destroyed only when the graph is torn down,
  // which should happen after renderers are destroyed). Should clean up
  // NodeAttachedProcessData when the last V8DetailedMemoryRequest is deleted,
  // which could happen at any time.
  blink::mojom::V8DetailedMemoryReporter::Mode mojo_mode;
  switch (mode) {
    case MeasurementMode::kLazy:
      mojo_mode = blink::mojom::V8DetailedMemoryReporter::Mode::LAZY;
      break;
    case MeasurementMode::kBounded:
      mojo_mode = blink::mojom::V8DetailedMemoryReporter::Mode::DEFAULT;
      break;
    case MeasurementMode::kEagerForTesting:
      mojo_mode = blink::mojom::V8DetailedMemoryReporter::Mode::EAGER;
      break;
  }

  resource_usage_reporter_->GetV8MemoryUsage(
      mojo_mode, base::BindOnce(&NodeAttachedProcessData::OnV8MemoryUsage,
                                weak_factory_.GetWeakPtr()));
}

void NodeAttachedProcessData::ScheduleUpgradeToBoundedMeasurement() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kMeasuringLazy);

  const V8DetailedMemoryRequest* process_bounded_request =
      process_measurement_requests_.GetNextBoundedRequest();
  const V8DetailedMemoryRequest* global_bounded_request = nullptr;
  auto* decorator =
      V8DetailedMemoryDecorator::GetFromGraph(process_node_->GetGraph());
  if (decorator) {
    global_bounded_request = decorator->GetNextBoundedRequest();
  }
  const V8DetailedMemoryRequest* bounded_request = ChooseHigherPriorityRequest(
      process_bounded_request, global_bounded_request);
  if (!bounded_request) {
    // All measurements have been cancelled, or decorator was removed from
    // graph.
    return;
  }

  base::TimeTicks bounded_request_time =
      last_request_time_ + bounded_request->min_time_between_requests();
  bounded_upgrade_timer_.Start(
      FROM_HERE, bounded_request_time - base::TimeTicks::Now(),
      base::BindOnce(
          &NodeAttachedProcessData::UpgradeToBoundedMeasurementIfNeeded,
          base::Unretained(this), bounded_request->mode()));
}

void NodeAttachedProcessData::UpgradeToBoundedMeasurementIfNeeded(
    MeasurementMode bounded_mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kMeasuringLazy) {
    // State changed before timer expired.
    return;
  }
  DCHECK(IsMeasurementBounded(bounded_mode));
  StartMeasurement(bounded_mode);
}

void NodeAttachedProcessData::OnV8MemoryUsage(
    blink::mojom::PerProcessV8MemoryUsagePtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Data has arrived so don't upgrade lazy requests to bounded, even if
  // another lazy request is issued before the timer expires.
  bounded_upgrade_timer_.Stop();

  // Distribute the data to the frames.
  // If a frame doesn't have corresponding data in the result, clear any data
  // it may have had. Any datum in the result that doesn't correspond to an
  // existing frame is likewise accrued to detached bytes.
  uint64_t detached_v8_bytes_used = 0;
  uint64_t detached_canvas_bytes_used = 0;
  uint64_t shared_v8_bytes_used = 0;
  uint64_t blink_bytes_used = 0;

  // Create a mapping from token to execution context usage for the merge below.
  std::vector<std::pair<ExecutionContextToken, PerContextV8MemoryUsagePtr>>
      v8_memory;
  std::vector<std::pair<ExecutionContextToken, PerContextCanvasMemoryUsagePtr>>
      canvas_memory;
  for (auto& isolate : result->isolates) {
    for (auto& entry : isolate->contexts) {
      v8_memory.emplace_back(entry->token, std::move(entry));
    }
    for (auto& entry : isolate->canvas_contexts) {
      canvas_memory.emplace_back(entry->token, std::move(entry));
    }
    detached_v8_bytes_used += isolate->detached_bytes_used;
    shared_v8_bytes_used += isolate->shared_bytes_used;
    blink_bytes_used += isolate->blink_bytes_used;
  }

  size_t v8_frame_count = v8_memory.size();
  size_t canvas_frame_count = canvas_memory.size();

  base::flat_map<ExecutionContextToken, PerContextV8MemoryUsagePtr>
      associated_v8_memory(std::move(v8_memory));
  base::flat_map<ExecutionContextToken, PerContextCanvasMemoryUsagePtr>
      associated_canvas_memory(std::move(canvas_memory));
  // Validate that the frame tokens were all unique. If there are duplicates,
  // the map will arbitrarily drop all but one record per unique token.
  DCHECK_EQ(associated_v8_memory.size(), v8_frame_count);
  DCHECK_EQ(associated_canvas_memory.size(), canvas_frame_count);

  std::vector<const execution_context::ExecutionContext*> execution_contexts;
  for (auto* node : process_node_->GetFrameNodes()) {
    execution_contexts.push_back(
        execution_context::ExecutionContext::From(node));
  }
  for (auto* node : process_node_->GetWorkerNodes()) {
    execution_contexts.push_back(
        execution_context::ExecutionContext::From(node));
  }

  for (const execution_context::ExecutionContext* ec : execution_contexts) {
    auto it = associated_v8_memory.find(ec->GetToken());
    auto it_canvas = associated_canvas_memory.find(ec->GetToken());
    if (it == associated_v8_memory.end()) {
      // No data for this node, clear any data associated with it.
      // Note that we may have canvas memory for the context even if there
      // is no V8 memory (e.g. when the context was added after V8 memory
      // measurement but before canvas measurement). We drop such canvas
      // memory for simplicity because such case are rare and not important
      // for the users.
      ExecutionContextAttachedData::Destroy(ec);
    } else {
      ExecutionContextAttachedData* ec_data =
          ExecutionContextAttachedData::GetOrCreate(ec);
      DCHECK_CALLED_ON_VALID_SEQUENCE(ec_data->sequence_checker_);

      ec_data->data_available_ = true;
      ec_data->data_.set_v8_bytes_used(it->second->bytes_used);
      ec_data->data_.set_url(std::move(it->second->url));
      // Zero out this datum as its usage has been consumed.
      // We avoid erase() here because it may take O(n) time.
      it->second.reset();
      if (it_canvas != associated_canvas_memory.end()) {
        ec_data->data_.set_canvas_bytes_used(it_canvas->second->bytes_used);
        it_canvas->second.reset();
      }
    }
  }

  for (const auto& it : associated_v8_memory) {
    if (it.second.is_null()) {
      // Execution context was already consumed.
      continue;
    }
    // Accrue the data for non-existent frames to detached bytes.
    detached_v8_bytes_used += it.second->bytes_used;
  }

  for (const auto& it : associated_canvas_memory) {
    if (it.second.is_null()) {
      // Execution context was already consumed.
      continue;
    }
    // Accrue the data for non-existent frames to detached bytes.
    detached_canvas_bytes_used += it.second->bytes_used;
  }

  data_available_ = true;
  data_.set_detached_v8_bytes_used(detached_v8_bytes_used);
  data_.set_detached_canvas_bytes_used(detached_canvas_bytes_used);
  data_.set_shared_v8_bytes_used(shared_v8_bytes_used);
  data_.set_blink_bytes_used(blink_bytes_used);

  // Schedule another measurement for this process node unless one is already
  // scheduled.
  if (state_ != State::kWaiting) {
    state_ = State::kIdle;
    ScheduleNextMeasurement();
  }

  process_measurement_requests_.NotifyObserversOnMeasurementAvailable(
      process_node_);
  V8DetailedMemoryDecorator::ObserverNotifier()
      .NotifyObserversOnMeasurementAvailable(process_node_);
}

void NodeAttachedProcessData::EnsureRemote() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (resource_usage_reporter_.is_bound())
    return;

  // This interface is implemented in //content/renderer/performance_manager.
  mojo::PendingReceiver<blink::mojom::V8DetailedMemoryReporter>
      pending_receiver = resource_usage_reporter_.BindNewPipeAndPassReceiver();

  RenderProcessHostProxy proxy = process_node_->GetRenderProcessHostProxy();

  if (g_test_bind_callback) {
    g_test_bind_callback->Run(std::move(pending_receiver), std::move(proxy));
  } else {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&BindReceiverOnUIThread, std::move(pending_receiver),
                       std::move(proxy)));
  }
}

// static
V8DetailedMemoryProcessData* NodeAttachedProcessData::GetOrCreateForTesting(
    const ProcessNode* process_node) {
  auto* node_data = NodeAttachedProcessData::GetOrCreate(process_node);
  DCHECK_CALLED_ON_VALID_SEQUENCE(node_data->sequence_checker_);
  node_data->data_available_ = true;
  return &node_data->data_;
}

}  // namespace

namespace internal {

void SetBindV8DetailedMemoryReporterCallbackForTesting(  // IN-TEST
    BindV8DetailedMemoryReporterCallback* callback) {
  g_test_bind_callback = callback;
}

void DestroyV8DetailedMemoryDecoratorForTesting(Graph* graph) {
  auto* decorator = V8DetailedMemoryDecorator::GetFromGraph(graph);
  if (decorator)
    graph->TakeFromGraph(decorator);
}

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
// V8DetailedMemoryDecorator

V8DetailedMemoryDecorator::V8DetailedMemoryDecorator()
    : measurement_requests_(std::make_unique<V8DetailedMemoryRequestQueue>()) {}

V8DetailedMemoryDecorator::~V8DetailedMemoryDecorator() = default;

void V8DetailedMemoryDecorator::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Iterate over the existing process nodes to put them under observation.
  for (const ProcessNode* process_node : graph->GetAllProcessNodes()) {
    OnProcessNodeAdded(process_node);
  }

  graph->AddProcessNodeObserver(this);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(
      this, "V8DetailedMemoryDecorator");
}

void V8DetailedMemoryDecorator::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ApplyToAllRequestQueues(&V8DetailedMemoryRequestQueue::OnOwnerUnregistered);
  UpdateProcessMeasurementSchedules();

  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
  graph->RemoveProcessNodeObserver(this);
}

void V8DetailedMemoryDecorator::OnProcessNodeAdded(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(nullptr, NodeAttachedProcessData::Get(process_node));

  // Only renderer processes have frames. Don't attempt to connect to other
  // process types.
  if (process_node->GetProcessType() != content::PROCESS_TYPE_RENDERER)
    return;

  // Creating the NodeAttachedProcessData will start a measurement.
  NodeAttachedProcessData::GetOrCreate(process_node);
}

void V8DetailedMemoryDecorator::OnBeforeProcessNodeRemoved(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only renderer processes have data.
  if (process_node->GetProcessType() != content::PROCESS_TYPE_RENDERER)
    return;

  auto* process_data = NodeAttachedProcessData::Get(process_node);
  DCHECK(process_data);
  process_data->process_measurement_requests().OnOwnerUnregistered();
}

base::Value::Dict V8DetailedMemoryDecorator::DescribeFrameNodeData(
    const FrameNode* frame_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* const frame_data =
      V8DetailedMemoryExecutionContextData::ForFrameNode(frame_node);
  if (!frame_data)
    return base::Value::Dict();

  base::Value::Dict dict;
  dict.Set("v8_bytes_used", static_cast<int>(frame_data->v8_bytes_used()));
  return dict;
}

base::Value::Dict V8DetailedMemoryDecorator::DescribeProcessNodeData(
    const ProcessNode* process_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* const process_data =
      V8DetailedMemoryProcessData::ForProcessNode(process_node);
  if (!process_data)
    return base::Value::Dict();

  DCHECK_EQ(content::PROCESS_TYPE_RENDERER, process_node->GetProcessType());

  base::Value::Dict dict;
  dict.Set("detached_v8_bytes_used",
           static_cast<int>(process_data->detached_v8_bytes_used()));
  dict.Set("shared_v8_bytes_used",
           static_cast<int>(process_data->shared_v8_bytes_used()));
  return dict;
}

const V8DetailedMemoryRequest* V8DetailedMemoryDecorator::GetNextRequest()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return measurement_requests_->GetNextRequest();
}

const V8DetailedMemoryRequest*
V8DetailedMemoryDecorator::GetNextBoundedRequest() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return measurement_requests_->GetNextBoundedRequest();
}

void V8DetailedMemoryDecorator::AddMeasurementRequest(
    base::PassKey<V8DetailedMemoryRequest> key,
    V8DetailedMemoryRequest* request,
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (process_node) {
    auto* process_data = NodeAttachedProcessData::Get(process_node);
    DCHECK(process_data);
    process_data->process_measurement_requests().AddMeasurementRequest(request);
  } else {
    measurement_requests_->AddMeasurementRequest(request);
  }
  UpdateProcessMeasurementSchedules();
}

void V8DetailedMemoryDecorator::RemoveMeasurementRequest(
    base::PassKey<V8DetailedMemoryRequest> key,
    V8DetailedMemoryRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Attempt to remove this request from all process-specific queues and the
  // global queue. It will only be in one of them.
  size_t removal_count = 0;
  ApplyToAllRequestQueues(
      [request, &removal_count](V8DetailedMemoryRequestQueue* queue) {
        removal_count += queue->RemoveMeasurementRequest(request);
      });
  DCHECK_EQ(removal_count, 1ULL);
  UpdateProcessMeasurementSchedules();
}

void V8DetailedMemoryDecorator::ApplyToAllRequestQueues(
    base::FunctionRef<void(V8DetailedMemoryRequestQueue*)> func) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  func(measurement_requests_.get());
  NodeAttachedProcessData::ApplyToAllRenderers(
      GetOwningGraph(), [func](NodeAttachedProcessData* process_data) {
        func(&process_data->process_measurement_requests());
      });
}

void V8DetailedMemoryDecorator::UpdateProcessMeasurementSchedules() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  measurement_requests_->Validate();
  NodeAttachedProcessData::ApplyToAllRenderers(
      GetOwningGraph(), &NodeAttachedProcessData::ScheduleNextMeasurement);
}

void V8DetailedMemoryDecorator::NotifyObserversOnMeasurementAvailable(
    base::PassKey<ObserverNotifier> key,
    const ProcessNode* process_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  measurement_requests_->NotifyObserversOnMeasurementAvailable(process_node);
}

// static
const V8DetailedMemoryExecutionContextData*
V8DetailedMemoryDecorator::GetExecutionContextData(const FrameNode* node) {
  const auto* ec = execution_context::ExecutionContext::From(node);
  return GetExecutionContextData(ec);
}

// static
const V8DetailedMemoryExecutionContextData*
V8DetailedMemoryDecorator::GetExecutionContextData(const WorkerNode* node) {
  const auto* ec = execution_context::ExecutionContext::From(node);
  return GetExecutionContextData(ec);
}

// static
const V8DetailedMemoryExecutionContextData*
V8DetailedMemoryDecorator::GetExecutionContextData(
    const execution_context::ExecutionContext* ec) {
  const auto* node_data = ExecutionContextAttachedData::Get(ec);
  return node_data ? node_data->data() : nullptr;
}

// static
V8DetailedMemoryExecutionContextData*
V8DetailedMemoryDecorator::CreateExecutionContextDataForTesting(
    const FrameNode* node) {
  const auto* ec = execution_context::ExecutionContext::From(node);
  return ExecutionContextAttachedData::GetOrCreateForTesting(ec);
}

// static
V8DetailedMemoryExecutionContextData*
V8DetailedMemoryDecorator::CreateExecutionContextDataForTesting(
    const WorkerNode* node) {
  const auto* ec = execution_context::ExecutionContext::From(node);
  return ExecutionContextAttachedData::GetOrCreateForTesting(ec);
}

// static
const V8DetailedMemoryProcessData* V8DetailedMemoryDecorator::GetProcessData(
    const ProcessNode* node) {
  auto* node_data = NodeAttachedProcessData::Get(node);
  return node_data ? node_data->data() : nullptr;
}

// static
V8DetailedMemoryProcessData*
V8DetailedMemoryDecorator::CreateProcessDataForTesting(
    const ProcessNode* node) {
  return NodeAttachedProcessData::GetOrCreateForTesting(node);
}

////////////////////////////////////////////////////////////////////////////////
// V8DetailedMemoryRequestQueue

V8DetailedMemoryRequestQueue::~V8DetailedMemoryRequestQueue() {
  DCHECK(bounded_measurement_requests_.empty());
  DCHECK(lazy_measurement_requests_.empty());
}

const V8DetailedMemoryRequest* V8DetailedMemoryRequestQueue::GetNextRequest()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ChooseHigherPriorityRequest(GetNextBoundedRequest(),
                                     lazy_measurement_requests_.empty()
                                         ? nullptr
                                         : lazy_measurement_requests_.front());
}

const V8DetailedMemoryRequest*
V8DetailedMemoryRequestQueue::GetNextBoundedRequest() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return bounded_measurement_requests_.empty()
             ? nullptr
             : bounded_measurement_requests_.front();
}

void V8DetailedMemoryRequestQueue::AddMeasurementRequest(
    V8DetailedMemoryRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(request);
  std::vector<raw_ptr<V8DetailedMemoryRequest, VectorExperimental>>&
      measurement_requests =
          IsMeasurementBounded(request->mode()) ? bounded_measurement_requests_
                                                : lazy_measurement_requests_;
  DCHECK(!base::Contains(measurement_requests, request))
      << "V8DetailedMemoryRequest object added twice";
  // Each user of the decorator is expected to issue a single
  // V8DetailedMemoryRequest, so the size of measurement_requests is too low
  // to make the complexity of real priority queue worthwhile.
  for (std::vector<raw_ptr<V8DetailedMemoryRequest, VectorExperimental>>::
           const_iterator it = measurement_requests.begin();
       it != measurement_requests.end(); ++it) {
    if (request->min_time_between_requests() <
        (*it)->min_time_between_requests()) {
      measurement_requests.insert(it, request);
      return;
    }
  }
  measurement_requests.push_back(request);
}

size_t V8DetailedMemoryRequestQueue::RemoveMeasurementRequest(
    V8DetailedMemoryRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(request);
  return std::erase(IsMeasurementBounded(request->mode())
                        ? bounded_measurement_requests_
                        : lazy_measurement_requests_,
                    request);
}

void V8DetailedMemoryRequestQueue::NotifyObserversOnMeasurementAvailable(
    const ProcessNode* process_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Raw pointers are safe because the callback is synchronous.
  ApplyToAllRequests([process_node](V8DetailedMemoryRequest* request) {
    request->NotifyObserversOnMeasurementAvailable(
        base::PassKey<V8DetailedMemoryRequestQueue>(), process_node);
  });
}

void V8DetailedMemoryRequestQueue::OnOwnerUnregistered() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ApplyToAllRequests([](V8DetailedMemoryRequest* request) {
    request->OnOwnerUnregistered(base::PassKey<V8DetailedMemoryRequestQueue>());
  });
  bounded_measurement_requests_.clear();
  lazy_measurement_requests_.clear();
}

void V8DetailedMemoryRequestQueue::Validate() {
#if DCHECK_IS_ON()
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto check_invariants =
      [](const std::vector<raw_ptr<V8DetailedMemoryRequest,
                                   VectorExperimental>>& measurement_requests,
         bool is_bounded) {
        for (size_t i = 1; i < measurement_requests.size(); ++i) {
          DCHECK(measurement_requests[i - 1]);
          DCHECK(measurement_requests[i]);
          DCHECK_EQ(IsMeasurementBounded(measurement_requests[i - 1]->mode()),
                    is_bounded);
          DCHECK_EQ(IsMeasurementBounded(measurement_requests[i]->mode()),
                    is_bounded);
          DCHECK_LE(measurement_requests[i - 1]->min_time_between_requests(),
                    measurement_requests[i]->min_time_between_requests());
        }
      };
  check_invariants(bounded_measurement_requests_, true);
  check_invariants(lazy_measurement_requests_, false);
#endif
}

void V8DetailedMemoryRequestQueue::ApplyToAllRequests(
    base::FunctionRef<void(V8DetailedMemoryRequest*)> func) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // First collect all requests to notify. The function may add or remove
  // requests from the queue, invalidating iterators.
  std::vector<V8DetailedMemoryRequest*> requests_to_notify;
  requests_to_notify.insert(requests_to_notify.end(),
                            bounded_measurement_requests_.begin(),
                            bounded_measurement_requests_.end());
  requests_to_notify.insert(requests_to_notify.end(),
                            lazy_measurement_requests_.begin(),
                            lazy_measurement_requests_.end());
  for (V8DetailedMemoryRequest* request : requests_to_notify) {
    func(request);
    // The function may have deleted |request| so it is no longer safe to
    // reference.
  }
}

}  // namespace v8_memory

}  // namespace performance_manager
