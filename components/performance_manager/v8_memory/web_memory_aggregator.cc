// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/web_memory_aggregator.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/stack.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/v8_memory/v8_detailed_memory.h"
#include "components/performance_manager/v8_memory/v8_context_tracker.h"
#include "url/gurl.h"

namespace performance_manager {

namespace v8_memory {

// A visitor that visits every node that can be aggregated into an aggregation
// point.
//
// TODO(joenotcharles): If we ever need to aggregate different data for each
// aggregation point, turn this into an interface and add a subclass for each
// type of data to aggregate.
class AggregationPointVisitor {
 public:
  // The given |main_origin| is the origin of the main web page, which is the
  // same as the origin of the top-level frames.
  AggregationPointVisitor(const url::Origin& requesting_origin,
                          const ProcessNode* requesting_process_node,
                          const url::Origin& main_origin);

  ~AggregationPointVisitor();

  AggregationPointVisitor(const AggregationPointVisitor& other) = delete;
  AggregationPointVisitor& operator=(const AggregationPointVisitor& other) =
      delete;

  mojom::WebMemoryMeasurementPtr TakeAggregationResult();

  // Called on first visiting |frame_node| in a depth-first traversal.
  void OnFrameEntered(const FrameNode* frame_node);

  // Called after visiting |frame_node| and all its children in a depth-first
  // traversal.
  void OnFrameExited(const FrameNode* frame_node);

  // Called on first visiting |worker_node| in a depth-first traversal.
  void OnWorkerEntered(const WorkerNode* worker_node);

  // Called after visiting |worker_node| and all its children in a depth-first
  // traversal.
  void OnWorkerExited(const WorkerNode* worker_node);

  // Called at the start of the depth-first traversal to set up the common
  // root node for all frame trees.
  void OnRootEntered();

  // Called at the end of the traversal.
  void OnRootExited();

