// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_WEB_MEMORY_AGGREGATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_WEB_MEMORY_AGGREGATOR_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/mojom/web_memory.mojom.h"
#include "content/public/browser/browsing_instance_id.h"
#include "url/origin.h"

namespace performance_manager {

class FrameNode;
class ProcessNode;
class WorkerNode;

namespace v8_memory {

class AggregationPointVisitor;

// Traverses the graph of execution contexts to find the results of the last
// memory measurement and aggregates them according to the rules defined in the
// performance.measureUserAgentSpecificMemory spec.
// (See public/v8_memory/web_memory.h for the link and spec version.)
class WebMemoryAggregator {
 public:
  // Constructs an aggregator for the results of a memory request from
  // |requesting_node|. This expects the caller to check if |requesting_node|
  // is allowed to measure memory according to the spec.
  //
  // The aggregation is performed by calling AggregateMemoryResult. The graph
  // traversal will not start directly from |requesting_node|, but from the
  // top frame nodes.
  explicit WebMemoryAggregator(const FrameNode* requesting_node);
  ~WebMemoryAggregator();

  WebMemoryAggregator(const WebMemoryAggregator& other) = delete;
  WebMemoryAggregator& operator=(const WebMemoryAggregator& other) = delete;

  // Performs the aggregation.
  mojom::WebMemoryMeasurementPtr AggregateMeasureMemoryResult();

 private:
  friend class AggregationPointVisitor;
  friend class WebMemoryAggregatorTest;

  // FrameNodeVisitor that recursively adds |frame_node| and its children to
  // the aggregation using |ap_visitor|.
  void VisitFrame(AggregationPointVisitor* ap_visitor,
                  const FrameNode* frame_node);

  // WorkerNodeVisitor that recursively adds |worker_node| and its children to
  // the aggregation using |ap_visitor|.
  void VisitWorker(AggregationPointVisitor* ap_visitor,
                   const WorkerNode* worker_node);

  // Creates a new breakdown entry with the given |scope| and |url|, and adds it
  // to the list in |measurement|. Returns a pointer to the newly created entry.
  static mojom::WebMemoryBreakdownEntry* CreateBreakdownEntry(
      mojom::WebMemoryAttribution::Scope scope,
      std::optional<std::string> url,
      mojom::WebMemoryMeasurement* measurement);

  // Sets the id and src attributes of |breakdown| using those stored in the
  // V8ContextTracker for the given |frame_node|.
  static void SetBreakdownAttributionFromFrame(
      const FrameNode* frame_node,
      mojom::WebMemoryBreakdownEntry* breakdown);

  // Copies the id and src attributes from |from| to |to|.
  static void CopyBreakdownAttribution(
      const mojom::WebMemoryBreakdownEntry* from,
      mojom::WebMemoryBreakdownEntry* to);

  // The origin of the node that requests memory measurement.
  const url::Origin requesting_origin_;
  // The process node of the requesting frame.
  const raw_ptr<const ProcessNode> requesting_process_node_;
  // The process node of the main frame.
  const raw_ptr<const ProcessNode> main_process_node_;
  // The browsing instance id of the requesting frame.
  const content::BrowsingInstanceId browsing_instance_id_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace v8_memory

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_WEB_MEMORY_AGGREGATOR_H_
