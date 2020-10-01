// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/v8_memory/v8_per_frame_memory_decorator.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/timer/timer.h"
#include "base/util/type_safety/pass_key.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {

namespace v8_memory {

class V8PerFrameMemoryDecorator::MeasurementRequestQueue {
 public:
  MeasurementRequestQueue() = default;

  ~MeasurementRequestQueue();

  const V8PerFrameMemoryRequest* GetNextRequest() const;
  const V8PerFrameMemoryRequest* GetNextBoundedRequest() const;

  void AddMeasurementRequest(V8PerFrameMemoryRequest* request);

  // Removes |request| if it is part of this queue, and returns the number of
  // elements removed (will be 0 or 1).
  size_t RemoveMeasurementRequest(V8PerFrameMemoryRequest* request);

  void NotifyObserversOnMeasurementAvailable(
      const ProcessNode* process_node) const;

  void OnOwnerUnregistered();

  // Check the data invariant on the measurement request lists.
  void Validate();

 private:
  // Lists of requests sorted by min_time_between_requests (lowest first).
  std::vector<V8PerFrameMemoryRequest*> bounded_measurement_requests_;
  std::vector<V8PerFrameMemoryRequest*> lazy_measurement_requests_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// This class is allowed to access
// V8PerFrameMemoryDecorator::NotifyObserversOnMeasurementAvailable.
class V8PerFrameMemoryDecorator::ObserverNotifier {
 public:
  void NotifyObserversOnMeasurementAvailable(const ProcessNode* process_node) {
    auto* decorator =
        V8PerFrameMemoryDecorator::GetFromGraph(process_node->GetGraph());
    if (decorator)
      decorator->NotifyObserversOnMeasurementAvailable(
          util::PassKey<ObserverNotifier>(), process_node);
  }
};

namespace {

using MeasurementMode = V8PerFrameMemoryRequest::MeasurementMode;

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
const V8PerFrameMemoryRequest* ChooseHigherPriorityRequest(
    const V8PerFrameMemoryRequest* a,
    const V8PerFrameMemoryRequest* b) {
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

internal::BindV8DetailedMemoryReporterCallback* g_test_bind_callback = nullptr;

#if DCHECK_IS_ON()
bool g_test_eager_measurement_requests_enabled = false;
#endif

// Per-frame memory measurement involves the following classes that live on the
// PM sequence:
//
// V8PerFrameMemoryDecorator: Central rendezvous point. Coordinates
//     V8PerFrameMemoryRequest and V8PerFrameMemoryObserver objects. Owned by
//     the graph; created the first time
//     V8PerFrameMemoryRequest::StartMeasurement is called.
//     TODO(b/1080672): Currently this lives forever; should be cleaned up when
//     there are no more measurements scheduled.
//
// V8PerFrameMemoryRequest: Indicates that a caller wants memory to be measured
//     at a specific interval. Owned by the caller but must live on the PM
//     sequence. V8PerFrameMemoryRequest objects register themselves with
//     V8PerFrameMemoryDecorator on creation and unregister themselves on
//     deletion, which cancels the corresponding measurement.
//
// NodeAttachedProcessData: Private class that schedules measurements and holds
//     the results for an individual process. Owned by the ProcessNode; created
//     when measurements start.
//     TODO(b/1080672): Currently this lives forever; should be cleaned up when
//     there are no more measurements scheduled.
//
// V8PerFrameMemoryProcessData: Public accessor to the measurement results held
//     in a NodeAttachedProcessData, which owns it.
//
// NodeAttachedFrameData: Private class that holds the measurement results for
//     a frame. Owned by the FrameNode; created when a measurement result
//     arrives.
//     TODO(b/1080672): Currently this lives forever; should be cleaned up when
//     there are no more measurements scheduled.
//
// V8PerFrameMemoryFrameData: Public accessor to the measurement results held
//     in a NodeAttachedFrameData, which owns it.
//
// V8PerFrameMemoryObserver: Callers can implement this and register with
//     V8PerFrameMemoryDecorator::AddObserver() to be notified when
//     measurements are available for a process. Owned by the caller but must
//     live on the PM sequence.
//
// Additional wrapper classes can access these classes from other sequences:
//
// V8PerFrameMemoryRequestAnySeq: Wraps V8PerFrameMemoryRequest. Owned by the
//     caller and lives on any sequence.
//
// V8PerFrameMemoryObserverAnySeq: Callers can implement this and register it
//     with V8PerFrameMemoryRequestAnySeq::AddObserver() to be notified when
//     measurements are available for a process. Owned by the caller and lives
//     on the same sequence as the V8PerFrameMemoryRequestAnySeq.

////////////////////////////////////////////////////////////////////////////////
// NodeAttachedFrameData

class NodeAttachedFrameData
    : public ExternalNodeAttachedDataImpl<NodeAttachedFrameData> {
 public:
  explicit NodeAttachedFrameData(const FrameNode* frame_node) {}
  ~NodeAttachedFrameData() override = default;

  NodeAttachedFrameData(const NodeAttachedFrameData&) = delete;
  NodeAttachedFrameData& operator=(const NodeAttachedFrameData&) = delete;

  const V8PerFrameMemoryFrameData* data() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return data_available_ ? &data_ : nullptr;
  }

 private:
  friend class NodeAttachedProcessData;

  V8PerFrameMemoryFrameData data_;
  bool data_available_ = false;
  SEQUENCE_CHECKER(sequence_checker_);
};

////////////////////////////////////////////////////////////////////////////////
// NodeAttachedProcessData

class NodeAttachedProcessData
    : public ExternalNodeAttachedDataImpl<NodeAttachedProcessData> {
 public:
  explicit NodeAttachedProcessData(const ProcessNode* process_node);
  ~NodeAttachedProcessData() override = default;

  NodeAttachedProcessData(const NodeAttachedProcessData&) = delete;
  NodeAttachedProcessData& operator=(const NodeAttachedProcessData&) = delete;

  // Runs the given |callback| for every ProcessNode in |graph| with type
  // PROCESS_TYPE_RENDERER, passing the NodeAttachedProcessData attached to the
  // node.
  static void ApplyToAllRenderers(
      Graph* graph,
      base::RepeatingCallback<void(NodeAttachedProcessData*)> callback);

  const V8PerFrameMemoryProcessData* data() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return data_available_ ? &data_ : nullptr;
  }

