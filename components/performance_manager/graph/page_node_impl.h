// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PAGE_NODE_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PAGE_NODE_IMPL_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/graph/node_attached_data.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/web_contents_proxy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace performance_manager {

class FrameNodeImpl;
class FrozenFrameAggregatorAccess;
class PageAggregatorAccess;
class PageLoadTrackerAccess;
class SiteDataAccess;

class PageNodeImpl
    : public PublicNodeImpl<PageNodeImpl, PageNode>,
      public TypedNodeBase<PageNodeImpl, PageNode, PageNodeObserver> {
 public:
  using PassKey = base::PassKey<PageNodeImpl>;
  using FrozenFrameDataStorage =
      InternalNodeAttachedDataStorage<sizeof(uintptr_t) + 8>;
  using PageAggregatorDataStorage =
      InternalNodeAttachedDataStorage<sizeof(uintptr_t) + 16>;

  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kPage; }

  PageNodeImpl(const WebContentsProxy& contents_proxy,
               const std::string& browser_context_id,
               const GURL& visible_url,
               bool is_visible,
               bool is_audible,
               base::TimeTicks visibility_change_time,
               PageState page_state);

  PageNodeImpl(const PageNodeImpl&) = delete;
  PageNodeImpl& operator=(const PageNodeImpl&) = delete;

  ~PageNodeImpl() override;

  // Returns the web contents associated with this page node. It is valid to
  // call this function on any thread but the weak pointer must only be
  // dereferenced on the UI thread.
  const WebContentsProxy& contents_proxy() const;

  void SetType(PageType type);
  void SetIsVisible(bool is_visible);
  void SetIsAudible(bool is_audible);
  void SetLoadingState(LoadingState loading_state);
  void SetUkmSourceId(ukm::SourceId ukm_source_id);
  void OnFaviconUpdated();
  void OnTitleUpdated();
  void OnAboutToBeDiscarded(base::WeakPtr<PageNode> new_page_node);
  void OnMainFrameNavigationCommitted(bool same_document,
                                      base::TimeTicks navigation_committed_time,
                                      int64_t navigation_id,
                                      const GURL& url,
                                      const std::string& contents_mime_type);

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
  FrameNodeImpl* opener_frame_node() const;
  FrameNodeImpl* embedder_frame_node() const;
  EmbeddingType embedding_type() const;
  PageType type() const;
  bool is_visible() const;
  bool is_audible() const;
  LoadingState loading_state() const;
  ukm::SourceId ukm_source_id() const;
  LifecycleState lifecycle_state() const;
  bool is_holding_weblock() const;
  bool is_holding_indexeddb_lock() const;
  const base::flat_set<FrameNodeImpl*>& main_frame_nodes() const;
  base::TimeTicks usage_estimate_time() const;
  uint64_t private_footprint_kb_estimate() const;
  const GURL& main_frame_url() const;
  int64_t navigation_id() const;
  const std::string& contents_mime_type() const;
  bool had_form_interaction() const;
  bool had_user_edits() const;
  const absl::optional<freezing::FreezingVote>& freezing_vote() const;
  PageState page_state() const;

  // Invoked to set/clear the opener of this page.
  void SetOpenerFrameNode(FrameNodeImpl* opener);
  void ClearOpenerFrameNode();

  // Invoked to set/clear the embedder of this page.
  void SetEmbedderFrameNodeAndEmbeddingType(FrameNodeImpl* embedder,
                                            EmbeddingType embedder_type);
  void ClearEmbedderFrameNodeAndEmbeddingType();

  void set_usage_estimate_time(base::TimeTicks usage_estimate_time);
  void set_private_footprint_kb_estimate(
      uint64_t private_footprint_kb_estimate);
  void set_has_nonempty_beforeunload(bool has_nonempty_beforeunload);
  void set_freezing_vote(absl::optional<freezing::FreezingVote> freezing_vote);
  void set_page_state(PageState page_state);

  void SetLifecycleStateForTesting(LifecycleState lifecycle_state) {
    SetLifecycleState(lifecycle_state);
  }

  void SetIsHoldingWebLockForTesting(bool is_holding_weblock) {
    SetIsHoldingWebLock(is_holding_weblock);
  }

  void SetIsHoldingIndexedDBLockForTesting(bool is_holding_weblock) {
    SetIsHoldingIndexedDBLock(is_holding_weblock);
  }

  void SetHadFormInteractionForTesting(bool had_form_interaction) {
    SetHadFormInteraction(had_form_interaction);
  }

  void SetHadUserEditsForTesting(bool had_user_edits) {
    SetHadUserEdits(had_user_edits);
  }

  base::WeakPtr<PageNodeImpl> GetWeakPtrOnUIThread() {
    // TODO(siggi): Validate thread context.
    return weak_this_;
  }

  base::WeakPtr<PageNodeImpl> GetWeakPtr() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return weak_factory_.GetWeakPtr();
  }

  // Accessors to some of the NodeAttachedData:
  std::unique_ptr<NodeAttachedData>& GetSiteData(
      base::PassKey<SiteDataAccess>) {
    return site_data_;
  }
  std::unique_ptr<NodeAttachedData>& GetPageLoadTrackerData(
      base::PassKey<PageLoadTrackerAccess>) {
    return page_load_tracker_data_;
  }
  FrozenFrameDataStorage& GetFrozenFrameData(
      base::PassKey<FrozenFrameAggregatorAccess>) {
    return frozen_frame_data_;
  }
  PageAggregatorDataStorage& GetPageAggregatorData(
      base::PassKey<PageAggregatorAccess>) {
    return page_aggregator_data_;
  }

  // Functions meant to be called by a FrameNodeImpl:
  void AddFrame(base::PassKey<FrameNodeImpl>, FrameNodeImpl* frame_node);
  void RemoveFrame(base::PassKey<FrameNodeImpl>, FrameNodeImpl* frame_node);

  // Function meant to be called by FrozenFrameAggregatorAccess.
  void SetLifecycleState(base::PassKey<FrozenFrameAggregatorAccess>,
                         LifecycleState lifecycle_state) {
    SetLifecycleState(lifecycle_state);
  }

  // Functions meant to be called by PageAggregatorAccess:
  void SetIsHoldingWebLock(base::PassKey<PageAggregatorAccess>,
                           bool is_holding_weblock) {
    SetIsHoldingWebLock(is_holding_weblock);
  }
  void SetIsHoldingIndexedDBLock(base::PassKey<PageAggregatorAccess>,
                                 bool is_holding_indexeddb_lock) {
    SetIsHoldingIndexedDBLock(is_holding_indexeddb_lock);
  }
  void SetHadFormInteraction(base::PassKey<PageAggregatorAccess>,
                             bool had_form_interaction) {
    SetHadFormInteraction(had_form_interaction);
  }

  void SetHadUserEdits(base::PassKey<PageAggregatorAccess>,
                       bool had_user_edits) {
    SetHadUserEdits(had_user_edits);
  }

 private:
  friend class PageNodeImplDescriber;

  // PageNode implementation.
  PageState GetPageState() const override;
  const std::string& GetBrowserContextID() const override;
  const FrameNode* GetOpenerFrameNode() const override;
  const FrameNode* GetEmbedderFrameNode() const override;
  EmbeddingType GetEmbeddingType() const override;
  PageType GetType() const override;
  bool IsVisible() const override;
  base::TimeDelta GetTimeSinceLastVisibilityChange() const override;
  bool IsAudible() const override;
  LoadingState GetLoadingState() const override;
  ukm::SourceId GetUkmSourceID() const override;
  LifecycleState GetLifecycleState() const override;
  bool IsHoldingWebLock() const override;
  bool IsHoldingIndexedDBLock() const override;
  int64_t GetNavigationID() const override;
  const std::string& GetContentsMimeType() const override;
  base::TimeDelta GetTimeSinceLastNavigation() const override;
  const FrameNode* GetMainFrameNode() const override;
  bool VisitMainFrameNodes(const FrameNodeVisitor& visitor) const override;
  const base::flat_set<const FrameNode*> GetMainFrameNodes() const override;
  const GURL& GetMainFrameUrl() const override;
  bool HadFormInteraction() const override;
  bool HadUserEdits() const override;
  const WebContentsProxy& GetContentsProxy() const override;
  const absl::optional<freezing::FreezingVote>& GetFreezingVote()
      const override;
  uint64_t EstimateResidentSetSize() const override;
  uint64_t EstimatePrivateFootprintSize() const override;

  // NodeBase:
  void OnJoiningGraph() override;
  void OnBeforeLeavingGraph() override;
  void RemoveNodeAttachedData() override;

  void SetLifecycleState(LifecycleState lifecycle_state);
  void SetIsHoldingWebLock(bool is_holding_weblock);
  void SetIsHoldingIndexedDBLock(bool is_holding_indexeddb_lock);
  void SetHadFormInteraction(bool had_form_interaction);
  void SetHadUserEdits(bool had_user_edits);

  // The WebContentsProxy associated with this page.
  const WebContentsProxy contents_proxy_;

  // The main frame nodes of this page. There can be more than one main frame
  // in a page, among other reasons because during main frame navigation, the
  // pending navigation will coexist with the existing main frame until it's
  // committed.
  base::flat_set<FrameNodeImpl*> main_frame_nodes_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The total count of frames that tally up to this page.
  size_t frame_node_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // The last time at which the page visibility changed.
  base::TimeTicks visibility_change_time_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The last time at which a main frame navigation was committed.
  base::TimeTicks navigation_committed_time_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The time the most recent resource usage estimate applies to.
  base::TimeTicks usage_estimate_time_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The most current memory footprint estimate.
  uint64_t private_footprint_kb_estimate_
      GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // Indicates whether or not this page has a non-empty beforeunload handler.
  // This is an aggregation of the same value on each frame in the page's frame
  // tree. The aggregation is made at the moment all frames associated with a
  // page have transition to frozen.
  bool has_nonempty_beforeunload_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // The URL the main frame last committed, or the initial URL a page was
  // initialized with. The latter case is distinguished by a zero navigation ID.
  ObservedProperty::
      NotifiesOnlyOnChanges<GURL, &PageNodeObserver::OnMainFrameUrlChanged>
          main_frame_url_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The unique ID of the navigation handle the main frame last committed, or
  // zero if the page has never committed a navigation.
  int64_t navigation_id_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // The MIME type of the content associated with the last committed navigation
  // event for the main frame of this page or an empty string if the page has
  // never committed a navigation
  std::string contents_mime_type_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The unique ID of the browser context that this page belongs to.
  const std::string browser_context_id_;

  // The opener of this page, if there is one.
  raw_ptr<FrameNodeImpl> opener_frame_node_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  // The embedder of this page, if there is one.
  raw_ptr<FrameNodeImpl> embedder_frame_node_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  // The way in which this page was embedded, if it was embedded.
  EmbeddingType embedding_type_ GUARDED_BY_CONTEXT(sequence_checker_) =
      EmbeddingType::kInvalid;

  // The type of the page.
  ObservedProperty::NotifiesOnlyOnChangesWithPreviousValue<
      PageType,
      PageType,
      &PageNodeObserver::OnTypeChanged>
      type_ GUARDED_BY_CONTEXT(sequence_checker_){PageType::kUnknown};

  // Whether or not the page is visible. Driven by browser instrumentation.
  // Initialized on construction.
  ObservedProperty::NotifiesOnlyOnChanges<bool,
                                          &PageNodeObserver::OnIsVisibleChanged>
      is_visible_ GUARDED_BY_CONTEXT(sequence_checker_){false};
  // Whether or not the page is audible. Driven by browser instrumentation.
  // Initialized on construction.
  ObservedProperty::NotifiesOnlyOnChanges<bool,
                                          &PageNodeObserver::OnIsAudibleChanged>
      is_audible_ GUARDED_BY_CONTEXT(sequence_checker_){false};
  // The loading state. This is driven by instrumentation in the browser
  // process.
  ObservedProperty::NotifiesOnlyOnChangesWithPreviousValue<
      LoadingState,
      LoadingState,
      &PageNodeObserver::OnLoadingStateChanged>
      loading_state_ GUARDED_BY_CONTEXT(sequence_checker_){
          LoadingState::kLoadingNotStarted};
  // The UKM source ID associated with the URL of the main frame of this page.
  ObservedProperty::NotifiesOnlyOnChanges<
      ukm::SourceId,
      &PageNodeObserver::OnUkmSourceIdChanged>
      ukm_source_id_ GUARDED_BY_CONTEXT(sequence_checker_){
          ukm::kInvalidSourceId};
  // The lifecycle state of this page. This is aggregated from the lifecycle
  // state of each frame in the frame tree.
  ObservedProperty::NotifiesOnlyOnChanges<
      LifecycleState,
      &PageNodeObserver::OnPageLifecycleStateChanged>
      lifecycle_state_ GUARDED_BY_CONTEXT(sequence_checker_){
          LifecycleState::kRunning};
  // Indicates if at least one frame of the page is currently holding a WebLock.
  ObservedProperty::NotifiesOnlyOnChanges<
      bool,
      &PageNodeObserver::OnPageIsHoldingWebLockChanged>
      is_holding_weblock_ GUARDED_BY_CONTEXT(sequence_checker_){false};
  // Indicates if at least one frame of the page is currently holding an
  // IndexedDB lock.
  ObservedProperty::NotifiesOnlyOnChanges<
      bool,
      &PageNodeObserver::OnPageIsHoldingIndexedDBLockChanged>
      is_holding_indexeddb_lock_ GUARDED_BY_CONTEXT(sequence_checker_){false};
  // Indicates if at least one frame of the page has received some form
  // interactions.
  ObservedProperty::NotifiesOnlyOnChanges<
      bool,
      &PageNodeObserver::OnHadFormInteractionChanged>
      had_form_interaction_ GUARDED_BY_CONTEXT(sequence_checker_){false};
  // Indicates if at least one frame of the page has received some
  // user-initiated edits.
  ObservedProperty::
      NotifiesOnlyOnChanges<bool, &PageNodeObserver::OnHadUserEditsChanged>
          had_user_edits_ GUARDED_BY_CONTEXT(sequence_checker_){false};
  // The freezing vote associated with this page, see the comment of to
  // Page::GetFreezingVote for a description of the different values this can
  // take.
  ObservedProperty::NotifiesOnlyOnChangesWithPreviousValue<
      absl::optional<freezing::FreezingVote>,
      absl::optional<freezing::FreezingVote>,
      &PageNodeObserver::OnFreezingVoteChanged>
      freezing_vote_ GUARDED_BY_CONTEXT(sequence_checker_);
  // The state of this page.
  ObservedProperty::NotifiesOnlyOnChangesWithPreviousValue<
      PageState,
      PageState,
      &PageNodeObserver::OnPageStateChanged>
      page_state_ GUARDED_BY_CONTEXT(sequence_checker_){PageState::kActive};

  // Storage for PageLoadTracker user data.
  std::unique_ptr<NodeAttachedData> page_load_tracker_data_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Storage for SiteDataNodeData user data.
  std::unique_ptr<NodeAttachedData> site_data_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Inline storage for FrozenFrameAggregator user data.
  FrozenFrameDataStorage frozen_frame_data_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Inline storage for PageAggregatorAccess user data.
  PageAggregatorDataStorage page_aggregator_data_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtr<PageNodeImpl> weak_this_;
  base::WeakPtrFactory<PageNodeImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PAGE_NODE_IMPL_H_
