// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/browser_child_process_host_proxy.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "content/public/browser/browsing_instance_id.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/process_type.h"
#include "third_party/blink/public/common/tokens/tokens.h"

class GURL;

namespace content {
class WebContents;
}

namespace url {
class Origin;
}

namespace performance_manager {

struct BrowserProcessNodeTag;

// The performance manager is a rendezvous point for binding to performance
// manager interfaces. All functions need to be called from the main thread
// only.
class PerformanceManagerImpl : public PerformanceManager {
 public:
  PerformanceManagerImpl(const PerformanceManagerImpl&) = delete;
  PerformanceManagerImpl& operator=(const PerformanceManagerImpl&) = delete;

  ~PerformanceManagerImpl() override;

  // Same as GetGraph(), but returns a GraphImpl*.
  static GraphImpl* GetGraphImpl();

  // Creates, initializes and registers an instance. Valid to call from the main
  // thread only.
  static std::unique_ptr<PerformanceManagerImpl> Create();

  // Unregisters |instance| and arranges for its deletion.
  static void Destroy(std::unique_ptr<PerformanceManager> instance);

  // Creates a new node of the requested type and adds it to the graph.
  static std::unique_ptr<FrameNodeImpl> CreateFrameNode(
      ProcessNodeImpl* process_node,
      PageNodeImpl* page_node,
      FrameNodeImpl* parent_frame_node,
      FrameNodeImpl* outer_document_for_fenced_frame,
      int render_frame_id,
      const blink::LocalFrameToken& frame_token,
      content::BrowsingInstanceId browsing_instance_id,
      content::SiteInstanceGroupId site_instance_group_id,
      bool is_current,
      bool is_active);
  static std::unique_ptr<PageNodeImpl> CreatePageNode(
      base::WeakPtr<content::WebContents> web_contents,
      const std::string& browser_context_id,
      const GURL& visible_url,
      PagePropertyFlags initial_properties,
      base::TimeTicks visibility_change_time);
  static std::unique_ptr<ProcessNodeImpl> CreateProcessNode(
      BrowserProcessNodeTag tag);
  static std::unique_ptr<ProcessNodeImpl> CreateProcessNode(
      RenderProcessHostProxy proxy,
      base::TaskPriority priority);
  static std::unique_ptr<ProcessNodeImpl> CreateProcessNode(
      content::ProcessType process_type,
      BrowserChildProcessHostProxy proxy);
  static std::unique_ptr<WorkerNodeImpl> CreateWorkerNode(
      const std::string& browser_context_id,
      WorkerNode::WorkerType worker_type,
      ProcessNodeImpl* process_node,
      const blink::WorkerToken& worker_token,
      const url::Origin& origin);

  // Destroys a node returned from the creation functions above.
  static void DeleteNode(std::unique_ptr<NodeBase> node);

  // Destroys multiples nodes in one single task. Equivalent to calling
  // DeleteNode() on all elements of the vector. This function takes care of
  // removing them from the graph in topological order and destroying them.
  static void BatchDeleteNodes(std::vector<std::unique_ptr<NodeBase>> nodes);

 private:
  friend class PerformanceManager;

  PerformanceManagerImpl();

  template <typename NodeType, typename... Args>
  static std::unique_ptr<NodeType> CreateNodeImpl(Args&&... constructor_args);

  GraphImpl graph_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_IMPL_H_