  void ScheduleNextMeasurement();

  V8PerFrameMemoryDecorator::MeasurementRequestQueue&
  process_measurement_requests() {
    return process_measurement_requests_;
  }

 private:
  void StartMeasurement(MeasurementMode mode);
  void ScheduleUpgradeToBoundedMeasurement();
  void UpgradeToBoundedMeasurementIfNeeded(MeasurementMode bounded_mode);
  void EnsureRemote();
  void OnV8MemoryUsage(blink::mojom::PerProcessV8MemoryUsagePtr result);

  const ProcessNode* const process_node_;

  // Measurement requests that will be sent to this process only.
  V8PerFrameMemoryDecorator::MeasurementRequestQueue
      process_measurement_requests_;

  mojo::Remote<blink::mojom::V8DetailedMemoryReporter> resource_usage_reporter_;

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
  State state_ = State::kIdle;

  // Used to schedule the next measurement.
  base::TimeTicks last_request_time_;
  base::OneShotTimer request_timer_;
  base::OneShotTimer bounded_upgrade_timer_;

  V8PerFrameMemoryProcessData data_;
  bool data_available_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<NodeAttachedProcessData> weak_factory_{this};
};

NodeAttachedProcessData::NodeAttachedProcessData(
    const ProcessNode* process_node)
    : process_node_(process_node) {
  ScheduleNextMeasurement();
}

// static
void NodeAttachedProcessData::ApplyToAllRenderers(
    Graph* graph,
    base::RepeatingCallback<void(NodeAttachedProcessData*)> callback) {
  for (const ProcessNode* node : graph->GetAllProcessNodes()) {
    NodeAttachedProcessData* process_data = NodeAttachedProcessData::Get(node);
    if (!process_data) {
      // NodeAttachedProcessData should have been created for all renderer
      // processes in OnProcessNodeAdded.
      DCHECK_NE(content::PROCESS_TYPE_RENDERER, node->GetProcessType());
      continue;
    }
    callback.Run(process_data);
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
  const V8PerFrameMemoryRequest* next_request =
      process_measurement_requests_.GetNextRequest();
  auto* decorator =
      V8PerFrameMemoryDecorator::GetFromGraph(process_node_->GetGraph());
  if (decorator) {
    next_request =
        ChooseHigherPriorityRequest(next_request, decorator->GetNextRequest());
  }

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
  // NodeAttachedProcessData when the last V8PerFrameMemoryRequest is deleted,
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
#if DCHECK_IS_ON()
      DCHECK(g_test_eager_measurement_requests_enabled);
#endif
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

  const V8PerFrameMemoryRequest* bounded_request =
      process_measurement_requests_.GetNextBoundedRequest();
  auto* decorator =
      V8PerFrameMemoryDecorator::GetFromGraph(process_node_->GetGraph());
  if (decorator) {
    bounded_request = ChooseHigherPriorityRequest(
        bounded_request, decorator->GetNextBoundedRequest());
  }
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
  // existing frame is likewise accured to unassociated usage.
  uint64_t unassociated_v8_bytes_used = 0;

  // Create a mapping from token to per-frame usage for the merge below.
  std::vector<std::pair<blink::LocalFrameToken,
                        blink::mojom::PerContextV8MemoryUsagePtr>>
      tmp;
  for (auto& isolate : result->isolates) {
    for (auto& entry : isolate->contexts) {
      if (entry->token.Is<blink::LocalFrameToken>()) {
        tmp.emplace_back(entry->token.GetAs<blink::LocalFrameToken>(),
                         std::move(entry));
      }
      // TODO(ulan): Handle WorkerFrameTokens here.
    }
    unassociated_v8_bytes_used += isolate->unassociated_bytes_used;
  }

  size_t found_frame_count = tmp.size();

  base::flat_map<blink::LocalFrameToken,
                 blink::mojom::PerContextV8MemoryUsagePtr>
      associated_memory(std::move(tmp));
  // Validate that the frame tokens were all unique. If there are duplicates,
  // the map will arbitrarily drop all but one record per unique token.
  DCHECK_EQ(associated_memory.size(), found_frame_count);

  base::flat_set<const FrameNode*> frame_nodes = process_node_->GetFrameNodes();
  for (const FrameNode* frame_node : frame_nodes) {
    auto it = associated_memory.find(frame_node->GetFrameToken());
    if (it == associated_memory.end()) {
      // No data for this node, clear any data associated with it.
      NodeAttachedFrameData::Destroy(frame_node);
    } else {
      NodeAttachedFrameData* frame_data =
          NodeAttachedFrameData::GetOrCreate(frame_node);
      frame_data->data_available_ = true;
      frame_data->data_.set_v8_bytes_used(it->second->bytes_used);
      // Zero out this datum as its usage has been consumed.
      // We avoid erase() here because it may take O(n) time.
      it->second.reset();
    }
  }

  for (const auto& it : associated_memory) {
    if (it.second.is_null()) {
      // Frame was already consumed.
      continue;
    }
    // Accrue the data for non-existent frames to unassociated bytes.
    unassociated_v8_bytes_used += it.second->bytes_used;
  }

  data_available_ = true;
  data_.set_unassociated_v8_bytes_used(unassociated_v8_bytes_used);

  // Schedule another measurement for this process node unless one is already
  // scheduled.
  if (state_ != State::kWaiting) {
    state_ = State::kIdle;
    ScheduleNextMeasurement();
  }

  process_measurement_requests_.NotifyObserversOnMeasurementAvailable(
      process_node_);
  V8PerFrameMemoryDecorator::ObserverNotifier()
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

}  // namespace

namespace internal {

void SetBindV8DetailedMemoryReporterCallbackForTesting(
    BindV8DetailedMemoryReporterCallback* callback) {
  g_test_bind_callback = callback;
}

void SetEagerMemoryMeasurementEnabledForTesting(bool enabled) {
#if DCHECK_IS_ON()
  g_test_eager_measurement_requests_enabled = enabled;
#endif
}

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
// V8PerFrameMemoryRequest

V8PerFrameMemoryRequest::V8PerFrameMemoryRequest(
    const base::TimeDelta& min_time_between_requests,
    MeasurementMode mode)
    : min_time_between_requests_(min_time_between_requests), mode_(mode) {
#if DCHECK_IS_ON()
  DCHECK_GT(min_time_between_requests_, base::TimeDelta());
  DCHECK(!min_time_between_requests_.is_inf());
  DCHECK(mode != MeasurementMode::kEagerForTesting ||
         g_test_eager_measurement_requests_enabled);
#endif
}

V8PerFrameMemoryRequest::V8PerFrameMemoryRequest(
    const base::TimeDelta& min_time_between_requests,
    Graph* graph)
    : V8PerFrameMemoryRequest(min_time_between_requests,
                              MeasurementMode::kDefault) {
  StartMeasurement(graph);
}

V8PerFrameMemoryRequest::V8PerFrameMemoryRequest(
    const base::TimeDelta& min_time_between_requests,
    MeasurementMode mode,
    Graph* graph)
    : V8PerFrameMemoryRequest(min_time_between_requests, mode) {
  StartMeasurement(graph);
}

// This constructor is called from the V8PerFrameMemoryRequestAnySeq's
// sequence.
V8PerFrameMemoryRequest::V8PerFrameMemoryRequest(
    util::PassKey<V8PerFrameMemoryRequestAnySeq>,
    const base::TimeDelta& min_time_between_requests,
    MeasurementMode mode,
    base::WeakPtr<V8PerFrameMemoryRequestAnySeq> off_sequence_request)
    : V8PerFrameMemoryRequest(min_time_between_requests, mode) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  off_sequence_request_ = std::move(off_sequence_request);
  off_sequence_request_sequence_ = base::SequencedTaskRunnerHandle::Get();
  // Unretained is safe since |this| will be destroyed on the graph sequence.
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindOnce(&V8PerFrameMemoryRequest::StartMeasurement,
                                base::Unretained(this)));
}

V8PerFrameMemoryRequest::~V8PerFrameMemoryRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (decorator_)
    decorator_->RemoveMeasurementRequest(
        util::PassKey<V8PerFrameMemoryRequest>(), this);
  // TODO(crbug.com/1080672): Delete the decorator and its NodeAttachedData
  // when the last request is destroyed. Make sure this doesn't mess up any
  // measurement that's already in progress.
}

void V8PerFrameMemoryRequest::StartMeasurement(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StartMeasurementImpl(graph, nullptr);
}

void V8PerFrameMemoryRequest::StartMeasurementForProcess(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(process_node);
  DCHECK_EQ(process_node->GetProcessType(), content::PROCESS_TYPE_RENDERER);
  StartMeasurementImpl(process_node->GetGraph(), process_node);
}

void V8PerFrameMemoryRequest::AddObserver(V8PerFrameMemoryObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void V8PerFrameMemoryRequest::RemoveObserver(
    V8PerFrameMemoryObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

void V8PerFrameMemoryRequest::OnOwnerUnregistered(
    util::PassKey<V8PerFrameMemoryDecorator::MeasurementRequestQueue>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  decorator_ = nullptr;
}

void V8PerFrameMemoryRequest::NotifyObserversOnMeasurementAvailable(
    util::PassKey<V8PerFrameMemoryDecorator::MeasurementRequestQueue>,
    const ProcessNode* process_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* process_data =
      V8PerFrameMemoryProcessData::ForProcessNode(process_node);
  DCHECK(process_data);
  for (V8PerFrameMemoryObserver& observer : observers_)
    observer.OnV8MemoryMeasurementAvailable(process_node, process_data);

  // If this request was made from off-sequence, notify its off-sequence
  // observers with a copy of the process and frame data.
  if (off_sequence_request_.MaybeValid()) {
    using FrameAndData =
        std::pair<content::GlobalFrameRoutingId, V8PerFrameMemoryFrameData>;
    std::vector<FrameAndData> all_frame_data;
    process_node->VisitFrameNodes(base::BindRepeating(
        [](std::vector<FrameAndData>* all_frame_data,
           const FrameNode* frame_node) {
          const auto* frame_data =
              V8PerFrameMemoryFrameData::ForFrameNode(frame_node);
          if (frame_data) {
            all_frame_data->push_back(std::make_pair(
                frame_node->GetRenderFrameHostProxy().global_frame_routing_id(),
                *frame_data));
          }
          return true;
        },
        base::Unretained(&all_frame_data)));
    off_sequence_request_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(&V8PerFrameMemoryRequestAnySeq::
                           NotifyObserversOnMeasurementAvailable,
                       off_sequence_request_,
                       util::PassKey<V8PerFrameMemoryRequest>(),
                       process_node->GetRenderProcessHostId(), *process_data,
                       V8PerFrameMemoryObserverAnySeq::FrameDataMap(
                           std::move(all_frame_data))));
  }
}

void V8PerFrameMemoryRequest::StartMeasurementImpl(
    Graph* graph,
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(nullptr, decorator_);
  DCHECK(!process_node || graph == process_node->GetGraph());
  decorator_ = V8PerFrameMemoryDecorator::GetFromGraph(graph);
  if (!decorator_) {
    // Create the decorator when the first measurement starts.
    auto decorator_ptr = std::make_unique<V8PerFrameMemoryDecorator>();
    decorator_ = decorator_ptr.get();
    graph->PassToGraph(std::move(decorator_ptr));
  }

  decorator_->AddMeasurementRequest(util::PassKey<V8PerFrameMemoryRequest>(),
                                    this, process_node);
}

////////////////////////////////////////////////////////////////////////////////
// V8PerFrameMemoryFrameData

const V8PerFrameMemoryFrameData* V8PerFrameMemoryFrameData::ForFrameNode(
    const FrameNode* node) {
  auto* node_data = NodeAttachedFrameData::Get(node);
  return node_data ? node_data->data() : nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// V8PerFrameMemoryProcessData

const V8PerFrameMemoryProcessData* V8PerFrameMemoryProcessData::ForProcessNode(
    const ProcessNode* node) {
  auto* node_data = NodeAttachedProcessData::Get(node);
  return node_data ? node_data->data() : nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// V8PerFrameMemoryDecorator

V8PerFrameMemoryDecorator::V8PerFrameMemoryDecorator()
    : measurement_requests_(std::make_unique<MeasurementRequestQueue>()) {}

V8PerFrameMemoryDecorator::~V8PerFrameMemoryDecorator() = default;

void V8PerFrameMemoryDecorator::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(nullptr, graph_);
  graph_ = graph;

  graph->RegisterObject(this);

  // Iterate over the existing process nodes to put them under observation.
  for (const ProcessNode* process_node : graph->GetAllProcessNodes())
    OnProcessNodeAdded(process_node);

  graph->AddProcessNodeObserver(this);
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(
      this, "V8PerFrameMemoryDecorator");
}

void V8PerFrameMemoryDecorator::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(graph, graph_);

  ApplyToAllRequestQueues(
      base::BindRepeating(&MeasurementRequestQueue::OnOwnerUnregistered));
  UpdateProcessMeasurementSchedules();

  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
  graph->RemoveProcessNodeObserver(this);
  graph->UnregisterObject(this);
  graph_ = nullptr;
}

void V8PerFrameMemoryDecorator::OnProcessNodeAdded(
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

void V8PerFrameMemoryDecorator::OnBeforeProcessNodeRemoved(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only renderer processes have data.
  if (process_node->GetProcessType() != content::PROCESS_TYPE_RENDERER)
    return;

  auto* process_data = NodeAttachedProcessData::Get(process_node);
  DCHECK(process_data);
  process_data->process_measurement_requests().OnOwnerUnregistered();
}

base::Value V8PerFrameMemoryDecorator::DescribeFrameNodeData(
    const FrameNode* frame_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* const frame_data =
      V8PerFrameMemoryFrameData::ForFrameNode(frame_node);
  if (!frame_data)
    return base::Value();

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("v8_bytes_used", frame_data->v8_bytes_used());
  return dict;
}

base::Value V8PerFrameMemoryDecorator::DescribeProcessNodeData(
    const ProcessNode* process_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* const process_data =
      V8PerFrameMemoryProcessData::ForProcessNode(process_node);
  if (!process_data)
    return base::Value();

  DCHECK_EQ(content::PROCESS_TYPE_RENDERER, process_node->GetProcessType());

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("unassociated_v8_bytes_used",
                 process_data->unassociated_v8_bytes_used());
  return dict;
}

const V8PerFrameMemoryRequest* V8PerFrameMemoryDecorator::GetNextRequest()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return measurement_requests_->GetNextRequest();
}

const V8PerFrameMemoryRequest*
V8PerFrameMemoryDecorator::GetNextBoundedRequest() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return measurement_requests_->GetNextBoundedRequest();
}

void V8PerFrameMemoryDecorator::AddMeasurementRequest(
    util::PassKey<V8PerFrameMemoryRequest> key,
    V8PerFrameMemoryRequest* request,
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

void V8PerFrameMemoryDecorator::RemoveMeasurementRequest(
    util::PassKey<V8PerFrameMemoryRequest> key,
    V8PerFrameMemoryRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Attempt to remove this request from all process-specific queues and the
  // global queue. It will only be in one of them.
  size_t removal_count = 0;
  // Unretained is safe because this callback is synchronous.
  ApplyToAllRequestQueues(base::BindRepeating(
      [](V8PerFrameMemoryRequest* request, size_t* removal_count,
         MeasurementRequestQueue* queue) {
        (*removal_count) += queue->RemoveMeasurementRequest(request);
      },
      base::Unretained(request), base::Unretained(&removal_count)));
  DCHECK_EQ(removal_count, 1ULL);
  UpdateProcessMeasurementSchedules();
}

void V8PerFrameMemoryDecorator::ApplyToAllRequestQueues(
    RequestQueueCallback callback) const {
  callback.Run(measurement_requests_.get());
  NodeAttachedProcessData::ApplyToAllRenderers(
      graph_, base::BindRepeating(
                  [](RequestQueueCallback callback,
                     NodeAttachedProcessData* process_data) {
                    callback.Run(&process_data->process_measurement_requests());
                  },
                  std::move(callback)));
}

void V8PerFrameMemoryDecorator::UpdateProcessMeasurementSchedules() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(graph_);
  measurement_requests_->Validate();
  NodeAttachedProcessData::ApplyToAllRenderers(
      graph_,
      base::BindRepeating(&NodeAttachedProcessData::ScheduleNextMeasurement));
}

void V8PerFrameMemoryDecorator::NotifyObserversOnMeasurementAvailable(
    util::PassKey<ObserverNotifier> key,
    const ProcessNode* process_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  measurement_requests_->NotifyObserversOnMeasurementAvailable(process_node);
}

V8PerFrameMemoryDecorator::MeasurementRequestQueue::~MeasurementRequestQueue() {
  DCHECK(bounded_measurement_requests_.empty());
  DCHECK(lazy_measurement_requests_.empty());
}

const V8PerFrameMemoryRequest*
V8PerFrameMemoryDecorator::MeasurementRequestQueue::GetNextRequest() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ChooseHigherPriorityRequest(GetNextBoundedRequest(),
                                     lazy_measurement_requests_.empty()
                                         ? nullptr
                                         : lazy_measurement_requests_.front());
}

