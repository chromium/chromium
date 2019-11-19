// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_SYSTEM_NODE_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_SYSTEM_NODE_IMPL_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/public/graph/system_node.h"

namespace performance_manager {

class SystemNodeImpl
    : public PublicNodeImpl<SystemNodeImpl, SystemNode>,
      public TypedNodeBase<SystemNodeImpl, SystemNode, SystemNodeObserver> {
 public:
  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kSystem; }

  explicit SystemNodeImpl(GraphImpl* graph);
  ~SystemNodeImpl() override;

  // This should be called after refreshing the memory usage data of the process
  // nodes.
  void OnProcessMemoryMetricsAvailable();

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemNodeImpl);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_SYSTEM_NODE_IMPL_H_
