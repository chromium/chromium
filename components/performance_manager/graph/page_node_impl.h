// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PAGE_NODE_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PAGE_NODE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/node_attached_data.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/web_contents_proxy.h"
#include "url/gurl.h"

namespace performance_manager {

class FrameNodeImpl;

class PageNodeImpl
    : public PublicNodeImpl<PageNodeImpl, PageNode>,
      public TypedNodeBase<PageNodeImpl, PageNode, PageNodeObserver> {
 public:
  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kPage; }

  PageNodeImpl(GraphImpl* graph,
               const WebContentsProxy& contents_proxy,
               const std::string& browser_context_id,
               const GURL& visible_url,
               bool is_visible,
               bool is_audible);
  ~PageNodeImpl() override;

  // Returns the web contents associated with this page node. It is valid to
  // call this function on any thread but the weak pointer must only be
  // dereferenced on the UI thread.
  const WebContentsProxy& contents_proxy() const;

  void SetIsVisible(bool is_visible);
  void SetIsAudible(bool is_audible);
  void SetIsLoading(bool is_loading);
  void SetUkmSourceId(ukm::SourceId ukm_source_id);
  void OnFaviconUpdated();
  void OnTitleUpdated();
  void OnMainFrameNavigationCommitted(bool same_document,
                                      base::TimeTicks navigation_committed_time,
                                      int64_t navigation_id,
                                      const GURL& url);

  // Returns the average CPU usage that can be attributed to this page over the
  // last measurement period. CPU usage is expressed as the average percentage
  // of cores occupied over the last measurement interval. One core fully
  // occupied would be 100, while two cores at 5% each would be 10.
  // TODO(chrisha): Make this 1.0 for 100%, and 0.1 for 10%.
  double GetCPUUsage() const;

  // Returns 0 if no navigation has happened, otherwise returns the time since
  // the last navigation commit.
  base::TimeDelta TimeSinceLastNavigation() const;

  // Returns the time since the last visibility change, it should always have a
  // value since we set the visibility property when we create a
  // page node.
  base::TimeDelta TimeSinceLastVisibilityChange() const;

  // Returns the current main frame node (if there is one), otherwise returns
  // any of the potentially multiple main frames that currently exist. If there
  // are no main frames at the moment, returns nullptr.
  FrameNodeImpl* GetMainFrameNodeImpl() const;

  // Accessors.
  const std::string& browser_context_id() const;
  bool is_visible() const;
  bool is_audible() const;
  bool is_loading() const;
  ukm::SourceId ukm_source_id() const;
  LifecycleState lifecycle_state() const;
  InterventionPolicy origin_trial_freeze_policy() const;
  bool is_holding_weblock() const;
  bool is_holding_indexeddb_lock() const;
  const base::flat_set<FrameNodeImpl*>& main_frame_nodes() const;
  base::TimeTicks usage_estimate_time() const;
  base::TimeDelta cumulative_cpu_usage_estimate() const;
  uint64_t private_footprint_kb_estimate() const;
  bool page_almost_idle() const;
  const GURL& main_frame_url() const;
  int64_t navigation_id() const;

  void set_usage_estimate_time(base::TimeTicks usage_estimate_time);
  void set_cumulative_cpu_usage_estimate(
      base::TimeDelta cumulative_cpu_usage_estimate);
  void set_private_footprint_kb_estimate(
      uint64_t private_footprint_kb_estimate);
  void set_has_nonempty_beforeunload(bool has_nonempty_beforeunload);

  void SetLifecycleStateForTesting(LifecycleState lifecycle_state) {
    SetLifecycleState(lifecycle_state);
  }

  void SetPageAlmostIdleForTesting(bool page_almost_idle) {
    SetPageAlmostIdle(page_almost_idle);
  }

  void SetIsHoldingWebLockForTesting(bool is_holding_weblock) {
    SetIsHoldingWebLock(is_holding_weblock);
  }

  base::WeakPtr<PageNodeImpl> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend class FrameNodeImpl;
  friend class PageAggregatorAccess;
  friend class FrozenFrameAggregatorAccess;
  friend class PageAlmostIdleAccess;

  // PageNode implementation:
  const std::string& GetBrowserContextID() const override;
  bool IsPageAlmostIdle() const override;
  bool IsVisible() const override;
  base::TimeDelta GetTimeSinceLastVisibilityChange() const override;
  bool IsAudible() const override;
  bool IsLoading() const override;
  ukm::SourceId GetUkmSourceID() const override;
  LifecycleState GetLifecycleState() const override;
  InterventionPolicy GetOriginTrialFreezePolicy() const override;
  bool IsHoldingWebLock() const override;
  bool IsHoldingIndexedDBLock() const override;
  int64_t GetNavigationID() const override;
  base::TimeDelta GetTimeSinceLastNavigation() const override;
  const FrameNode* GetMainFrameNode() const override;
  const base::flat_set<const FrameNode*> GetMainFrameNodes() const override;
  const GURL& GetMainFrameUrl() const override;
  const WebContentsProxy& GetContentsProxy() const override;

  void AddFrame(FrameNodeImpl* frame_node);
  void RemoveFrame(FrameNodeImpl* frame_node);
  void JoinGraph() override;
  void LeaveGraph() override;