const V8PerFrameMemoryRequest*
V8PerFrameMemoryDecorator::MeasurementRequestQueue::GetNextBoundedRequest()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return bounded_measurement_requests_.empty()
             ? nullptr
             : bounded_measurement_requests_.front();
}

void V8PerFrameMemoryDecorator::MeasurementRequestQueue::AddMeasurementRequest(
    V8PerFrameMemoryRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(request);
  std::vector<V8PerFrameMemoryRequest*>& measurement_requests =
      IsMeasurementBounded(request->mode()) ? bounded_measurement_requests_
                                            : lazy_measurement_requests_;
  DCHECK(!base::Contains(measurement_requests, request))
      << "V8PerFrameMemoryRequest object added twice";
  // Each user of the decorator is expected to issue a single
  // V8PerFrameMemoryRequest, so the size of measurement_requests is too low
  // to make the complexity of real priority queue worthwhile.
  for (std::vector<V8PerFrameMemoryRequest*>::const_iterator it =
           measurement_requests.begin();
       it != measurement_requests.end(); ++it) {
    if (request->min_time_between_requests() <
        (*it)->min_time_between_requests()) {
      measurement_requests.insert(it, request);
      return;
    }
  }
  measurement_requests.push_back(request);
}

size_t
V8PerFrameMemoryDecorator::MeasurementRequestQueue::RemoveMeasurementRequest(
    V8PerFrameMemoryRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(request);
  return base::Erase(IsMeasurementBounded(request->mode())
                         ? bounded_measurement_requests_
                         : lazy_measurement_requests_,
                     request);
}

