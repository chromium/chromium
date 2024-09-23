// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_POLICIES_PREFETCH_VIRTUAL_MEMORY_POLICY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_POLICIES_PREFETCH_VIRTUAL_MEMORY_POLICY_H_

#include "base/memory/weak_ptr.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager::policies {

// Policy that refreshes the main browser DLL in RAM based on conditions in
// the NeedToRefresh function. Currently it is just elapsed time since the
// last refresh. Initial timestamp is from creation of the policy.
class PrefetchVirtualMemoryPolicy : public GraphOwned,
                                    public ProcessNode::ObserverDefaultImpl {
 public:
  explicit PrefetchVirtualMemoryPolicy(const base::FilePath file_to_prefetch);
  PrefetchVirtualMemoryPolicy(const PrefetchVirtualMemoryPolicy&) = delete;
  PrefetchVirtualMemoryPolicy(PrefetchVirtualMemoryPolicy&&) = delete;
  PrefetchVirtualMemoryPolicy& operator=(const PrefetchVirtualMemoryPolicy&) =
      delete;
  PrefetchVirtualMemoryPolicy& operator=(PrefetchVirtualMemoryPolicy&&) =
      delete;
  ~PrefetchVirtualMemoryPolicy() override;

 private:
  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // ProcessNodeObserver implementation:
  void OnProcessNodeAdded(const ProcessNode* process_node) override;

  bool NeedToRefresh();

  base::FilePath file_to_prefetch_;
  base::TimeTicks last_prefetch_time_;
  bool ongoing_preread_;
  base::WeakPtrFactory<PrefetchVirtualMemoryPolicy> weak_ptr_factory_;
};

}  // namespace performance_manager::policies

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_POLICIES_PREFETCH_VIRTUAL_MEMORY_POLICY_H_