  void SetPageAlmostIdle(bool page_almost_idle);
  void SetLifecycleState(LifecycleState lifecycle_state);
  void SetOriginTrialFreezePolicy(InterventionPolicy policy);
  void SetIsHoldingWebLock(bool is_holding_weblock);
  void SetIsHoldingIndexedDBLock(bool is_holding_indexeddb_lock);

  // The WebContentsProxy associated with this page.
  const WebContentsProxy contents_proxy_;

  // The main frame nodes of this page. There can be more than one main frame
  // in a page, among other reasons because during main frame navigation, the
  // pending navigation will coexist with the existing main frame until it's
  // committed.
  base::flat_set<FrameNodeImpl*> main_frame_nodes_;

  // The total count of frames that tally up to this page.
  size_t frame_node_count_ = 0;

  // The last time at which the page visibility changed.
  base::TimeTicks visibility_change_time_;

  // The last time at which a main frame navigation was committed.
  base::TimeTicks navigation_committed_time_;

  // The time the most recent resource usage estimate applies to.
  base::TimeTicks usage_estimate_time_;

  // The most current CPU usage estimate. Note that this estimate is most
  // generously described as "piecewise linear", as it attributes the CPU
  // cost incurred since the last measurement was made equally to pages
  // hosted by a process. If, e.g. a frame has come into existence and vanished
  // from a given process between measurements, the entire cost to that frame
  // will be mis-attributed to other frames hosted in that process.
  base::TimeDelta cumulative_cpu_usage_estimate_;

  // The most current memory footprint estimate.
  uint64_t private_footprint_kb_estimate_ = 0;

  // Indicates whether or not this page has a non-empty beforeunload handler.
  // This is an aggregation of the same value on each frame in the page's frame
  // tree. The aggregation is made at the moment all frames associated with a
  // page have transition to frozen.
  bool has_nonempty_beforeunload_ = false;

  // The URL the main frame last committed, or the initial URL a page was
  // initialized with. The latter case is distinguished by a zero navigation ID.
  ObservedProperty::
      NotifiesOnlyOnChanges<GURL, &PageNodeObserver::OnMainFrameUrlChanged>
          main_frame_url_;

  // The unique ID of the navigation handle the main frame last committed, or
  // zero if the page has never committed a navigation.
  int64_t navigation_id_ = 0;

  // The unique ID of the browser context that this page belongs to.
  const std::string browser_context_id_;

  // Page almost idle state. This is the output that is driven by the
  // PageAlmostIdleDecorator.
  ObservedProperty::
      NotifiesOnlyOnChanges<bool, &PageNodeObserver::OnPageAlmostIdleChanged>
          page_almost_idle_{false};
  // Whether or not the page is visible. Driven by browser instrumentation.
  // Initialized on construction.
  ObservedProperty::NotifiesOnlyOnChanges<bool,
                                          &PageNodeObserver::OnIsVisibleChanged>
      is_visible_{false};
  // Whether or not the page is audible. Driven by browser instrumentation.
  // Initialized on construction.
  ObservedProperty::NotifiesOnlyOnChanges<bool,
                                          &PageNodeObserver::OnIsAudibleChanged>
      is_audible_{false};
  // The loading state. This is driven by instrumentation in the browser
  // process.
  ObservedProperty::NotifiesOnlyOnChanges<bool,
                                          &PageNodeObserver::OnIsLoadingChanged>
      is_loading_{false};
  // The UKM source ID associated with the URL of the main frame of this page.
  ObservedProperty::NotifiesOnlyOnChanges<
      ukm::SourceId,
      &PageNodeObserver::OnUkmSourceIdChanged>
      ukm_source_id_{ukm::kInvalidSourceId};
  // The lifecycle state of this page. This is aggregated from the lifecycle
  // state of each frame in the frame tree.
  ObservedProperty::NotifiesOnlyOnChanges<
      LifecycleState,
      &PageNodeObserver::OnPageLifecycleStateChanged>
      lifecycle_state_{LifecycleState::kRunning};
  // The origin trial freeze policy of this page. This is aggregated from the
  // origin trial freeze policy of each current frame in the frame tree.
  ObservedProperty::NotifiesOnlyOnChanges<
      InterventionPolicy,
      &PageNodeObserver::OnPageOriginTrialFreezePolicyChanged>
      origin_trial_freeze_policy_{InterventionPolicy::kDefault};
  // Indicates if at least one frame of the page is currently holding a WebLock.
  ObservedProperty::NotifiesOnlyOnChanges<
      bool,
      &PageNodeObserver::OnPageIsHoldingWebLockChanged>
      is_holding_weblock_{false};
  // Indicates if at least one frame of the page is currently holding an
  // IndexedDB lock.
  ObservedProperty::NotifiesOnlyOnChanges<
      bool,
      &PageNodeObserver::OnPageIsHoldingIndexedDBLockChanged>
      is_holding_indexeddb_lock_{false};

  // Storage for PageAlmostIdle user data.
  std::unique_ptr<NodeAttachedData> page_almost_idle_data_;

  // Inline storage for FrozenFrameAggregator user data.
  InternalNodeAttachedDataStorage<sizeof(uintptr_t) + 8> frozen_frame_data_;

  // Inline storage for PageAggregatorAccess user data.
  InternalNodeAttachedDataStorage<sizeof(uintptr_t) + 24> page_aggregator_data_;

  base::WeakPtrFactory<PageNodeImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PageNodeImpl);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PAGE_NODE_IMPL_H_
