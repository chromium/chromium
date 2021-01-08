// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/web_memory_aggregator.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/stl_util.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/v8_memory/v8_detailed_memory.h"
#include "components/performance_manager/v8_memory/v8_context_tracker.h"
#include "url/gurl.h"

namespace performance_manager {

namespace v8_memory {

namespace {

using AttributionScope = mojom::WebMemoryAttribution::Scope;

// Returns true if |page_node| has an opener that should be followed by the
// aggregation algorithm.
bool ShouldFollowOpenerLink(const PageNode* page_node) {
  return page_node->GetOpenedType() == PageNode::OpenedType::kPopup;
}

// Returns |frame_node|'s origin based on its current url.
url::Origin GetOrigin(const FrameNode* frame_node) {
  return url::Origin::Create(frame_node->GetURL());
}

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

}  // anonymous namespace


////////////////////////////////////////////////////////////////////////////////
// WebMemoryAggregator

WebMemoryAggregator::WebMemoryAggregator(const FrameNode* requesting_node)
    : requesting_origin_(GetOrigin(requesting_node)),
      aggregation_start_node_(
          internal::FindAggregationStartNode(requesting_node)) {
  DCHECK(aggregation_start_node_);
}

WebMemoryAggregator::~WebMemoryAggregator() = default;

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

  if (requesting_origin_.IsSameOriginWith(GetOrigin(parent_node)))
    return NodeAggregationType::kCrossOriginAggregationPoint;

  // Otherwise |frame_node|'s memory should be aggregated into the last
  // aggregation point.
  return NodeAggregationType::kCrossOriginAggregated;
}

mojom::WebMemoryMeasurementPtr
WebMemoryAggregator::AggregateMeasureMemoryResult() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  aggregation_result_ = mojom::WebMemoryMeasurement::New();
  VisitFrame(nullptr, aggregation_start_node_);
  return std::move(aggregation_result_);
}

