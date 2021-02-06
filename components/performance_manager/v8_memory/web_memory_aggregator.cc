// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/web_memory_aggregator.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/containers/stack.h"
#include "base/memory/checked_ptr.h"
#include "base/stl_util.h"
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
class WebMemoryAggregator::AggregationPointVisitor {
 public:
  AggregationPointVisitor(const FrameNode* aggregation_start_node,
                          const url::Origin& requesting_origin);

  ~AggregationPointVisitor();

  AggregationPointVisitor(const AggregationPointVisitor& other) = delete;
  AggregationPointVisitor& operator=(const AggregationPointVisitor& other) =
      delete;

  mojom::WebMemoryMeasurementPtr TakeAggregationResult();

  // Called on first visiting |frame_node| in a depth-first traversal.
  // |aggregation_type| specificies how to treat the node in the aggregation.
  void OnFrameEntered(const FrameNode* frame_node,
                      NodeAggregationType aggregation_type);

  // Called after visiting |frame_node| and all its children in a depth-first
  // traversal.
  void OnFrameExited(const FrameNode* frame_node);

  // Called on first visiting |worker_node| in a depth-first traversal.
  // |aggregation_type| specificies how to treat the node in the aggregation.
  void OnWorkerEntered(const WorkerNode* worker_node,
                       NodeAggregationType aggregation_type);

  // Called after visiting |worker_node| and all its children in a depth-first
  // traversal.
  void OnWorkerExited(const WorkerNode* worker_node);

 private:
  CheckedPtr<const FrameNode> aggregation_start_node_;
  const url::Origin requesting_origin_;
  mojom::WebMemoryMeasurementPtr aggregation_result_ =
      mojom::WebMemoryMeasurement::New();
  base::stack<mojom::WebMemoryBreakdownEntry*> enclosing_aggregation_points_;
};

