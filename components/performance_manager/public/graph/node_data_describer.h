// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_DATA_DESCRIBER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_DATA_DESCRIBER_H_

#include "base/values.h"

namespace performance_manager {

class FrameNode;
class PageNode;
class ProcessNode;
class SystemNode;
class WorkerNode;

// An interface for decoding node-private data for ultimate display as
// human-comprehensible text to allow diagnosis of node private data.
//
// Typically describers should be returning a dictionary, as the keys are the
// labels that will be attached to the data in the display. Care should be
// taken to make a difference between "has no data" and "has empty data". In the
// case where the describer truly has no data regarding a node, it should return
// a base::Value() (null value); in the case where the object being described
// knows about the node and has some data structure allocated relative to that
// node, but which is presently empty, it makes more sense to return an empty
// base::DictionaryValue. A null value will result in no data being displayed
// on the graph UI, while an empty dictionary will be displayed. Similarly, if
// the value being presented is a potentially null string, making that
// distinction by returning a null value instead of an empty string is
// worthwhile.
//
// In general, describers should return values using the base::Value type that
// is most appropriate. If no appropriate type exists (ie, a 64-bit integer
// value, or a time value, etc), prefer to use a human-readable string. See
// node_data_describer_util.h for helper functions for common value types.
class NodeDataDescriber {
 public:
  virtual ~NodeDataDescriber() = default;

  virtual base::Value DescribeFrameNodeData(const FrameNode* node) const = 0;
  virtual base::Value DescribePageNodeData(const PageNode* node) const = 0;
  virtual base::Value DescribeProcessNodeData(
      const ProcessNode* node) const = 0;
  virtual base::Value DescribeSystemNodeData(const SystemNode* node) const = 0;
  virtual base::Value DescribeWorkerNodeData(const WorkerNode* node) const = 0;
};

// A convenience do-nothing implementation of the interface above. Returns
// an is_none() value for all nodes.
class NodeDataDescriberDefaultImpl : public NodeDataDescriber {
 public:
  base::Value DescribeFrameNodeData(const FrameNode* node) const override;
  base::Value DescribePageNodeData(const PageNode* node) const override;
  base::Value DescribeProcessNodeData(const ProcessNode* node) const override;
  base::Value DescribeSystemNodeData(const SystemNode* node) const override;
  base::Value DescribeWorkerNodeData(const WorkerNode* node) const override;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_DATA_DESCRIBER_H_
