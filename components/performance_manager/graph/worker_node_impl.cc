// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/worker_node_impl.h"

#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"

namespace performance_manager {

WorkerNodeImpl::WorkerNodeImpl(GraphImpl* graph,
                               const std::string& browser_context_id,
                               WorkerType worker_type,
                               ProcessNodeImpl* process_node,
                               const GURL& url,
                               const base::UnguessableToken& dev_tools_token)
    : TypedNodeBase(graph),
      browser_context_id_(browser_context_id),
      worker_type_(worker_type),
      process_node_(process_node),
      url_(url),
      dev_tools_token_(dev_tools_token) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(process_node);
}

WorkerNodeImpl::~WorkerNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_frames_.empty());
  DCHECK(client_workers_.empty());
  DCHECK(child_workers_.empty());
}

void WorkerNodeImpl::AddClientFrame(FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool inserted = client_frames_.insert(frame_node).second;
  DCHECK(inserted);

  frame_node->AddChildWorker(this);

  for (auto* observer : GetObservers())
    observer->OnClientFrameAdded(this, frame_node);
}

void WorkerNodeImpl::RemoveClientFrame(FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto* observer : GetObservers())
    observer->OnBeforeClientFrameRemoved(this, frame_node);

  frame_node->RemoveChildWorker(this);

  size_t removed = client_frames_.erase(frame_node);
  DCHECK_EQ(1u, removed);
}

void WorkerNodeImpl::AddClientWorker(WorkerNodeImpl* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (worker_type_) {
    case WorkerType::kDedicated:
      // Nested dedicated workers are only available from other dedicated
      // workers in Chrome.
      DCHECK_EQ(worker_node->worker_type(), WorkerType::kDedicated);
      break;
    case WorkerType::kShared:
      // Nested shared workers are not available in Chrome.
      NOTREACHED();
      break;
    case WorkerType::kService:
      // A service worker may not control another service worker.
      DCHECK_NE(worker_node->worker_type(), WorkerType::kService);
      break;
  }

  bool inserted = client_workers_.insert(worker_node).second;
  DCHECK(inserted);

  worker_node->AddChildWorker(this);

  for (auto* observer : GetObservers())
    observer->OnClientWorkerAdded(this, worker_node);
}

void WorkerNodeImpl::RemoveClientWorker(WorkerNodeImpl* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto* observer : GetObservers())
    observer->OnBeforeClientWorkerRemoved(this, worker_node);

  worker_node->RemoveChildWorker(this);

  size_t removed = client_workers_.erase(worker_node);
  DCHECK_EQ(removed, 1u);
}

const std::string& WorkerNodeImpl::browser_context_id() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return browser_context_id_;
}

WorkerNode::WorkerType WorkerNodeImpl::worker_type() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return worker_type_;
}

ProcessNodeImpl* WorkerNodeImpl::process_node() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return process_node_;
}

const GURL& WorkerNodeImpl::url() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return url_;
}

const base::UnguessableToken& WorkerNodeImpl::dev_tools_token() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return dev_tools_token_;
}

const base::flat_set<FrameNodeImpl*>& WorkerNodeImpl::client_frames() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return client_frames_;
}

const base::flat_set<WorkerNodeImpl*>& WorkerNodeImpl::client_workers() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return client_workers_;
}

const base::flat_set<WorkerNodeImpl*>& WorkerNodeImpl::child_workers() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return child_workers_;
}

void WorkerNodeImpl::JoinGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  process_node_->AddWorker(this);

  NodeBase::JoinGraph();
}

void WorkerNodeImpl::LeaveGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NodeBase::LeaveGraph();

  process_node_->RemoveWorker(this);
}

WorkerNode::WorkerType WorkerNodeImpl::GetWorkerType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return worker_type();
}

const std::string& WorkerNodeImpl::GetBrowserContextID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return browser_context_id();
}

const ProcessNode* WorkerNodeImpl::GetProcessNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return process_node();
}

const GURL& WorkerNodeImpl::GetURL() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return url();
}

const base::UnguessableToken& WorkerNodeImpl::GetDevToolsToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return dev_tools_token();
}

const base::flat_set<const FrameNode*> WorkerNodeImpl::GetClientFrames() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::flat_set<const FrameNode*> client_frames;
  for (auto* client : client_frames_)
    client_frames.insert(static_cast<const FrameNode*>(client));
  DCHECK_EQ(client_frames.size(), client_frames_.size());
  return client_frames;
}

const base::flat_set<const WorkerNode*> WorkerNodeImpl::GetClientWorkers()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::flat_set<const WorkerNode*> client_workers;
  for (auto* client : client_workers_)
    client_workers.insert(static_cast<const WorkerNode*>(client));
  DCHECK_EQ(client_workers.size(), client_workers_.size());
  return client_workers;
}

const base::flat_set<const WorkerNode*> WorkerNodeImpl::GetChildWorkers()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::flat_set<const WorkerNode*> child_workers;
  for (auto* child : child_workers_)
    child_workers.insert(static_cast<const WorkerNode*>(child));
  DCHECK_EQ(child_workers.size(), child_workers_.size());
  return child_workers;
}

void WorkerNodeImpl::AddChildWorker(WorkerNodeImpl* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool inserted = child_workers_.insert(worker_node).second;
  DCHECK(inserted);
}

void WorkerNodeImpl::RemoveChildWorker(WorkerNodeImpl* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t removed = child_workers_.erase(worker_node);
  DCHECK_EQ(removed, 1u);
}

}  // namespace performance_manager