namespace {

using AttributionScope = mojom::WebMemoryAttribution::Scope;

// Returns true if |page_node| has an opener that should be followed by the
// aggregation algorithm.
bool ShouldFollowOpenerLink(const PageNode* page_node) {
  return page_node->GetOpenedType() == PageNode::OpenedType::kPopup;
}

// Returns |frame_node|'s origin based on its current url.
// An about:blank iframe inherits the origin of its parent. See:
// https://html.spec.whatwg.org/multipage/browsers.html#determining-the-origin
url::Origin GetOrigin(const FrameNode* frame_node) {
  if (frame_node->GetParentFrameNode()) {
    return url::Origin::Resolve(
        frame_node->GetURL(),
        url::Origin::Create(frame_node->GetParentFrameNode()->GetURL()));
  } else {
    return url::Origin::Create(frame_node->GetURL());
  }
}

#if DCHECK_IS_ON()
// Returns |worker_node|'s origin based on its current url.
url::Origin GetOrigin(const WorkerNode* worker_node) {
  return url::Origin::Create(worker_node->GetURL());
}
#endif

// Returns the parent of |frame_node|, the opener if it has no parent, or
// nullptr if it has neither.
const FrameNode* GetParentOrOpener(const FrameNode* frame_node) {
  // Only the main frame of a page should have an opener. So first check if
  // there's a parent and, if not, check if there's an opener.
  if (auto* parent = frame_node->GetParentFrameNode())
    return parent;
  auto* page_node = frame_node->GetPageNode();
  DCHECK(page_node);
  if (ShouldFollowOpenerLink(page_node))
    return page_node->GetOpenerFrameNode();
  return nullptr;
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
      // TODO(crbug.com/1169168): Support service and shared workers.
      NOTREACHED();
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
}

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// AggregationPointVisitor

WebMemoryAggregator::AggregationPointVisitor::AggregationPointVisitor(
    const FrameNode* aggregation_start_node,
    const url::Origin& requesting_origin)
    : aggregation_start_node_(aggregation_start_node),
      requesting_origin_(requesting_origin) {}

WebMemoryAggregator::AggregationPointVisitor::~AggregationPointVisitor() =
    default;

mojom::WebMemoryMeasurementPtr
WebMemoryAggregator::AggregationPointVisitor::TakeAggregationResult() {
  DCHECK(aggregation_result_);
  auto result = std::move(aggregation_result_);
  aggregation_result_ = nullptr;
  return result;
}

void WebMemoryAggregator::AggregationPointVisitor::OnFrameEntered(
    const FrameNode* frame_node,
    NodeAggregationType aggregation_type) {
  DCHECK(frame_node);
  DCHECK_EQ(enclosing_aggregation_points_.empty(),
            frame_node == aggregation_start_node_);
  mojom::WebMemoryBreakdownEntry* aggregation_point = nullptr;
  switch (aggregation_type) {
    case NodeAggregationType::kInvisible:
      NOTREACHED();
      return;

    case NodeAggregationType::kSameOriginAggregationPoint:
      // Create a new aggregation point with window scope. Since this node is
      // same-origin to the start node, the start node can view its current
      // url.
      aggregation_point = CreateBreakdownEntry(AttributionScope::kWindow,
                                               frame_node->GetURL().spec(),
                                               aggregation_result_.get());
      if (frame_node->IsMainFrame() || frame_node == aggregation_start_node_) {
        // There should be no id or src attribute since there is no visible
        // parent to take them from. Do nothing.
      } else if (GetSameOriginParentOrOpener(frame_node, requesting_origin_)) {
        // The parent or opener is also same-origin so the start node can view
        // its attributes. Add the id and src recorded for the node in
        // V8ContextTracker to the new breakdown entry.
        SetBreakdownAttributionFromFrame(frame_node, aggregation_point);
      } else {
        // Some grandparent node is the most recent aggregation point whose
        // attributes are visible to the start node, and
        // |enclosing_aggregation_point| includes those attributes. Copy the
        // id and src attributes from there.
        CopyBreakdownAttribution(enclosing_aggregation_points_.top(),
                                 aggregation_point);
      }
      break;

    case NodeAggregationType::kCrossOriginAggregationPoint:
      // Create a new aggregation point with cross-origin-aggregated scope.
      // Since this node is NOT same-origin to the start node, the start node
      // CANNOT view its current url.
      aggregation_point =
          CreateBreakdownEntry(AttributionScope::kCrossOriginAggregated,
                               base::nullopt, aggregation_result_.get());
      // This is cross-origin but not being aggregated into another
      // aggregation point, so its parent or opener must be same-origin to the
      // start node, which can therefore view its attributes. Add the id and
      // src recorded for the node in V8ContextTracker to the new breakdown
      // entry.
      SetBreakdownAttributionFromFrame(frame_node, aggregation_point);
      break;

    case NodeAggregationType::kCrossOriginAggregated:
      // Update the enclosing aggregation point in-place.
      aggregation_point = enclosing_aggregation_points_.top();
      break;
  }

  // Now update the memory used in the chosen aggregation point.
  DCHECK(aggregation_point);
  AddMemoryBytes(aggregation_point,
                 V8DetailedMemoryExecutionContextData::ForFrameNode(frame_node),
                 frame_node->GetProcessNode() ==
                     aggregation_start_node_->GetProcessNode());

  enclosing_aggregation_points_.push(aggregation_point);
}

void WebMemoryAggregator::AggregationPointVisitor::OnFrameExited(
    const FrameNode* frame_node) {
  DCHECK(!enclosing_aggregation_points_.empty());
  enclosing_aggregation_points_.pop();
}

void WebMemoryAggregator::AggregationPointVisitor::OnWorkerEntered(
    const WorkerNode* worker_node,
    NodeAggregationType aggregation_type) {
  DCHECK(worker_node);

  // Aggregation starts from a frame node, so the enclosing aggregation point
  // is guaranteed to exist.
  DCHECK(!enclosing_aggregation_points_.empty());

  mojom::WebMemoryBreakdownEntry* aggregation_point = nullptr;
  switch (aggregation_type) {
    case NodeAggregationType::kSameOriginAggregationPoint:
      // Create a new aggregation point with window scope. Since this node is
      // same-origin to the start node, the start node can view its current
      // url.
      aggregation_point = CreateBreakdownEntry(
          AttributionScopeFromWorkerType(worker_node->GetWorkerType()),
          worker_node->GetURL().spec(), aggregation_result_.get());
      CopyBreakdownAttribution(enclosing_aggregation_points_.top(),
                               aggregation_point);
      break;

    case NodeAggregationType::kCrossOriginAggregated:
      // Update the enclosing aggregation point in-place.
      aggregation_point = enclosing_aggregation_points_.top();
      break;

    case NodeAggregationType::kInvisible:
    case NodeAggregationType::kCrossOriginAggregationPoint:
      NOTREACHED();
      return;
  }

  // Now update the memory used in the chosen aggregation point.
  DCHECK(aggregation_point);
  AddMemoryBytes(
      aggregation_point,
      V8DetailedMemoryExecutionContextData::ForWorkerNode(worker_node),
      worker_node->GetProcessNode() ==
          aggregation_start_node_->GetProcessNode());

  enclosing_aggregation_points_.push(aggregation_point);
}

void WebMemoryAggregator::AggregationPointVisitor::OnWorkerExited(
    const WorkerNode* worker_node) {
  DCHECK(!enclosing_aggregation_points_.empty());
  enclosing_aggregation_points_.pop();
}

////////////////////////////////////////////////////////////////////////////////
// WebMemoryAggregator

WebMemoryAggregator::WebMemoryAggregator(const FrameNode* requesting_node)
    : requesting_origin_(GetOrigin(requesting_node)),
      aggregation_start_node_(FindAggregationStartNode(requesting_node)) {
  DCHECK(aggregation_start_node_);
}

WebMemoryAggregator::~WebMemoryAggregator() = default;

mojom::WebMemoryMeasurementPtr
WebMemoryAggregator::AggregateMeasureMemoryResult() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AggregationPointVisitor ap_visitor(aggregation_start_node_,
                                     requesting_origin_);
  VisitFrame(&ap_visitor, aggregation_start_node_);

