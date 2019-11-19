// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/page_node_impl.h"

#include <memory>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/time/default_tick_clock.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/graph_impl_operations.h"
#include "components/performance_manager/graph/process_node_impl.h"

namespace performance_manager {

PageNodeImpl::PageNodeImpl(GraphImpl* graph,
                           const WebContentsProxy& contents_proxy,
                           const std::string& browser_context_id,
                           const GURL& visible_url,
                           bool is_visible,
                           bool is_audible)
    : TypedNodeBase(graph),
      contents_proxy_(contents_proxy),
      visibility_change_time_(base::TimeTicks::Now()),
      main_frame_url_(visible_url),
      browser_context_id_(browser_context_id),
      is_visible_(is_visible),
      is_audible_(is_audible) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PageNodeImpl::~PageNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

const WebContentsProxy& PageNodeImpl::contents_proxy() const {
  return contents_proxy_;
}

void PageNodeImpl::AddFrame(FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_node);
  DCHECK_EQ(this, frame_node->page_node());
  DCHECK(graph()->NodeInGraph(frame_node));

  ++frame_node_count_;
  if (frame_node->parent_frame_node() == nullptr)
    main_frame_nodes_.insert(frame_node);
}

void PageNodeImpl::RemoveFrame(FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_node);
  DCHECK_EQ(this, frame_node->page_node());
  DCHECK(graph()->NodeInGraph(frame_node));

  --frame_node_count_;
  if (frame_node->parent_frame_node() == nullptr) {
    size_t removed = main_frame_nodes_.erase(frame_node);
    DCHECK_EQ(1u, removed);
  }
}

void PageNodeImpl::SetIsLoading(bool is_loading) {
  is_loading_.SetAndMaybeNotify(this, is_loading);
}

void PageNodeImpl::SetIsVisible(bool is_visible) {
  if (is_visible_.SetAndMaybeNotify(this, is_visible)) {
    // The change time needs to be updated after observers are notified, as they
    // use this to determine time passed since the *previous* visibility state
    // change. They can infer the current state change time themselves via
    // NowTicks.
    visibility_change_time_ = base::TimeTicks::Now();
  }
}

void PageNodeImpl::SetIsAudible(bool is_audible) {
  is_audible_.SetAndMaybeNotify(this, is_audible);
}

void PageNodeImpl::SetUkmSourceId(ukm::SourceId ukm_source_id) {
  ukm_source_id_.SetAndMaybeNotify(this, ukm_source_id);
}

void PageNodeImpl::OnFaviconUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto* observer : GetObservers())
    observer->OnFaviconUpdated(this);
}

void PageNodeImpl::OnTitleUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto* observer : GetObservers())
    observer->OnTitleUpdated(this);
}

void PageNodeImpl::OnMainFrameNavigationCommitted(
    bool same_document,
    base::TimeTicks navigation_committed_time,
    int64_t navigation_id,
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This should never be invoked with a null navigation, nor should it be
  // called twice for the same navigation.
  DCHECK_NE(0, navigation_id);
  DCHECK_NE(navigation_id_, navigation_id);
  navigation_committed_time_ = navigation_committed_time;
  navigation_id_ = navigation_id;
  main_frame_url_.SetAndMaybeNotify(this, url);

  // No mainframe document change notification on same-document navigations.
  if (same_document)
    return;

  for (auto* observer : GetObservers())
    observer->OnMainFrameDocumentChanged(this);
}

double PageNodeImpl::GetCPUUsage() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  double cpu_usage = 0;

  // TODO(chrisha/siggi): This should all be ripped out / refactored.
  for (auto* process_node :
       GraphImplOperations::GetAssociatedProcessNodes(this)) {
    size_t pages_in_process =
        GraphImplOperations::GetAssociatedPageNodes(process_node).size();
    DCHECK_LE(1u, pages_in_process);
    cpu_usage += process_node->cpu_usage() / pages_in_process;
  }

  return cpu_usage;
}

base::TimeDelta PageNodeImpl::TimeSinceLastNavigation() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (navigation_committed_time_.is_null())
    return base::TimeDelta();
  return base::TimeTicks::Now() - navigation_committed_time_;
}

base::TimeDelta PageNodeImpl::TimeSinceLastVisibilityChange() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::TimeTicks::Now() - visibility_change_time_;
}

FrameNodeImpl* PageNodeImpl::GetMainFrameNodeImpl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (main_frame_nodes_.empty())
    return nullptr;

  // Return the current frame node if there is one. Iterating over this set is
  // fine because it is almost always of length 1 or 2.
  for (auto* frame : main_frame_nodes_) {
    if (frame->is_current())
      return frame;
  }

  // Otherwise, return any old main frame node.
  return *main_frame_nodes_.begin();
}

bool PageNodeImpl::is_visible() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_visible_.value();
}

bool PageNodeImpl::is_audible() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_audible_.value();
}

bool PageNodeImpl::is_loading() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_loading_.value();
}

ukm::SourceId PageNodeImpl::ukm_source_id() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ukm_source_id_.value();
}

PageNodeImpl::LifecycleState PageNodeImpl::lifecycle_state() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return lifecycle_state_.value();
}

