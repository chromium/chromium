// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_SYSTEM_NODE_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_SYSTEM_NODE_IMPL_H_

#include <cstdint>
#include <memory>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/graph/properties.h"
#include "components/performance_manager/public/graph/system_node.h"

namespace performance_manager {

class SystemNodeImpl
    : public PublicNodeImpl<SystemNodeImpl, SystemNode>,
      public TypedNodeBase<SystemNodeImpl, SystemNode, SystemNodeObserver> {
 public:
  using TypedNodeBase<SystemNodeImpl, SystemNode, SystemNodeObserver>::FromNode;

  SystemNodeImpl();

  SystemNodeImpl(const SystemNodeImpl&) = delete;
  SystemNodeImpl& operator=(const SystemNodeImpl&) = delete;

  ~SystemNodeImpl() override;

  // Implements NodeBase:
  void RemoveNodeAttachedData() override;

  // This should be called after refreshing the memory usage data of the process
  // nodes.
  void OnProcessMemoryMetricsAvailable();

  void OnMemoryPressureForTesting(MemoryPressureLevel new_level) {
    OnMemoryPressure(new_level);
  }

  base::WeakPtr<SystemNodeImpl> GetWeakPtr() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return weak_factory_.GetWeakPtr();
  }

 private:
  void OnMemoryPressure(MemoryPressureLevel new_level);

  // The memory pressure listener.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  base::WeakPtrFactory<SystemNodeImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_SYSTEM_NODE_IMPL_H_
