// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_POLICIES_PROCESS_PRIORITY_POLICY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_POLICIES_PROCESS_PRIORITY_POLICY_H_

#include "base/callback.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/render_process_host_proxy.h"

namespace performance_manager {
namespace policies {

// Policy that observes priority changes on ProcessNodes, and applies these
// to the actual processes via RenderProcessHost::SetPriorityOverride. There
// is no need for more than one of these to be instantiated at a time (enforced
// by a DCHECK). This policy expects to be attached to an empty graph (also
// enforced by a DCHECK).
class ProcessPriorityPolicy : public GraphOwned,
                              public ProcessNode::ObserverDefaultImpl {
 public:
  using SetPriorityOnUiThreadCallback =
      base::RepeatingCallback<void(RenderProcessHostProxy rph_proxy,
                                   bool foreground)>;

  ProcessPriorityPolicy();
  ProcessPriorityPolicy(const ProcessPriorityPolicy&) = delete;
  ProcessPriorityPolicy(ProcessPriorityPolicy&&) = delete;
  ProcessPriorityPolicy& operator=(const ProcessPriorityPolicy&) = delete;
  ProcessPriorityPolicy& operator=(ProcessPriorityPolicy&&) = delete;
  ~ProcessPriorityPolicy() override;

  // Testing seams. This allows testing this class without requiring a full
  // browser test.
  static void SetCallbackForTesting(SetPriorityOnUiThreadCallback callback);
  static void ClearCallbackForTesting();

 private:
  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // ProcessNodeObserver implementation:
  void OnProcessNodeAdded(const ProcessNode* process_node) override;
  void OnPriorityChanged(const ProcessNode* process_node,
                         base::TaskPriority previous_value) override;
};

}  // namespace policies
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_POLICIES_PROCESS_PRIORITY_POLICY_H_