 private:
  struct Enclosing {
    url::Origin origin;
    raw_ptr<mojom::WebMemoryBreakdownEntry> aggregation_point;
  };
  const url::Origin requesting_origin_;
  raw_ptr<const ProcessNode> requesting_process_node_;
  const url::Origin main_origin_;
  mojom::WebMemoryMeasurementPtr aggregation_result_ =
      mojom::WebMemoryMeasurement::New();
  mojom::WebMemoryBreakdownEntryPtr root_aggregation_point_;
  base::stack<Enclosing> enclosing_;
};

namespace {

using AttributionScope = mojom::WebMemoryAttribution::Scope;

// The various ways a node can be treated during the aggregation.
enum class NodeAggregationType {
  // Node is same-origin to |requesting_node| and its iframe attributes are
  // visible;
  // will be a new aggregation point with a scope depending on the node type
  // (eg. "Window" or "DedicatedWorker").
  kSameOriginAggregationPoint,
  // Node is same-origin to |requesting_node| but its iframe attributes are not
  // visible;
  // will be a new aggregation point with a scope depending on the node type
  // (eg. "Window" or "DedicatedWorker").
  kSameOriginAggregationPointWithHiddenAttributes,
  // Node is cross-origin with |requesting_node| but its parent is not; will
  // be a new aggregation point with scope "cross-origin-aggregated".
  kCrossOriginAggregationPoint,
  // Node is cross-origin with |requesting_node| and so is its parent; will
  // be aggregated into its parent's aggregation point.
  kCrossOriginAggregated,
};

NodeAggregationType GetNodeAggregationType(const url::Origin& requesting_origin,
                                           const url::Origin& enclosing_origin,
                                           const url::Origin& node_origin) {
  bool same_origin_node = requesting_origin.IsSameOriginWith(node_origin);
  bool same_origin_parent =
      requesting_origin.IsSameOriginWith(enclosing_origin);

  if (same_origin_node) {
    return same_origin_parent
               ? NodeAggregationType::kSameOriginAggregationPoint
               : NodeAggregationType::
                     kSameOriginAggregationPointWithHiddenAttributes;
  } else {
    return same_origin_parent
               ? NodeAggregationType::kCrossOriginAggregationPoint
               : NodeAggregationType::kCrossOriginAggregated;
  }
}

// Returns a mutable pointer to the WebMemoryAttribution structure in the given
// |breakdown|.
mojom::WebMemoryAttribution* GetAttributionFromBreakdown(
    mojom::WebMemoryBreakdownEntry* breakdown) {
  // We only store a single attribution with each breakdown.
  DCHECK_EQ(breakdown->attribution.size(), 1U);
  mojom::WebMemoryAttribution* attribution =
      breakdown->attribution.front().get();
  DCHECK(attribution);
  return attribution;
}

// Returns a const pointer to the WebMemoryAttribution structure in the given
// |breakdown|.
const mojom::WebMemoryAttribution* GetAttributionFromBreakdown(
    const mojom::WebMemoryBreakdownEntry* breakdown) {
  auto* mutable_breakdown =
      const_cast<mojom::WebMemoryBreakdownEntry*>(breakdown);
  auto* mutable_attribution = GetAttributionFromBreakdown(mutable_breakdown);
  return const_cast<mojom::WebMemoryAttribution*>(mutable_attribution);
}

AttributionScope AttributionScopeFromWorkerType(
    WorkerNode::WorkerType worker_type) {
  switch (worker_type) {
    case WorkerNode::WorkerType::kDedicated:
      return AttributionScope::kDedicatedWorker;
    case WorkerNode::WorkerType::kShared:
    case WorkerNode::WorkerType::kService:
      // TODO(crbug.com/40165276): Support service and shared workers.
      NOTREACHED_IN_MIGRATION();
      return AttributionScope::kDedicatedWorker;
  }
}

void AddMemoryBytes(mojom::WebMemoryBreakdownEntry* aggregation_point,
                    const V8DetailedMemoryExecutionContextData* data,
                    bool is_same_process) {
  if (!data) {
    return;
  }
  if (!aggregation_point->memory) {
    aggregation_point->memory = mojom::WebMemoryUsage::New();
  }
  // Ensure this frame is actually in the same process as the requesting
  // frame. If not it should be considered to have 0 bytes.
  // (https://github.com/WICG/performance-measure-memory/issues/20).
  uint64_t bytes_used = is_same_process ? data->v8_bytes_used() : 0;
  aggregation_point->memory->bytes += bytes_used;

  // Add canvas memory similar to V8 memory above.
  if (data->canvas_bytes_used()) {
    uint64_t canvas_bytes_used =
        is_same_process ? *data->canvas_bytes_used() : 0;
    if (!aggregation_point->canvas_memory) {
      aggregation_point->canvas_memory = mojom::WebMemoryUsage::New();
    }
    aggregation_point->canvas_memory->bytes += canvas_bytes_used;
  }
}

const FrameNode* GetTopFrame(const FrameNode* frame) {
  DCHECK(frame);
  // Follow the parent to find the top-most frame.
  auto* current = frame;
  while (auto* parent = current->GetParentFrameNode()) {
    current = parent;
  }

  DCHECK(current);
  // Make sure we didn't break out of the browsing context group.
  DCHECK_EQ(current->GetBrowsingInstanceId(), frame->GetBrowsingInstanceId());
  return current;
}

// Returns the process node of the main frame that is in the same browsing
// context group as the given frame.
const ProcessNode* GetMainProcess(const FrameNode* frame) {
  // COOP guarantees that the top-most frame of the current frame tree
  // and the main frame of the page have the same origin and thus have
  // the same process node.
  return GetTopFrame(frame)->GetProcessNode();
}

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// AggregationPointVisitor

AggregationPointVisitor::AggregationPointVisitor(
    const url::Origin& requesting_origin,
    const ProcessNode* requesting_process_node,
    const url::Origin& main_origin)
    : requesting_origin_(requesting_origin),
      requesting_process_node_(requesting_process_node),
      main_origin_(main_origin) {}

AggregationPointVisitor::~AggregationPointVisitor() {
  DCHECK(enclosing_.empty());
}

mojom::WebMemoryMeasurementPtr
AggregationPointVisitor::TakeAggregationResult() {
  DCHECK(aggregation_result_);
  auto result = std::move(aggregation_result_);
  aggregation_result_ = nullptr;
  return result;
}

void AggregationPointVisitor::OnRootEntered() {
  DCHECK(enclosing_.empty());
  root_aggregation_point_ = mojom::WebMemoryBreakdownEntry::New();
  root_aggregation_point_->attribution.emplace_back(
      mojom::WebMemoryAttribution::New());
  enclosing_.push(Enclosing{main_origin_, root_aggregation_point_.get()});
}

void AggregationPointVisitor::OnRootExited() {
  if (root_aggregation_point_->memory) {
    aggregation_result_->breakdown.push_back(
        std::move(root_aggregation_point_));
  }
  enclosing_.pop();
  DCHECK(enclosing_.empty());
}

void AggregationPointVisitor::OnFrameEntered(const FrameNode* frame_node) {
  DCHECK(!enclosing_.empty());
  DCHECK(frame_node);
  // If the frame's origin isn't known yet, use a unique opaque origin.
  const url::Origin node_origin =
      frame_node->GetOrigin().value_or(url::Origin());
  NodeAggregationType aggregation_type = GetNodeAggregationType(
      requesting_origin_, enclosing_.top().origin, node_origin);
  mojom::WebMemoryBreakdownEntry* aggregation_point = nullptr;
  switch (aggregation_type) {
    case NodeAggregationType::kSameOriginAggregationPoint:
      aggregation_point = WebMemoryAggregator::CreateBreakdownEntry(
          AttributionScope::kWindow, frame_node->GetURL().spec(),
          aggregation_result_.get());
      WebMemoryAggregator::SetBreakdownAttributionFromFrame(frame_node,
                                                            aggregation_point);
      break;

    case NodeAggregationType::kSameOriginAggregationPointWithHiddenAttributes:
      aggregation_point = WebMemoryAggregator::CreateBreakdownEntry(
          AttributionScope::kWindow, frame_node->GetURL().spec(),
          aggregation_result_.get());
      // Some grandparent node is the most recent aggregation point whose
      // attributes are visible to the start node, and
      // |enclosing_aggregation_point| includes those attributes. Copy the
      // id and src attributes from there.
      WebMemoryAggregator::CopyBreakdownAttribution(
          enclosing_.top().aggregation_point, aggregation_point);
      break;
    case NodeAggregationType::kCrossOriginAggregationPoint:
      // Create a new aggregation point with cross-origin-aggregated scope.
      // Since this node is NOT same-origin to the start node, the start node
      // CANNOT view its current url.
      aggregation_point = WebMemoryAggregator::CreateBreakdownEntry(
          AttributionScope::kCrossOriginAggregated, std::nullopt,
          aggregation_result_.get());
      // This is cross-origin but not being aggregated into another
      // aggregation point, so its parent or opener must be same-origin to the
      // start node, which can therefore view its attributes. Add the id and
      // src recorded for the node in V8ContextTracker to the new breakdown
      // entry.
      WebMemoryAggregator::SetBreakdownAttributionFromFrame(frame_node,
                                                            aggregation_point);
      break;

    case NodeAggregationType::kCrossOriginAggregated:
      // Update the enclosing aggregation point in-place.
      aggregation_point = enclosing_.top().aggregation_point;
      break;
  }

  // Now update the memory used in the chosen aggregation point.
  DCHECK(aggregation_point);
  AddMemoryBytes(aggregation_point,
                 V8DetailedMemoryExecutionContextData::ForFrameNode(frame_node),
                 frame_node->GetProcessNode() == requesting_process_node_);

  enclosing_.push(Enclosing{node_origin, aggregation_point});
}

void AggregationPointVisitor::OnFrameExited(const FrameNode* frame_node) {
  enclosing_.pop();
  DCHECK(!enclosing_.empty());
}

void AggregationPointVisitor::OnWorkerEntered(const WorkerNode* worker_node) {
  DCHECK(!enclosing_.empty());
  DCHECK(worker_node);
  // TODO(crbug.com/40165276): Support service and shared workers.
  DCHECK_EQ(worker_node->GetWorkerType(), WorkerNode::WorkerType::kDedicated);
  // A dedicated worker is guaranteed to have the same origin as its parent,
  // which means that a dedicated worker cannot be a cross-origin aggregation
  // point.
#if DCHECK_IS_ON()
  auto client_frames = worker_node->GetClientFrames();
  DCHECK(base::ranges::all_of(
      client_frames, [worker_node](const FrameNode* client) {
        return client->GetOrigin().has_value() &&
               client->GetOrigin()->IsSameOriginWith(worker_node->GetOrigin());
      }));
  auto client_workers = worker_node->GetClientWorkers();
  DCHECK(base::ranges::all_of(
      client_workers, [worker_node](const WorkerNode* client) {
        return client->GetOrigin().IsSameOriginWith(worker_node->GetOrigin());
      }));
#endif
  NodeAggregationType aggregation_type = GetNodeAggregationType(
      requesting_origin_, enclosing_.top().origin, worker_node->GetOrigin());

  mojom::WebMemoryBreakdownEntry* aggregation_point = nullptr;
  switch (aggregation_type) {
    case NodeAggregationType::kSameOriginAggregationPoint:
    case NodeAggregationType::kSameOriginAggregationPointWithHiddenAttributes: {
      // Create a new aggregation point with window scope. Since this node is
      // same-origin to the start node, the start node can view its current
      // url.
      std::string url = worker_node->GetURL().spec();
      if (url.empty()) {
        // TODO(crbug.com/40093136): Remove this once PlzDedicatedWorker ships.
        // Until then the browser does not know URLs of dedicated workers, so we
        // pass them together with the measurement result.
        const auto* data =
            V8DetailedMemoryExecutionContextData::ForWorkerNode(worker_node);
        if (data && data->url()) {
          url = *data->url();
        }
      }
      aggregation_point = WebMemoryAggregator::CreateBreakdownEntry(
          AttributionScopeFromWorkerType(worker_node->GetWorkerType()),
          std::move(url), aggregation_result_.get());
      WebMemoryAggregator::CopyBreakdownAttribution(
          enclosing_.top().aggregation_point, aggregation_point);
      break;
    }

    case NodeAggregationType::kCrossOriginAggregated:
      // Update the enclosing aggregation point in-place.
      aggregation_point = enclosing_.top().aggregation_point;
      break;

    case NodeAggregationType::kCrossOriginAggregationPoint:
      NOTREACHED_IN_MIGRATION();
      return;
  }

  // Now update the memory used in the chosen aggregation point.
  DCHECK(aggregation_point);
  AddMemoryBytes(
      aggregation_point,
      V8DetailedMemoryExecutionContextData::ForWorkerNode(worker_node),
      worker_node->GetProcessNode() == requesting_process_node_);

  enclosing_.push(Enclosing{worker_node->GetOrigin(), aggregation_point});
}

void AggregationPointVisitor::OnWorkerExited(const WorkerNode* worker_node) {
  enclosing_.pop();
  DCHECK(!enclosing_.empty());
}

////////////////////////////////////////////////////////////////////////////////
// WebMemoryAggregator

WebMemoryAggregator::WebMemoryAggregator(const FrameNode* requesting_node)
    : requesting_origin_(requesting_node->GetOrigin().value()),
      requesting_process_node_(requesting_node->GetProcessNode()),
      main_process_node_(GetMainProcess(requesting_node)),
      browsing_instance_id_(requesting_node->GetBrowsingInstanceId()) {}

WebMemoryAggregator::~WebMemoryAggregator() = default;

namespace {

// Returns v8_browsing_memory / v8_process_memory where
// - v8_browsing_memory is the total V8 memory usage of all frames of the given
//   |browsing_instance_id| in the given |process_node|.
// - v8_process_memory is the total V8 memory usage of all frames in the given
//   |process_node|.
double GetBrowsingInstanceV8BytesFraction(
    const ProcessNode* process_node,
    content::BrowsingInstanceId browsing_instance_id) {
  uint64_t bytes_used = 0;
  uint64_t total_bytes_used = 0;
  for (const FrameNode* frame_node : process_node->GetFrameNodes()) {
    const auto* data =
        V8DetailedMemoryExecutionContextData::ForFrameNode(frame_node);
    if (data) {
      if (frame_node->GetBrowsingInstanceId() == browsing_instance_id) {
        bytes_used += data->v8_bytes_used();
      }
      total_bytes_used += data->v8_bytes_used();
    }
  }
  DCHECK_LE(bytes_used, total_bytes_used);
  return total_bytes_used == 0
             ? 1
             : static_cast<double>(bytes_used) / total_bytes_used;
}

}  // anonymous namespace

mojom::WebMemoryMeasurementPtr
WebMemoryAggregator::AggregateMeasureMemoryResult() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<const FrameNode*> top_frames;
  for (const FrameNode* node : main_process_node_->GetFrameNodes()) {
    if (node->GetBrowsingInstanceId() == browsing_instance_id_ &&
        !node->GetParentFrameNode() && node->GetOrigin() &&
        !node->GetOrigin()->opaque()) {
      top_frames.push_back(node);
    }
  }