void V8PerFrameMemoryDecorator::MeasurementRequestQueue::
    NotifyObserversOnMeasurementAvailable(
        const ProcessNode* process_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const V8PerFrameMemoryRequest* request : bounded_measurement_requests_) {
    request->NotifyObserversOnMeasurementAvailable(
        util::PassKey<MeasurementRequestQueue>(), process_node);
  }
  for (const V8PerFrameMemoryRequest* request : lazy_measurement_requests_) {
    request->NotifyObserversOnMeasurementAvailable(
        util::PassKey<MeasurementRequestQueue>(), process_node);
  }
}

void V8PerFrameMemoryDecorator::MeasurementRequestQueue::OnOwnerUnregistered() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (V8PerFrameMemoryRequest* request : bounded_measurement_requests_) {
    request->OnOwnerUnregistered(util::PassKey<MeasurementRequestQueue>());
  }
  bounded_measurement_requests_.clear();
  for (V8PerFrameMemoryRequest* request : lazy_measurement_requests_) {
    request->OnOwnerUnregistered(util::PassKey<MeasurementRequestQueue>());
  }
  lazy_measurement_requests_.clear();
}

void V8PerFrameMemoryDecorator::MeasurementRequestQueue::Validate() {
#if DCHECK_IS_ON()
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto check_invariants =
      [](const std::vector<V8PerFrameMemoryRequest*>& measurement_requests,
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

////////////////////////////////////////////////////////////////////////////////
// V8PerFrameMemoryRequestAnySeq

V8PerFrameMemoryRequestAnySeq::V8PerFrameMemoryRequestAnySeq(
    const base::TimeDelta& min_time_between_requests,
    MeasurementMode mode) {
  // |request_| must be initialized in the constructor body so that
  // |weak_factory_| is completely constructed.
  //
  // Can't use make_unique since this calls the private any-sequence
  // constructor. After construction the V8PerFrameMemoryRequest must only be
  // accessed on the graph sequence.
  request_ = base::WrapUnique(new V8PerFrameMemoryRequest(
      util::PassKey<V8PerFrameMemoryRequestAnySeq>(), min_time_between_requests,
      mode, weak_factory_.GetWeakPtr()));
}

V8PerFrameMemoryRequestAnySeq::~V8PerFrameMemoryRequestAnySeq() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindOnce(
                     [](std::unique_ptr<V8PerFrameMemoryRequest> request) {
                       request.reset();
                     },
                     std::move(request_)));
}

bool V8PerFrameMemoryRequestAnySeq::HasObserver(
    V8PerFrameMemoryObserverAnySeq* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return observers_.HasObserver(observer);
}

void V8PerFrameMemoryRequestAnySeq::AddObserver(
    V8PerFrameMemoryObserverAnySeq* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void V8PerFrameMemoryRequestAnySeq::RemoveObserver(
    V8PerFrameMemoryObserverAnySeq* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

void V8PerFrameMemoryRequestAnySeq::NotifyObserversOnMeasurementAvailable(
    util::PassKey<V8PerFrameMemoryRequest>,
    RenderProcessHostId render_process_host_id,
    const V8PerFrameMemoryProcessData& process_data,
    const V8PerFrameMemoryObserverAnySeq::FrameDataMap& frame_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (V8PerFrameMemoryObserverAnySeq& observer : observers_)
    observer.OnV8MemoryMeasurementAvailable(render_process_host_id,
                                            process_data, frame_data);
}

}  // namespace v8_memory

}  // namespace performance_manager