  mojom::WebMemoryMeasurementPtr aggregation_result =
      ap_visitor.TakeAggregationResult();
  auto* process_data = V8DetailedMemoryProcessData::ForProcessNode(
      aggregation_start_node_->GetProcessNode());
  if (process_data) {
    aggregation_result->detached_memory = mojom::WebMemoryUsage::New();
    aggregation_result->detached_memory->bytes =
        process_data->detached_v8_bytes_used();
    aggregation_result->shared_memory = mojom::WebMemoryUsage::New();
    aggregation_result->shared_memory->bytes =
        process_data->shared_v8_bytes_used();
  }

  return aggregation_result;
}

WebMemoryAggregator::NodeAggregationType
WebMemoryAggregator::FindNodeAggregationType(const FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  auto* node = frame_node;
  while (node && node != aggregation_start_node_) {
    node = GetParentOrOpener(node);
  }
  // Should have broken out of the loop by reaching the start node, not nullptr.
  DCHECK_EQ(node, aggregation_start_node_);
#endif

  // If |frame_node| is in a different browsing context group from |start_node|
  // it should be invisible.
  if (frame_node->GetBrowsingInstanceId() !=
      aggregation_start_node_->GetBrowsingInstanceId()) {
    return NodeAggregationType::kInvisible;
  }

  auto frame_origin = GetOrigin(frame_node);

  // If |frame_node| is same-origin to |start_node|, it's an aggregation point.
  // (This trivially includes the |start_node| itself.)
  if (requesting_origin_.IsSameOriginWith(frame_origin))
    return NodeAggregationType::kSameOriginAggregationPoint;
  DCHECK_NE(frame_node, aggregation_start_node_);

  // If |frame_node| is cross-origin from |start_node|, but is a direct child of
  // a same-origin node, its existence is visible to |start_node| so it's an
  // aggregation point. But its current url will be hidden from |start_node|.
  const FrameNode* parent_node = frame_node->GetParentFrameNode();

  if (!parent_node) {
    // A cross-origin window opened via window.open gets its own browsing
    // context group due to COOP. However, while the window is being loaded it
    // belongs to the old browsing context group. In that case the origin is
    // opaque.
    DCHECK(frame_origin.opaque());
    return NodeAggregationType::kInvisible;
  }

  auto parent_origin = GetOrigin(parent_node);
  if (requesting_origin_.IsSameOriginWith(parent_origin)) {
    return NodeAggregationType::kCrossOriginAggregationPoint;
  }

  // Otherwise |frame_node|'s memory should be aggregated into the last
  // aggregation point.
  return NodeAggregationType::kCrossOriginAggregated;
}

WebMemoryAggregator::NodeAggregationType
WebMemoryAggregator::FindNodeAggregationType(const WorkerNode* worker_node,
                                             NodeAggregationType parent_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1169168): Support service and shared workers.
  DCHECK_EQ(worker_node->GetWorkerType(), WorkerNode::WorkerType::kDedicated);
  // A dedicated worker is guaranteed to have the same origin as its parent,
  // which means that a dedicated worker cannot be a cross-origin aggregation
  // point.
#if DCHECK_IS_ON()
  // TODO(crbug.com/1169178): The URL of a worker node is currently not
  // available without PlzDedicatedWorker, which is disabled by default. Remove
  // this guard once the URL is properly propagated to PM.
  if (!worker_node->GetURL().is_empty()) {
    auto worker_origin = GetOrigin(worker_node);
    auto client_frames = worker_node->GetClientFrames();
    DCHECK(std::all_of(client_frames.begin(), client_frames.end(),
                       [worker_origin](const FrameNode* client) {
                         return worker_origin.IsSameOriginWith(
                             GetOrigin(client));
                       }));
    auto client_workers = worker_node->GetClientWorkers();
    DCHECK(std::all_of(client_workers.begin(), client_workers.end(),
                       [worker_origin](const WorkerNode* client) {
                         return worker_origin.IsSameOriginWith(
                             GetOrigin(client));
                       }));
  }