bool WebMemoryAggregator::VisitFrame(
    mojom::WebMemoryBreakdownEntry* enclosing_aggregation_point,
    const FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(aggregation_result_);
  DCHECK(enclosing_aggregation_point || frame_node == aggregation_start_node_);
  DCHECK(frame_node);

  // An aggregation point is a node in the graph that holds a memory breakdown
  // covering itself and any descendant nodes that are aggregated into the same
  // breakdown. It is represented directly by the WebMemoryBreakdownEntry
  // object the describes the breakdown since there is no extra information to
  // store about the aggregation point.
  mojom::WebMemoryBreakdownEntry* aggregation_point = nullptr;
  switch (FindNodeAggregationType(frame_node)) {
    case NodeAggregationType::kInvisible:
      // Ignore this node, continue iterating its siblings.
      return true;

    case NodeAggregationType::kSameOriginAggregationPoint:
      // Create a new aggregation point with window scope. Since this node is
      // same-origin to the start node, the start node can view its current
      // url.
      aggregation_point = internal::CreateBreakdownEntry(
          AttributionScope::kWindow, frame_node->GetURL().spec(),
          aggregation_result_.get());
      if (frame_node->IsMainFrame() || frame_node == aggregation_start_node_) {
        // There should be no id or src attribute since there is no visible
        // parent to take them from. Do nothing.
      } else if (internal::GetSameOriginParentOrOpener(frame_node,
                                                       requesting_origin_)) {
        // The parent or opener is also same-origin so the start node can view
        // its attributes. Add the id and src recorded for the node in
        // V8ContextTracker to the new breakdown entry.
        internal::SetBreakdownAttributionFromFrame(frame_node,
                                                   aggregation_point);
      } else {
        // Some grandparent node is the most recent aggregation point whose
        // attributes are visible to the start node, and
        // |enclosing_aggregation_point| includes those attributes. Copy the id
        // and src attributes from there.
        internal::CopyBreakdownAttribution(enclosing_aggregation_point,
                                           aggregation_point);
      }
      break;

    case NodeAggregationType::kCrossOriginAggregationPoint:
      // Create a new aggregation point with cross-origin-aggregated scope.
      // Since this node is NOT same-origin to the start node, the start node
      // CANNOT view its current url.
      aggregation_point = internal::CreateBreakdownEntry(
          AttributionScope::kCrossOriginAggregated, base::nullopt,
          aggregation_result_.get());
      // This is cross-origin but not being aggregated into another aggregation
      // point, so its parent or opener must be same-origin to the start node,
      // which can therefore view its attributes. Add the id and src recorded
      // for the node in V8ContextTracker to the new breakdown entry.
      internal::SetBreakdownAttributionFromFrame(frame_node, aggregation_point);
      break;

    case NodeAggregationType::kCrossOriginAggregated:
      // Update the enclosing aggregation point in-place.
      aggregation_point = enclosing_aggregation_point;
      break;
  }

  // Now update the memory used in the chosen aggregation point.
  DCHECK(aggregation_point);
  if (auto* frame_data =
          V8DetailedMemoryExecutionContextData::ForFrameNode(frame_node)) {
    if (!aggregation_point->memory) {
      aggregation_point->memory = mojom::WebMemoryUsage::New();
    }

    // Ensure this frame is actually in the same process as the requesting
    // frame. If not it should be considered to have 0 bytes.
    // (https://github.com/WICG/performance-measure-memory/issues/20).
    uint64_t bytes_used = (frame_node->GetProcessNode() ==
                           aggregation_start_node_->GetProcessNode())
                              ? frame_data->v8_bytes_used()
                              : 0;
    aggregation_point->memory->bytes += bytes_used;
  }

  // Recurse into children and opened pages. This node's aggregation point
  // becomes the enclosing aggregation point for those nodes. Unretained is safe
  // because the Visit* functions are synchronous.
  frame_node->VisitOpenedPageNodes(
      base::BindRepeating(&WebMemoryAggregator::VisitOpenedPage,
                          base::Unretained(this), aggregation_point));
  return frame_node->VisitChildFrameNodes(
      base::BindRepeating(&WebMemoryAggregator::VisitFrame,
                          base::Unretained(this), aggregation_point));
}

bool WebMemoryAggregator::VisitOpenedPage(
    mojom::WebMemoryBreakdownEntry* enclosing_aggregation_point,
    const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShouldFollowOpenerLink(page_node)) {
    // Visit only the "current" main frame instead of all of the main frames
    // (non-current ones are either about to die, or represent an ongoing
    // navigation).
    return VisitFrame(enclosing_aggregation_point,
                      page_node->GetMainFrameNode());
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// Free functions

namespace internal {

const FrameNode* GetSameOriginParentOrOpener(const FrameNode* frame_node,
                                             const url::Origin& origin) {
  if (auto* parent_or_opener = GetParentOrOpener(frame_node)) {
    if (origin.IsSameOriginWith(GetOrigin(parent_or_opener)))
      return parent_or_opener;
  }
  return nullptr;
}

const FrameNode* FindAggregationStartNode(const FrameNode* requesting_node) {
  DCHECK(requesting_node);
  auto requesting_origin = GetOrigin(requesting_node);

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

mojom::WebMemoryBreakdownEntry* CreateBreakdownEntry(
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

void SetBreakdownAttributionFromFrame(
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

void CopyBreakdownAttribution(const mojom::WebMemoryBreakdownEntry* from,
                              mojom::WebMemoryBreakdownEntry* to) {
  DCHECK(from);
  DCHECK(to);
  const auto* from_attribution = GetAttributionFromBreakdown(from);
  auto* to_attribution = GetAttributionFromBreakdown(to);
  to_attribution->id = from_attribution->id;
  to_attribution->src = from_attribution->src;
}

}  // namespace internal

}  // namespace v8_memory

}  // namespace performance_manager