PageNodeImpl::InterventionPolicy PageNodeImpl::origin_trial_freeze_policy()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return origin_trial_freeze_policy_.value();
}

bool PageNodeImpl::is_holding_weblock() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_holding_weblock_.value();
}

bool PageNodeImpl::is_holding_indexeddb_lock() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_holding_indexeddb_lock_.value();
}

const base::flat_set<FrameNodeImpl*>& PageNodeImpl::main_frame_nodes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return main_frame_nodes_;
}

base::TimeTicks PageNodeImpl::usage_estimate_time() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return usage_estimate_time_;
}

base::TimeDelta PageNodeImpl::cumulative_cpu_usage_estimate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cumulative_cpu_usage_estimate_;
}

uint64_t PageNodeImpl::private_footprint_kb_estimate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return private_footprint_kb_estimate_;
}

const std::string& PageNodeImpl::browser_context_id() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return browser_context_id_;
}

bool PageNodeImpl::page_almost_idle() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return page_almost_idle_.value();
}

const GURL& PageNodeImpl::main_frame_url() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return main_frame_url_.value();
}

int64_t PageNodeImpl::navigation_id() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return navigation_id_;
}

void PageNodeImpl::set_usage_estimate_time(
    base::TimeTicks usage_estimate_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  usage_estimate_time_ = usage_estimate_time;
}

void PageNodeImpl::set_cumulative_cpu_usage_estimate(
    base::TimeDelta cumulative_cpu_usage_estimate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cumulative_cpu_usage_estimate_ = cumulative_cpu_usage_estimate;
}

void PageNodeImpl::set_private_footprint_kb_estimate(
    uint64_t private_footprint_kb_estimate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  private_footprint_kb_estimate_ = private_footprint_kb_estimate;
}

void PageNodeImpl::set_has_nonempty_beforeunload(
    bool has_nonempty_beforeunload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_nonempty_beforeunload_ = has_nonempty_beforeunload;
}

void PageNodeImpl::JoinGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  // Dereferencing the WeakPtr associated with this node will bind it to the
  // current sequence (all subsequent calls to |GetWeakPtr| will return the
  // same WeakPtr).
  GetWeakPtr()->GetImpl();
#endif

  NodeBase::JoinGraph();
}

void PageNodeImpl::LeaveGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_EQ(0u, frame_node_count_);

  NodeBase::LeaveGraph();
}

const std::string& PageNodeImpl::GetBrowserContextID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return browser_context_id();
}

bool PageNodeImpl::IsPageAlmostIdle() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return page_almost_idle();
}

bool PageNodeImpl::IsVisible() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_visible();
}

base::TimeDelta PageNodeImpl::GetTimeSinceLastVisibilityChange() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return TimeSinceLastVisibilityChange();
}

bool PageNodeImpl::IsAudible() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_audible();
}

bool PageNodeImpl::IsLoading() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_loading();
}

ukm::SourceId PageNodeImpl::GetUkmSourceID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ukm_source_id();
}

PageNodeImpl::LifecycleState PageNodeImpl::GetLifecycleState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return lifecycle_state();
}

PageNodeImpl::InterventionPolicy PageNodeImpl::GetOriginTrialFreezePolicy()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return origin_trial_freeze_policy();
}

bool PageNodeImpl::IsHoldingWebLock() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_holding_weblock();
}

bool PageNodeImpl::IsHoldingIndexedDBLock() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_holding_indexeddb_lock();
}

int64_t PageNodeImpl::GetNavigationID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return navigation_id();
}

base::TimeDelta PageNodeImpl::GetTimeSinceLastNavigation() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return TimeSinceLastNavigation();
}

const FrameNode* PageNodeImpl::GetMainFrameNode() const {
  return GetMainFrameNodeImpl();
}

const base::flat_set<const FrameNode*> PageNodeImpl::GetMainFrameNodes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::flat_set<const FrameNode*> main_frame_nodes(main_frame_nodes_.begin(),
                                                    main_frame_nodes_.end());
  return main_frame_nodes;
}

const WebContentsProxy& PageNodeImpl::GetContentsProxy() const {
  return contents_proxy();
}

const GURL& PageNodeImpl::GetMainFrameUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return main_frame_url();
}

void PageNodeImpl::SetPageAlmostIdle(bool page_almost_idle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  page_almost_idle_.SetAndMaybeNotify(this, page_almost_idle);
}

void PageNodeImpl::SetLifecycleState(LifecycleState lifecycle_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  lifecycle_state_.SetAndMaybeNotify(this, lifecycle_state);
}

void PageNodeImpl::SetOriginTrialFreezePolicy(InterventionPolicy policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  origin_trial_freeze_policy_.SetAndMaybeNotify(this, policy);
}

void PageNodeImpl::SetIsHoldingWebLock(bool is_holding_weblock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_holding_weblock_.SetAndMaybeNotify(this, is_holding_weblock);
}

void PageNodeImpl::SetIsHoldingIndexedDBLock(bool is_holding_indexeddb_lock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_holding_indexeddb_lock_.SetAndMaybeNotify(this, is_holding_indexeddb_lock);
}

}  // namespace performance_manager