  CHECK(!top_frames.empty());
  const url::Origin main_origin = top_frames.front()->GetOrigin().value();
  DCHECK(
      base::ranges::all_of(top_frames, [&main_origin](const FrameNode* node) {
        return node->GetOrigin()->IsSameOriginWith(main_origin);
      }));

  AggregationPointVisitor ap_visitor(requesting_origin_,
                                     requesting_process_node_, main_origin);
  ap_visitor.OnRootEntered();
  for (const FrameNode* node : top_frames) {
    VisitFrame(&ap_visitor, node);
  }
  ap_visitor.OnRootExited();
  mojom::WebMemoryMeasurementPtr aggregation_result =
      ap_visitor.TakeAggregationResult();
  auto* process_data =
      V8DetailedMemoryProcessData::ForProcessNode(requesting_process_node_);
  if (process_data) {
    // Shared memory is shared between browsing context groups in the process.
    // We cannot attribute it to a single browsing context group, so we report
    // it as is.
    aggregation_result->shared_memory = mojom::WebMemoryUsage::New();
    aggregation_result->shared_memory->bytes =
        process_data->shared_v8_bytes_used();
    // As we don't have precise attribution for detached and Blink memory,
    // we approximate it using V8 memory.
    double browsing_instance_factor = GetBrowsingInstanceV8BytesFraction(
        requesting_process_node_, browsing_instance_id_);
    aggregation_result->detached_memory = mojom::WebMemoryUsage::New();
    aggregation_result->detached_memory->bytes = static_cast<uint64_t>(
        process_data->detached_v8_bytes_used() * browsing_instance_factor);
    aggregation_result->blink_memory = mojom::WebMemoryUsage::New();
    aggregation_result->blink_memory->bytes = static_cast<uint64_t>(
        process_data->blink_bytes_used() * browsing_instance_factor);
  }
  return aggregation_result;
}

void WebMemoryAggregator::VisitFrame(AggregationPointVisitor* ap_visitor,
                                     const FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_node);
  if (frame_node->GetBrowsingInstanceId() != browsing_instance_id_) {
    // Ignore frames from other browsing contexts.
    return;
  }
  ap_visitor->OnFrameEntered(frame_node);
  for (const WorkerNode* child_worker_node :
       frame_node->GetChildWorkerNodes()) {
    if (child_worker_node->GetWorkerType() !=
        WorkerNode::WorkerType::kDedicated) {
      continue;
    }
    VisitWorker(ap_visitor, child_worker_node);
  }
  for (const FrameNode* child_frame_node : frame_node->GetChildFrameNodes()) {
    VisitFrame(ap_visitor, child_frame_node);
  }
  ap_visitor->OnFrameExited(frame_node);
}

void WebMemoryAggregator::VisitWorker(AggregationPointVisitor* ap_visitor,
                                      const WorkerNode* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40165276): Support service and shared workers.
  DCHECK_EQ(worker_node->GetWorkerType(), WorkerNode::WorkerType::kDedicated);