#endif
  switch (parent_type) {
    case NodeAggregationType::kCrossOriginAggregationPoint:
      return NodeAggregationType::kCrossOriginAggregated;
    case NodeAggregationType::kCrossOriginAggregated:
    case NodeAggregationType::kSameOriginAggregationPoint:
      return parent_type;
    case NodeAggregationType::kInvisible:
      // Visitation stops at an invisible node and does not enter its children.
      NOTREACHED();
      return NodeAggregationType::kInvisible;
  }
}

bool WebMemoryAggregator::VisitFrame(AggregationPointVisitor* ap_visitor,
                                     const FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_node);

  // An aggregation point is a node in the graph that holds a memory breakdown
  // covering itself and any descendant nodes that are aggregated into the same
  // breakdown. It is represented directly by the WebMemoryBreakdownEntry
  // object the describes the breakdown since there is no extra information to
  // store about the aggregation point.
  auto aggregation_type = FindNodeAggregationType(frame_node);
  if (aggregation_type == NodeAggregationType::kInvisible) {
    // Ignore this node, continue iterating its siblings.
    return true;
  }

  ap_visitor->OnFrameEntered(frame_node, aggregation_type);

  // Recurse into children and opened pages. Unretained is safe because the
  // Visit* functions are synchronous.
  frame_node->VisitOpenedPageNodes(
      base::BindRepeating(&WebMemoryAggregator::VisitOpenedPage,
                          base::Unretained(this), ap_visitor));
  frame_node->VisitChildDedicatedWorkers(base::BindRepeating(
      &WebMemoryAggregator::VisitWorker, base::Unretained(this), ap_visitor,
      aggregation_type));
  frame_node->VisitChildFrameNodes(base::BindRepeating(
      &WebMemoryAggregator::VisitFrame, base::Unretained(this), ap_visitor));

  ap_visitor->OnFrameExited(frame_node);

  return true;
}

bool WebMemoryAggregator::VisitWorker(
    AggregationPointVisitor* ap_visitor,
    NodeAggregationType parent_aggregation_type,
    const WorkerNode* worker_node) {
  // TODO(crbug.com/1169168): Support service and shared workers.
  DCHECK_EQ(worker_node->GetWorkerType(), WorkerNode::WorkerType::kDedicated);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  NodeAggregationType aggregation_type =
      FindNodeAggregationType(worker_node, parent_aggregation_type);
  if (aggregation_type == NodeAggregationType::kInvisible) {
    // Ignore this node, continue iterating its siblings.
    return true;
  }

  ap_visitor->OnWorkerEntered(worker_node, aggregation_type);

  worker_node->VisitChildDedicatedWorkers(base::BindRepeating(
      &WebMemoryAggregator::VisitWorker, base::Unretained(this), ap_visitor,
      aggregation_type));

  ap_visitor->OnWorkerExited(worker_node);

  return true;
}

bool WebMemoryAggregator::VisitOpenedPage(AggregationPointVisitor* ap_visitor,
                                          const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShouldFollowOpenerLink(page_node)) {
    // Visit only the "current" main frame instead of all of the main frames
    // (non-current ones are either about to die, or represent an ongoing
    // navigation).
    return VisitFrame(ap_visitor, page_node->GetMainFrameNode());
  }
  return true;
}

// static
const FrameNode* WebMemoryAggregator::GetSameOriginParentOrOpener(
    const FrameNode* frame_node,
    const url::Origin& origin) {
  if (auto* parent_or_opener = GetParentOrOpener(frame_node)) {
    if (origin.IsSameOriginWith(GetOrigin(parent_or_opener)))
      return parent_or_opener;
  }
  return nullptr;
}

// static
const FrameNode* WebMemoryAggregator::FindAggregationStartNode(
    const FrameNode* requesting_node) {
  DCHECK(requesting_node);
  auto requesting_origin = GetOrigin(requesting_node);
  DCHECK(!requesting_origin.opaque());

  // Follow parent and opener links to find the most general same-site node to
  // start the aggregation traversal from.
  const FrameNode* start_node = nullptr;
  for (auto* parent_or_opener = requesting_node; parent_or_opener;
       parent_or_opener =
           GetSameOriginParentOrOpener(parent_or_opener, requesting_origin)) {
    // Only consider nodes in the same process as potential start nodes.
    // (https://github.com/WICG/performance-measure-memory/issues/20).
    if (parent_or_opener->GetProcessNode() ==
        requesting_node->GetProcessNode()) {
      start_node = parent_or_opener;
    }
  }

  DCHECK(start_node);
  DCHECK(requesting_origin.IsSameOriginWith(GetOrigin(start_node)));

  // Make sure we didn't break out of the browsing context group.
  DCHECK_EQ(start_node->GetBrowsingInstanceId(),
            requesting_node->GetBrowsingInstanceId());
  return start_node;
}

// static
mojom::WebMemoryBreakdownEntry* WebMemoryAggregator::CreateBreakdownEntry(
    AttributionScope scope,
    base::Optional<std::string> url,
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
