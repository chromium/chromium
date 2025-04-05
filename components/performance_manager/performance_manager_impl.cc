// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/performance_manager_impl.h"

#include <atomic>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/delayed_task_handle.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/system_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace performance_manager {

namespace {

// Singleton instance of PerformanceManagerImpl. Set from the constructor, and
// reset from the destructor. Null if the singleton instance doesn't exist,
// which only happens very early or very late in the process lifetime (or in
// some tests).
PerformanceManagerImpl* g_performance_manager = nullptr;

// Removes a frame tree from the graph starting from the leaf nodes.
void RemoveFrameAndChildrenFromGraph(FrameNodeImpl* frame_node,
                                     GraphImpl* graph) {
  // Recurse on the first child while there is one.
  while (!frame_node->child_frame_nodes().empty()) {
    RemoveFrameAndChildrenFromGraph(*(frame_node->child_frame_nodes().begin()),
                                    graph);
  }

  // Now that all children are deleted, delete this frame.
  graph->RemoveNode(frame_node);
}

}  // namespace

// static
bool PerformanceManager::IsAvailable() {
  return g_performance_manager;
}

PerformanceManagerImpl::~PerformanceManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(g_performance_manager, this);
  // TODO(crbug.com/40629049): Move this to a TearDown function.
  graph_.TearDown();
  g_performance_manager = nullptr;
}

// static
GraphImpl* PerformanceManagerImpl::GetGraphImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(g_performance_manager);
  return &g_performance_manager->graph_;
}

// static
std::unique_ptr<PerformanceManagerImpl> PerformanceManagerImpl::Create() {
  return base::WrapUnique(new PerformanceManagerImpl());
}

// static
void PerformanceManagerImpl::Destroy(
    std::unique_ptr<PerformanceManager> instance) {
  // `instance` is deleted at the end of this function.
}

// static
std::unique_ptr<FrameNodeImpl> PerformanceManagerImpl::CreateFrameNode(
    ProcessNodeImpl* process_node,
    PageNodeImpl* page_node,
    FrameNodeImpl* parent_frame_node,
    FrameNodeImpl* outer_document_for_fenced_frame,
    int render_frame_id,
    const blink::LocalFrameToken& frame_token,
    content::BrowsingInstanceId browsing_instance_id,
    content::SiteInstanceGroupId site_instance_group_id,
    bool is_current) {
  return CreateNodeImpl<FrameNodeImpl>(
      process_node, page_node, parent_frame_node,
      outer_document_for_fenced_frame, render_frame_id, frame_token,
      browsing_instance_id, site_instance_group_id, is_current);
}

// static
std::unique_ptr<PageNodeImpl> PerformanceManagerImpl::CreatePageNode(
    base::WeakPtr<content::WebContents> web_contents,
    const std::string& browser_context_id,
    const GURL& visible_url,
    PagePropertyFlags initial_property_flags,
    base::TimeTicks visibility_change_time) {
  return CreateNodeImpl<PageNodeImpl>(
      std::move(web_contents), browser_context_id, visible_url,
      initial_property_flags, visibility_change_time);
}

// static
std::unique_ptr<ProcessNodeImpl> PerformanceManagerImpl::CreateProcessNode(
    BrowserProcessNodeTag tag) {
  return CreateNodeImpl<ProcessNodeImpl>(tag);
}

// static
std::unique_ptr<ProcessNodeImpl> PerformanceManagerImpl::CreateProcessNode(
    RenderProcessHostProxy render_process_host_proxy,
    base::TaskPriority priority) {
  return CreateNodeImpl<ProcessNodeImpl>(std::move(render_process_host_proxy),
                                         priority);
}

// static
std::unique_ptr<ProcessNodeImpl> PerformanceManagerImpl::CreateProcessNode(
    content::ProcessType process_type,
    BrowserChildProcessHostProxy browser_child_process_host_proxy) {
  return CreateNodeImpl<ProcessNodeImpl>(
      process_type, std::move(browser_child_process_host_proxy));
}

// static
std::unique_ptr<WorkerNodeImpl> PerformanceManagerImpl::CreateWorkerNode(
    const std::string& browser_context_id,
    WorkerNode::WorkerType worker_type,
    ProcessNodeImpl* process_node,
    const blink::WorkerToken& worker_token,
    const url::Origin& origin) {
  return CreateNodeImpl<WorkerNodeImpl>(browser_context_id, worker_type,
                                        process_node, worker_token, origin);
}

// static
void PerformanceManagerImpl::DeleteNode(std::unique_ptr<NodeBase> node) {
  CHECK(IsAvailable());
  GetGraphImpl()->RemoveNode(node.get());
  // The node is deleted at the end of this function.
}

// static
void PerformanceManagerImpl::BatchDeleteNodes(
    std::vector<std::unique_ptr<NodeBase>> nodes) {
  GraphImpl* graph = GetGraphImpl();

  base::flat_set<ProcessNodeImpl*> process_nodes;

  for (const auto& node : nodes) {
    switch (node->GetNodeType()) {
      case PageNodeImpl::Type(): {
        auto* page_node = PageNodeImpl::FromNodeBase(node.get());

        // Delete the main frame nodes until no more exist.
        while (!page_node->main_frame_nodes().empty()) {
          RemoveFrameAndChildrenFromGraph(
              *(page_node->main_frame_nodes().begin()), graph);
        }

        graph->RemoveNode(page_node);
        break;
      }
      case ProcessNodeImpl::Type(): {
        // Keep track of the process nodes for removing once all frames nodes
        // are removed.
        auto* process_node = ProcessNodeImpl::FromNodeBase(node.get());
        process_nodes.insert(process_node);
        break;
      }
      case FrameNodeImpl::Type():
        break;
      case WorkerNodeImpl::Type(): {
        auto* worker_node = WorkerNodeImpl::FromNodeBase(node.get());
        graph->RemoveNode(worker_node);
        break;
      }
      case SystemNodeImpl::Type(): {
        NOTREACHED();
      }
    }
  }

  // Remove the process nodes from the graph.
  for (auto* process_node : process_nodes)
    graph->RemoveNode(process_node);

  // When |nodes| goes out of scope, all nodes are deleted.
}

PerformanceManagerImpl::PerformanceManagerImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  CHECK(!g_performance_manager);
  g_performance_manager = this;

  graph_.SetUp();
  graph_.set_ukm_recorder(ukm::UkmRecorder::Get());
}

// static
template <typename NodeType, typename... Args>
std::unique_ptr<NodeType> PerformanceManagerImpl::CreateNodeImpl(
    Args&&... constructor_args) {
  std::unique_ptr<NodeType> new_node =
      std::make_unique<NodeType>(std::forward<Args>(constructor_args)...);
  PerformanceManagerImpl::GetGraphImpl()->AddNewNode(new_node.get());
  return new_node;
}

}  // namespace performance_manager