  ap_visitor->OnWorkerEntered(worker_node);
  for (const WorkerNode* child_worker_node : worker_node->GetChildWorkers()) {
    if (child_worker_node->GetWorkerType() !=
        WorkerNode::WorkerType::kDedicated) {
      continue;
    }

    VisitWorker(ap_visitor, child_worker_node);
  }
  ap_visitor->OnWorkerExited(worker_node);
}

// static
mojom::WebMemoryBreakdownEntry* WebMemoryAggregator::CreateBreakdownEntry(
    AttributionScope scope,
    std::optional<std::string> url,
    mojom::WebMemoryMeasurement* measurement) {
  auto breakdown = mojom::WebMemoryBreakdownEntry::New();
  auto attribution = mojom::WebMemoryAttribution::New();
  attribution->scope = scope;
  attribution->url = std::move(url);
  breakdown->attribution.push_back(std::move(attribution));
  measurement->breakdown.push_back(std::move(breakdown));
  return measurement->breakdown.back().get();
}

// static
void WebMemoryAggregator::SetBreakdownAttributionFromFrame(
    const FrameNode* frame_node,
    mojom::WebMemoryBreakdownEntry* breakdown) {
  DCHECK(breakdown);
  DCHECK(frame_node);
  auto* v8_context_tracker =
      V8ContextTracker::GetFromGraph(frame_node->GetGraph());
  DCHECK(v8_context_tracker);
  auto* ec_state =
      v8_context_tracker->GetExecutionContextState(frame_node->GetFrameToken());
  if (!ec_state)
    return;
  const mojom::IframeAttributionDataPtr& ec_attribution =
      ec_state->iframe_attribution_data;
  if (!ec_attribution)
    return;
  auto* attribution = GetAttributionFromBreakdown(breakdown);
  attribution->id = ec_attribution->id;
  attribution->src = ec_attribution->src;
}

// static
void WebMemoryAggregator::CopyBreakdownAttribution(
    const mojom::WebMemoryBreakdownEntry* from,
    mojom::WebMemoryBreakdownEntry* to) {
  DCHECK(from);
  DCHECK(to);
  const auto* from_attribution = GetAttributionFromBreakdown(from);
  auto* to_attribution = GetAttributionFromBreakdown(to);
  to_attribution->id = from_attribution->id;
  to_attribution->src = from_attribution->src;
}

}  // namespace v8_memory

}  // namespace performance_manager
