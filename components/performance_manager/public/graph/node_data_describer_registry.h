// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_DATA_DESCRIBER_REGISTRY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_DATA_DESCRIBER_REGISTRY_H_

#include <string_view>

#include "base/values.h"

namespace performance_manager {

class Node;
class NodeDataDescriber;

// Allows registering NodeDataDescribers.
class NodeDataDescriberRegistry {
 public:
  virtual ~NodeDataDescriberRegistry() = default;

  // Register |describer| with |name|.
  // The |describer| must not be registered already, and |name| must be unique
  // to this registration.
  virtual void RegisterDescriber(const NodeDataDescriber* describer,
                                 std::string_view name) = 0;
  // Unregister previously registered |describer|.
  virtual void UnregisterDescriber(const NodeDataDescriber* describer) = 0;

  // Invoke all registered describers for |node| and return a dictionary from
  // their name to their description - if any.
  virtual base::Value::Dict DescribeNodeData(const Node* node) const = 0;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_DATA_DESCRIBER_REGISTRY_H_
