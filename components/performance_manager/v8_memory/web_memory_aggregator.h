// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_WEB_MEMORY_AGGREGATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_WEB_MEMORY_AGGREGATOR_H_

#include <string>

#include "base/optional.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/mojom/web_memory.mojom.h"
#include "url/origin.h"

namespace performance_manager {

class FrameNode;
class PageNode;

namespace v8_memory {

// Traverses the graph of execution contexts to find the results of the last
// memory measurement and aggregates them according to the rules defined in the
// performance.measureMemory spec. (See public/v8_memory/web_memory.h for the
// link and spec version.)
class WebMemoryAggregator {
 public:
  // Constructs an aggregator for the results of a memory request from
  // |requesting_node|. This expects the caller to check if |requesting_node|
  // is allowed to measure memory according to the spec.
  //
  // The aggregation is performed by calling AggregateMemoryResult. The graph
  // traversal will not start directly from |requesting_node|, but from the
  // highest node in the frame tree that is visible to it as found by
  // FindAggregationStartNode. (This allows a same-origin subframe to request
  // memory for the whole page it's embedded in.)
  explicit WebMemoryAggregator(const FrameNode* requesting_node);
  ~WebMemoryAggregator();

  WebMemoryAggregator(const WebMemoryAggregator& other) = delete;
  WebMemoryAggregator& operator=(const WebMemoryAggregator& other) = delete;

  // The various ways a node can be treated during the aggregation.
  enum class NodeAggregationType {
    // Node is same-origin to |requesting_node|; will be a new aggregation
    // point with scope "Window".
    kSameOriginAggregationPoint,
    // Node is cross-origin with |requesting_node| but its parent is not; will
    // be a new aggregation point with scope
    // "cross-origin-aggregated".
    kCrossOriginAggregationPoint,
    // Node is cross-origin with |requesting_node| and so is its parent; will
    // be aggregated into its parent's aggregation point.
    kCrossOriginAggregated,
    // Node is in a different browsing context group; will not be added to the
    // aggregation.
    kInvisible,
  };

  // Returns the origin of |requesting_node|.
  const url::Origin& requesting_origin() const { return requesting_origin_; }

  // Returns the way that |frame_node| should be treated during the
  // aggregation.  |aggregation_start_node_| must be reachable from
  // |frame_node| by following parent/child or opener links. This will always
  // be true if |frame_node| comes from a call to VisitFrame.
  NodeAggregationType FindNodeAggregationType(const FrameNode* frame_node);

  // Performs the aggregation.
  mojom::WebMemoryMeasurementPtr AggregateMeasureMemoryResult();

 private:
  // FrameNodeVisitor that recursively adds |frame_node| and its children to
  // the aggregation. |enclosing_aggregation_point| is the aggregation point
  // that |frame_node|'s parent or opener is in. Always returns true to
  // continue traversal.
  bool VisitFrame(mojom::WebMemoryBreakdownEntry* enclosing_aggregation_point,
                  const FrameNode* frame_node);

  // PageNodeVisitor that recursively adds |page_node|'s main frames and their
  // children to the aggregation. |enclosing_aggregation_point| is the
  // aggregation point that |page_node|'s opener is in. Always returns true to
  // continue traversal.
  bool VisitOpenedPage(
      mojom::WebMemoryBreakdownEntry* enclosing_aggregation_point,
      const PageNode* page_node);

  // The origin of |requesting_node|. Cached so it doesn't have to be
  // recalculated in each call to VisitFrame.
  const url::Origin requesting_origin_;

  // The node that the graph traversal should start from, found from
  // |requesting_node| using FindAggregationStartNode.
  const FrameNode* aggregation_start_node_;

  // Stores the result of the aggregation. This is populated by
  // AggregateMeasureMemoryResult.
  mojom::WebMemoryMeasurementPtr aggregation_result_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

namespace internal {

// These functions are used in the implementation and exposed in the header for
// testing.

// Returns |frame_node|'s parent or opener if the parent or opener is
// same-origin with |origin|, nullptr otherwise.
const FrameNode* GetSameOriginParentOrOpener(const FrameNode* frame_node,
                                             const url::Origin& origin);

// Walks back the chain of parents and openers from |requesting_node| to find
// the farthest ancestor that should be visible to it (all intermediate nodes
// in the chain are same-origin).
const FrameNode* FindAggregationStartNode(const FrameNode* requesting_node);

// Creates a new breakdown entry with the given |scope| and |url|, and adds it
// to the list in |measurement|. Returns a pointer to the newly created entry.
mojom::WebMemoryBreakdownEntry* CreateBreakdownEntry(
    mojom::WebMemoryAttribution::Scope scope,
    base::Optional<std::string> url,
    mojom::WebMemoryMeasurement* measurement);

// Sets the id and src attributes of |breakdown| using those stored in the
// V8ContextTracker for the given |frame_node|.
void SetBreakdownAttributionFromFrame(
    const FrameNode* frame_node,
    mojom::WebMemoryBreakdownEntry* breakdown);

// Copies the id and src attributes from |from| to |to|.
void CopyBreakdownAttribution(const mojom::WebMemoryBreakdownEntry* from,
                              mojom::WebMemoryBreakdownEntry* to);

}  // namespace internal

}  // namespace v8_memory

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_WEB_MEMORY_AGGREGATOR_H_
