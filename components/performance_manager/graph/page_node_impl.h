// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PAGE_NODE_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PAGE_NODE_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/enum_set.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "base/types/token_type.h"
#include "build/build_config.h"
#include "components/performance_manager/decorators/page_aggregator_data.h"
#include "components/performance_manager/decorators/page_load_tracker_decorator_data.h"
#include "components/performance_manager/freezing/frozen_data.h"
#include "components/performance_manager/graph/node_attached_data_storage.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/scenarios/loading_scenario_data.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/performance_manager/decorators/site_data_node_data.h"
#endif

namespace performance_manager {

class FrameNodeImpl;
class FrozenFrameAggregator;

// The starting state of various boolean properties of the PageNode.
enum class PagePropertyFlag {
  kIsVisible,  // initializes PageNode::IsVisible()
  kMin = kIsVisible,
  kIsAudible,            // initializes PageNode::IsAudible()
  kHasPictureInPicture,  // initializes PageNode::HasPictureInPicture()
  kIsOffTheRecord,       // initializes PageNode::IsOffTheRecord()
  kMax = kIsOffTheRecord,
};
using PagePropertyFlags = base::
    EnumSet<PagePropertyFlag, PagePropertyFlag::kMin, PagePropertyFlag::kMax>;

class PageNodeImpl
    : public PublicNodeImpl<PageNodeImpl, PageNode>,
      public TypedNodeBase<PageNodeImpl, PageNode, PageNodeObserver>,
      public SupportsNodeInlineData<PageLoadTrackerDecoratorData,
                                    PageAggregatorData,
#if !BUILDFLAG(IS_ANDROID)
                                    SiteDataNodeData,
#endif
                                    FrozenData,
                                    LoadingScenarioPageFrameCounts,
                                    // Keep this last to avoid merge conflicts.
                                    NodeAttachedDataStorage> {
 public:
  using PassKey = base::PassKey<PageNodeImpl>;

  // A unique token to identify the PageNode and its associated WebContents for
  // the lifetime of the browser. Most node types use an existing unique
  // identifier for this (eg. FrameNode uses content::GlobalRenderFrameHostId,
  // WorkerNode uses blink::WorkerToken) but WebContents has no id to use.
  using PageToken = base::TokenType<class PageTokenTag>;

  using TypedNodeBase<PageNodeImpl, PageNode, PageNodeObserver>::FromNode;

  PageNodeImpl(base::WeakPtr<content::WebContents> web_contents,
               const std::string& browser_context_id,
               const GURL& visible_url,
               PagePropertyFlags initial_properties,
               base::TimeTicks visibility_change_time);

  PageNodeImpl(const PageNodeImpl&) = delete;
  PageNodeImpl& operator=(const PageNodeImpl&) = delete;

  ~PageNodeImpl() override;

  // Partial PageNode implementation:
  const std::string& GetBrowserContextID() const override;
  resource_attribution::PageContext GetResourceContext() const override;
  EmbeddingType GetEmbeddingType() const override;
  PageType GetType() const override;
  bool IsFocused() const override;
  bool IsVisible() const override;
  base::TimeDelta GetTimeSinceLastVisibilityChange() const override;
  bool IsAudible() const override;
  std::optional<base::TimeDelta> GetTimeSinceLastAudibleChange() const override;
  bool HasPictureInPicture() const override;
  bool IsOffTheRecord() const override;
  LoadingState GetLoadingState() const override;
  ukm::SourceId GetUkmSourceID() const override;
  LifecycleState GetLifecycleState() const override;
  bool IsHoldingWebLock() const override;
  bool IsHoldingIndexedDBLock() const override;
  bool UsesWebRTC() const override;
  int64_t GetNavigationID() const override;
  const std::string& GetContentsMimeType() const override;
  std::optional<blink::mojom::PermissionStatus>
  GetNotificationPermissionStatus() const override;
  base::TimeDelta GetTimeSinceLastNavigation() const override;
  const GURL& GetMainFrameUrl() const override;
  uint64_t EstimateMainFramePrivateFootprintSize() const override;
  bool HadFormInteraction() const override;
  bool HadUserEdits() const override;
  base::WeakPtr<content::WebContents> GetWebContents() const override;
  uint64_t EstimateResidentSetSize() const override;
  uint64_t EstimatePrivateFootprintSize() const override;

  // Returns the unique token for the page node. This function can be called
  // from any thread.
  const PageToken& page_token() const { return page_token_; }

  void SetType(PageType type);
  void SetIsFocused(bool is_focused);
  void SetIsVisible(bool is_visible);
  void SetIsAudible(bool is_audible);
  void SetHasPictureInPicture(bool has_picture_in_picture);
  void SetLoadingState(LoadingState loading_state);
  void SetUkmSourceId(ukm::SourceId ukm_source_id);
  void OnFaviconUpdated();
  void OnTitleUpdated();
  void OnAboutToBeDiscarded(base::WeakPtr<PageNode> new_page_node);
  // Set main frame information of a restored page before the first navigation
  // is committed.
  void SetMainFrameRestoredState(
      const GURL& url,
      blink::mojom::PermissionStatus notification_permission_status);
  // Invoked when a main frame navigation is committed.
  void OnMainFrameNavigationCommitted(
      bool same_document,
      base::TimeTicks navigation_committed_time,
      int64_t navigation_id,
      const GURL& url,
      const std::string& contents_mime_type,
      std::optional<blink::mojom::PermissionStatus>
          notification_permission_status);
  // While notification permission status is most often updated on main frame
  // navigation, it can also be updated independently from main frame navigation
  // when the user grants/revokes the permission.
  void OnNotificationPermissionStatusChange(
      blink::mojom::PermissionStatus permission_status);

  // Accessors.
  FrameNodeImpl* opener_frame_node() const;
  FrameNodeImpl* embedder_frame_node() const;
  FrameNodeImpl* main_frame_node() const;
  NodeSetView<FrameNodeImpl*> main_frame_nodes() const;

  // Invoked to set/clear the opener of this page.
  void SetOpenerFrameNode(FrameNodeImpl* opener);
  void ClearOpenerFrameNode();

  // Invoked to set/clear the embedder of this page.
  void SetEmbedderFrameNodeAndEmbeddingType(FrameNodeImpl* embedder,
                                            EmbeddingType embedder_type);
  void ClearEmbedderFrameNodeAndEmbeddingType();

  void set_has_nonempty_beforeunload(bool has_nonempty_beforeunload);

  void SetLifecycleStateForTesting(LifecycleState lifecycle_state) {
    SetLifecycleState(lifecycle_state);
  }

  void SetIsHoldingWebLockForTesting(bool is_holding_weblock) {
    SetIsHoldingWebLock(is_holding_weblock);
  }

  void SetIsHoldingIndexedDBLockForTesting(bool is_holding_weblock) {
    SetIsHoldingIndexedDBLock(is_holding_weblock);
  }

  void SetUsesWebRTCForTesting(bool uses_webrtc) { SetUsesWebRTC(uses_webrtc); }

  void SetHadFormInteractionForTesting(bool had_form_interaction) {
    SetHadFormInteraction(had_form_interaction);
  }

  void SetHadUserEditsForTesting(bool had_user_edits) {
    SetHadUserEdits(had_user_edits);
  }

  base::WeakPtr<PageNodeImpl> GetWeakPtrOnUIThread();
  base::WeakPtr<PageNodeImpl> GetWeakPtr();

  // Functions meant to be called by a FrameNodeImpl:
  void AddFrame(base::PassKey<FrameNodeImpl>, FrameNodeImpl* frame_node);
  void RemoveFrame(base::PassKey<FrameNodeImpl>, FrameNodeImpl* frame_node);

  // Function meant to be called by FrozenFrameAggregator.
  void SetLifecycleState(base::PassKey<FrozenFrameAggregator>,
                         LifecycleState lifecycle_state) {
    SetLifecycleState(lifecycle_state);
  }

  // Functions meant to be called by PageAggregatorData:
  void SetIsHoldingWebLock(base::PassKey<PageAggregatorData>,
                           bool is_holding_weblock) {
    SetIsHoldingWebLock(is_holding_weblock);
  }
  void SetIsHoldingIndexedDBLock(base::PassKey<PageAggregatorData>,
                                 bool is_holding_indexeddb_lock) {
    SetIsHoldingIndexedDBLock(is_holding_indexeddb_lock);
  }
  void SetUsesWebRTC(base::PassKey<PageAggregatorData>, bool uses_web_rtc) {
    SetUsesWebRTC(uses_web_rtc);
  }
  void SetHadFormInteraction(base::PassKey<PageAggregatorData>,
                             bool had_form_interaction) {
    SetHadFormInteraction(had_form_interaction);
  }

  void SetHadUserEdits(base::PassKey<PageAggregatorData>, bool had_user_edits) {
    SetHadUserEdits(had_user_edits);
  }

 private:
  friend class PageNodeImplDescriber;

  // Partial PageNode implementation:
  const FrameNode* GetOpenerFrameNode() const override;
  const FrameNode* GetEmbedderFrameNode() const override;
  const FrameNode* GetMainFrameNode() const override;
  NodeSetView<const FrameNode*> GetMainFrameNodes() const override;

  // NodeBase:
  void OnJoiningGraph() override;
  void OnBeforeLeavingGraph() override;
  void RemoveNodeAttachedData() override;

  void SetLifecycleState(LifecycleState lifecycle_state);
  void SetIsHoldingWebLock(bool is_holding_weblock);
  void SetIsHoldingIndexedDBLock(bool is_holding_indexeddb_lock);
  void SetUsesWebRTC(bool uses_web_rtc);
  void SetHadFormInteraction(bool had_form_interaction);
  void SetHadUserEdits(bool had_user_edits);

  // The WebContents associated with this page.
  const base::WeakPtr<content::WebContents> web_contents_;

  // The unique token that identifies this PageNode for the life of the browser.
  const PageToken page_token_;

  // The main frame nodes of this page. There can be more than one main frame
  // in a page, among other reasons because during main frame navigation, the
  // pending navigation will coexist with the existing main frame until it's
  // committed.
  NodeSet main_frame_nodes_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The total count of frames that tally up to this page.
  size_t frame_node_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // The last time at which the page visibility changed.
  base::TimeTicks visibility_change_time_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The last time at which the audible property changed, or nullopt if the node
  // has never been audible.
  std::optional<base::TimeTicks> audible_change_time_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The last time at which a main frame navigation was committed.
  base::TimeTicks navigation_committed_time_
      GUARDED_BY_CONTEXT(sequence_checker_);

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

  // The notification permission status for the last committed main frame
  // navigation.
  std::optional<blink::mojom::PermissionStatus> notification_permission_status_
      GUARDED_BY_CONTEXT(sequence_checker_);

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

  // Whether or not the page is focused. Driven by browser instrumentation.
  ObservedProperty::NotifiesOnlyOnChanges<bool,
                                          &PageNodeObserver::OnIsFocusedChanged>
      is_focused_ GUARDED_BY_CONTEXT(sequence_checker_){false};
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
  // Whether or not the page is displaying content in picture-in-picture. Driven
  // by browser instrumentation. Initialized on construction.
  ObservedProperty::NotifiesOnlyOnChanges<
      bool,
      &PageNodeObserver::OnHasPictureInPictureChanged>
      has_picture_in_picture_ GUARDED_BY_CONTEXT(sequence_checker_){false};

  const bool is_off_the_record_;

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
  // Indicates if at least one frame of the page currently uses WebRTC.
  ObservedProperty::
      NotifiesOnlyOnChanges<bool, &PageNodeObserver::OnPageUsesWebRTCChanged>
          uses_web_rtc_ GUARDED_BY_CONTEXT(sequence_checker_){false};
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

  base::WeakPtr<PageNodeImpl> weak_this_;
  base::WeakPtrFactory<PageNodeImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_PAGE_NODE_IMPL_H_
