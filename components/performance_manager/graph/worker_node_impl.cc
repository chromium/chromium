// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/worker_node_impl.h"

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {

// static
constexpr char WorkerNodeImpl::kDefaultPriorityReason[] =
    "default worker priority";

using PriorityAndReason = execution_context_priority::PriorityAndReason;

WorkerNodeImpl::WorkerNodeImpl(const std::string& browser_context_id,
                               WorkerType worker_type,
                               ProcessNodeImpl* process_node,
                               const blink::WorkerToken& worker_token,
                               const url::Origin& origin)
    : browser_context_id_(browser_context_id),
      worker_type_(worker_type),
      process_node_(process_node),
      worker_token_(worker_token),
      origin_(origin) {
  // Nodes are created on the UI thread, then accessed on the PM sequence.
  // `weak_this_` can be returned from GetWeakPtrOnUIThread() and dereferenced
  // on the PM sequence.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DETACH_FROM_SEQUENCE(sequence_checker_);
  weak_this_ = weak_factory_.GetWeakPtr();

  DCHECK(process_node);
}

WorkerNodeImpl::~WorkerNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_frames_.empty());
  DCHECK(client_workers_.empty());
  DCHECK(child_workers_.empty());
}

WorkerNode::WorkerType WorkerNodeImpl::GetWorkerType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return worker_type_;
}

const std::string& WorkerNodeImpl::GetBrowserContextID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return browser_context_id_;
}

const blink::WorkerToken& WorkerNodeImpl::GetWorkerToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return worker_token_;
}

resource_attribution::WorkerContext WorkerNodeImpl::GetResourceContext() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return resource_attribution::WorkerContext::FromWorkerNode(this);
}

const GURL& WorkerNodeImpl::GetURL() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return url_;
}

const url::Origin& WorkerNodeImpl::GetOrigin() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return origin_;
}

const PriorityAndReason& WorkerNodeImpl::GetPriorityAndReason() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return priority_and_reason_.value();
}

uint64_t WorkerNodeImpl::GetResidentSetKbEstimate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return resident_set_kb_estimate_;
}

uint64_t WorkerNodeImpl::GetPrivateFootprintKbEstimate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return private_footprint_kb_estimate_;
}

void WorkerNodeImpl::AddClientFrame(FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : GetObservers()) {
    observer.OnBeforeClientFrameAdded(this, frame_node);
  }

  bool inserted = client_frames_.insert(frame_node).second;
  DCHECK(inserted);

  frame_node->AddChildWorker(this);

  for (auto& observer : GetObservers()) {
    observer.OnClientFrameAdded(this, frame_node);
  }
}

void WorkerNodeImpl::RemoveClientFrame(FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : GetObservers()) {
    observer.OnBeforeClientFrameRemoved(this, frame_node);
  }

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
      DCHECK_EQ(worker_node->GetWorkerType(), WorkerType::kDedicated);
      break;
    case WorkerType::kShared:
      // Nested shared workers are not available in Chrome.
      NOTREACHED_IN_MIGRATION();
      break;
    case WorkerType::kService:
      // A service worker may not control another service worker.
      DCHECK_NE(worker_node->GetWorkerType(), WorkerType::kService);
      break;
  }

  for (auto& observer : GetObservers()) {
    observer.OnBeforeClientWorkerAdded(this, worker_node);
  }

  bool inserted = client_workers_.insert(worker_node).second;
  DCHECK(inserted);

  worker_node->AddChildWorker(this);

  for (auto& observer : GetObservers()) {
    observer.OnClientWorkerAdded(this, worker_node);
  }
}

void WorkerNodeImpl::RemoveClientWorker(WorkerNodeImpl* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : GetObservers()) {
    observer.OnBeforeClientWorkerRemoved(this, worker_node);
  }

  worker_node->RemoveChildWorker(this);

  size_t removed = client_workers_.erase(worker_node);
  DCHECK_EQ(removed, 1u);
}

void WorkerNodeImpl::SetPriorityAndReason(
    const PriorityAndReason& priority_and_reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  priority_and_reason_.SetAndMaybeNotify(this, priority_and_reason);
}

void WorkerNodeImpl::SetResidentSetKbEstimate(uint64_t rss_estimate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  resident_set_kb_estimate_ = rss_estimate;
}

void WorkerNodeImpl::SetPrivateFootprintKbEstimate(uint64_t pmf_estimate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  private_footprint_kb_estimate_ = pmf_estimate;
}

void WorkerNodeImpl::OnFinalResponseURLDetermined(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url_.is_empty());
  url_ = url;

  for (auto& observer : GetObservers()) {
    observer.OnFinalResponseURLDetermined(this);
  }
}

ProcessNodeImpl* WorkerNodeImpl::process_node() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return process_node_;
}

WorkerNode::NodeSetView<FrameNodeImpl*> WorkerNodeImpl::client_frames() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return NodeSetView<FrameNodeImpl*>(client_frames_);
}

WorkerNode::NodeSetView<WorkerNodeImpl*> WorkerNodeImpl::client_workers()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return NodeSetView<WorkerNodeImpl*>(client_workers_);
}

WorkerNode::NodeSetView<WorkerNodeImpl*> WorkerNodeImpl::child_workers() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return NodeSetView<WorkerNodeImpl*>(child_workers_);
}

base::WeakPtr<WorkerNodeImpl> WorkerNodeImpl::GetWeakPtrOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return weak_this_;
}

base::WeakPtr<WorkerNodeImpl> WorkerNodeImpl::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void WorkerNodeImpl::OnJoiningGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Make sure all weak pointers, even `weak_this_` that was created on the UI
  // thread in the constructor, can only be dereferenced on the graph sequence.
  weak_factory_.BindToCurrentSequence(
      base::subtle::BindWeakPtrFactoryPassKey());

  NodeAttachedDataStorage::Create(this);
  execution_context::WorkerExecutionContext::Create(this, this);

  process_node_->AddWorker(this);
}

void WorkerNodeImpl::OnBeforeLeavingGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  process_node_->RemoveWorker(this);
}

void WorkerNodeImpl::RemoveNodeAttachedData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DestroyNodeInlineDataStorage();
}

const ProcessNode* WorkerNodeImpl::GetProcessNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return process_node();
}

WorkerNode::NodeSetView<const FrameNode*> WorkerNodeImpl::GetClientFrames()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return NodeSetView<const FrameNode*>(client_frames_);
}

WorkerNode::NodeSetView<const WorkerNode*> WorkerNodeImpl::GetClientWorkers()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return NodeSetView<const WorkerNode*>(client_workers_);
}

WorkerNode::NodeSetView<const WorkerNode*> WorkerNodeImpl::GetChildWorkers()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return NodeSetView<const WorkerNode*>(child_workers_);
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
