// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/performance_manager/public/mojom/web_memory.mojom.h"
#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "content/public/browser/browsing_instance_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace performance_manager {

class FrameNodeImplDescriber;

namespace execution_context {
class ExecutionContextAccess;
}  // namespace execution_context

// Frame nodes for a tree structure that is described in
// components/performance_manager/public/graph/frame_node.h.
class FrameNodeImpl
    : public PublicNodeImpl<FrameNodeImpl, FrameNode>,
      public TypedNodeBase<FrameNodeImpl, FrameNode, FrameNodeObserver>,
      public mojom::DocumentCoordinationUnit {
 public:
  static const char kDefaultPriorityReason[];
  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kFrame; }

  // Construct a frame node associated with a |process_node|, a |page_node| and
  // optionally with a |parent_frame_node|. For the main frame of |page_node|
  // the |parent_frame_node| parameter should be nullptr. |render_frame_id| is
  // the routing id of the frame (from RenderFrameHost::GetRoutingID).
  FrameNodeImpl(ProcessNodeImpl* process_node,
                PageNodeImpl* page_node,
                FrameNodeImpl* parent_frame_node,
                int render_frame_id,
                const blink::LocalFrameToken& frame_token,
                content::BrowsingInstanceId browsing_instance_id,
                content::SiteInstanceId site_instance_id);

  FrameNodeImpl(const FrameNodeImpl&) = delete;
  FrameNodeImpl& operator=(const FrameNodeImpl&) = delete;

  ~FrameNodeImpl() override;

  void Bind(mojo::PendingReceiver<mojom::DocumentCoordinationUnit> receiver);

  // mojom::DocumentCoordinationUnit implementation.
  void SetNetworkAlmostIdle() override;
  void SetLifecycleState(LifecycleState state) override;
  void SetHasNonEmptyBeforeUnload(bool has_nonempty_beforeunload) override;
  void SetIsAdFrame(bool is_ad_frame) override;
  void SetHadFormInteraction() override;
  void SetHadUserEdits() override;
  void OnNonPersistentNotificationCreated() override;
  void OnFirstContentfulPaint(
      base::TimeDelta time_since_navigation_start) override;
  const RenderFrameHostProxy& GetRenderFrameHostProxy() const override;
  void OnWebMemoryMeasurementRequested(
      mojom::WebMemoryMeasurement::Mode mode,
      OnWebMemoryMeasurementRequestedCallback callback) override;

  // Partial FrameNodbase::TimeDelta time_since_navigatione implementation:
  bool IsMainFrame() const override;

  // Getters for const properties.
  FrameNodeImpl* parent_frame_node() const;
  PageNodeImpl* page_node() const;
  ProcessNodeImpl* process_node() const;
  int render_frame_id() const;
  const blink::LocalFrameToken& frame_token() const;
  content::BrowsingInstanceId browsing_instance_id() const;
  content::SiteInstanceId site_instance_id() const;
  resource_attribution::FrameContext resource_context() const;
  const RenderFrameHostProxy& render_frame_host_proxy() const;

  // Getters for non-const properties. These are not thread safe.
  const base::flat_set<FrameNodeImpl*>& child_frame_nodes() const;
  const base::flat_set<PageNodeImpl*>& opened_page_nodes() const;
  const base::flat_set<PageNodeImpl*>& embedded_page_nodes() const;
  LifecycleState lifecycle_state() const;
  bool has_nonempty_beforeunload() const;
  const GURL& url() const;
  bool is_current() const;
  bool network_almost_idle() const;
  bool is_ad_frame() const;
  bool is_holding_weblock() const;
  bool is_holding_indexeddb_lock() const;
  const base::flat_set<WorkerNodeImpl*>& child_worker_nodes() const;
  const PriorityAndReason& priority_and_reason() const;
  bool had_form_interaction() const;
  bool had_user_edits() const;
  bool is_audible() const;
  const absl::optional<gfx::Rect>& viewport_intersection() const;
  Visibility visibility() const;
  uint64_t resident_set_kb_estimate() const;
  uint64_t private_footprint_kb_estimate() const;

  // Setters are not thread safe.
  void SetIsCurrent(bool is_current);
  void SetIsHoldingWebLock(bool is_holding_weblock);
  void SetIsHoldingIndexedDBLock(bool is_holding_indexeddb_lock);
  void SetIsAudible(bool is_audible);
  void SetViewportIntersection(const gfx::Rect& viewport_intersection);
  void SetInitialVisibility(Visibility visibility);
  void SetVisibility(Visibility visibility);
  void SetResidentSetKbEstimate(uint64_t rss_estimate);
  void SetPrivateFootprintKbEstimate(uint64_t private_footprint_estimate);

  // Invoked when a navigation is committed in the frame.
  void OnNavigationCommitted(const GURL& url, bool same_document);

  // Invoked by |worker_node| when it starts/stops being a child of this frame.
  void AddChildWorker(WorkerNodeImpl* worker_node);
  void RemoveChildWorker(WorkerNodeImpl* worker_node);

  // Invoked to set the frame priority, and the reason behind it.
  void SetPriorityAndReason(const PriorityAndReason& priority_and_reason);

  base::WeakPtr<FrameNodeImpl> GetWeakPtrOnUIThread();
  base::WeakPtr<FrameNodeImpl> GetWeakPtr();

  void SeverPageRelationshipsAndMaybeReparentForTesting() {
    SeverPageRelationshipsAndMaybeReparent();
  }

  // Implementation details below this point.

  // Invoked by opened pages when this frame is set/cleared as their opener.
  // See PageNodeImpl::(Set|Clear)OpenerFrameNode.
  void AddOpenedPage(base::PassKey<PageNodeImpl> key, PageNodeImpl* page_node);
  void RemoveOpenedPage(base::PassKey<PageNodeImpl> key,
                        PageNodeImpl* page_node);

  // Invoked by embedded pages when this frame is set/cleared as their embedder.
  // See PageNodeImpl::(Set|Clear)EmbedderFrameNodeAndEmbeddingType.
  void AddEmbeddedPage(base::PassKey<PageNodeImpl> key,
                       PageNodeImpl* page_node);
  void RemoveEmbeddedPage(base::PassKey<PageNodeImpl> key,
                          PageNodeImpl* page_node);

  // Used by the ExecutionContextRegistry mechanism.
  std::unique_ptr<NodeAttachedData>* GetExecutionContextStorage(
      base::PassKey<execution_context::ExecutionContextAccess> key) {
    return &execution_context_;
  }

 private:
  friend class ExecutionContextPriorityAccess;
  friend class FrameNodeImplDescriber;
  friend class ProcessNodeImpl;

  // Rest of FrameNode implementation. These are private so that users of the
  // impl use the private getters rather than the public interface.
  const FrameNode* GetParentFrameNode() const override;
  const PageNode* GetPageNode() const override;
  const ProcessNode* GetProcessNode() const override;
  const blink::LocalFrameToken& GetFrameToken() const override;
  content::BrowsingInstanceId GetBrowsingInstanceId() const override;
  content::SiteInstanceId GetSiteInstanceId() const override;
  resource_attribution::FrameContext GetResourceContext() const override;
  bool VisitChildFrameNodes(const FrameNodeVisitor& visitor) const override;
  const base::flat_set<const FrameNode*> GetChildFrameNodes() const override;
  bool VisitOpenedPageNodes(const PageNodeVisitor& visitor) const override;
  const base::flat_set<const PageNode*> GetOpenedPageNodes() const override;
  bool VisitEmbeddedPageNodes(const PageNodeVisitor& visitor) const override;
  const base::flat_set<const PageNode*> GetEmbeddedPageNodes() const override;
  LifecycleState GetLifecycleState() const override;
  bool HasNonemptyBeforeUnload() const override;
  const GURL& GetURL() const override;
  bool IsCurrent() const override;
  bool GetNetworkAlmostIdle() const override;
  bool IsAdFrame() const override;
  bool IsHoldingWebLock() const override;
  bool IsHoldingIndexedDBLock() const override;
  const base::flat_set<const WorkerNode*> GetChildWorkerNodes() const override;
  bool VisitChildDedicatedWorkers(
      const WorkerNodeVisitor& visitor) const override;
  bool HadFormInteraction() const override;
  bool HadUserEdits() const override;
  bool IsAudible() const override;
  const absl::optional<gfx::Rect>& GetViewportIntersection() const override;
  Visibility GetVisibility() const override;
  uint64_t GetResidentSetKbEstimate() const override;
  uint64_t GetPrivateFootprintKbEstimate() const override;
  const PriorityAndReason& GetPriorityAndReason() const override;

  // Properties associated with a Document, which are reset when a
  // different-document navigation is committed in the frame.
  struct DocumentProperties {
    DocumentProperties();
    ~DocumentProperties();

    void Reset(FrameNodeImpl* frame_node, const GURL& url_in);

    ObservedProperty::NotifiesOnlyOnChangesWithPreviousValue<
        GURL,
        const GURL&,
        &FrameNodeObserver::OnURLChanged>
        url;
    bool has_nonempty_beforeunload = false;

    // Network is considered almost idle when there are no more than 2 network
    // connections.
    ObservedProperty::NotifiesOnlyOnChanges<
        bool,
        &FrameNodeObserver::OnNetworkAlmostIdleChanged>
        network_almost_idle{false};

    // Indicates if a form in the frame has been interacted with.
    // TODO(crbug.com/1156388): Remove this once HadUserEdits is known to cover
    // all existing cases.
    ObservedProperty::NotifiesOnlyOnChanges<
        bool,
        &FrameNodeObserver::OnHadFormInteractionChanged>
        had_form_interaction{false};

    // Indicates that the user has made edits to the page. This is a superset of
    // `had_form_interaction`, but can also represent changes to
    // `contenteditable` elements.
    ObservedProperty::
        NotifiesOnlyOnChanges<bool, &FrameNodeObserver::OnHadUserEditsChanged>
            had_user_edits{false};
  };

  // Invoked by subframes on joining/leaving the graph.
  void AddChildFrame(FrameNodeImpl* frame_node);
  void RemoveChildFrame(FrameNodeImpl* frame_node);

  // NodeBase:
  void OnJoiningGraph() override;
  void OnBeforeLeavingGraph() override;
  void RemoveNodeAttachedData() override;

  // Helper function to sever all opened/embedded page relationships. This is
  // called before destroying the frame node in "OnBeforeLeavingGraph". Note
  // that this will reparent embedded pages to this frame's parent so that
  // tracking is maintained.
  void SeverPageRelationshipsAndMaybeReparent();

  // This is not quite the same as GetMainFrame, because there can be multiple
  // main frames while the main frame is navigating. This explicitly walks up
  // the tree to find the main frame that corresponds to this frame tree node,
  // even if it is not current.
  FrameNodeImpl* GetFrameTreeRoot() const;

  bool HasFrameNodeInAncestors(FrameNodeImpl* frame_node) const;
  bool HasFrameNodeInDescendants(FrameNodeImpl* frame_node) const;
  bool HasFrameNodeInTree(FrameNodeImpl* frame_node) const;

  mojo::Receiver<mojom::DocumentCoordinationUnit> receiver_{this};

  const raw_ptr<FrameNodeImpl, DanglingUntriaged> parent_frame_node_;
  const raw_ptr<PageNodeImpl, DanglingUntriaged> page_node_;
  const raw_ptr<ProcessNodeImpl, DanglingUntriaged> process_node_;
  // The routing id of the frame.
  const int render_frame_id_;

  // This is the unique token for this frame instance as per e.g.
  // RenderFrameHost::GetFrameToken().
  const blink::LocalFrameToken frame_token_;

  // The unique ID of the BrowsingInstance this frame belongs to. Frames in the
  // same BrowsingInstance are allowed to script each other at least
  // asynchronously (if cross-site), and sometimes synchronously (if same-site,
  // and thus same SiteInstance).
  const content::BrowsingInstanceId browsing_instance_id_;
  // The unique ID of the SiteInstance this frame belongs to. Frames in the
  // same SiteInstance may sychronously script each other. Frames with the
  // same |site_instance_id_| will also have the same |browsing_instance_id_|.
  const content::SiteInstanceId site_instance_id_;
  // A proxy object that lets the underlying RFH be safely dereferenced on the
  // UI thread.
  const RenderFrameHostProxy render_frame_host_proxy_;

  base::flat_set<FrameNodeImpl*> child_frame_nodes_;

  // The set of pages that have been opened by this frame.
  base::flat_set<PageNodeImpl*> opened_page_nodes_;

  // The set of pages that have been embedded by this frame.
  base::flat_set<PageNodeImpl*> embedded_page_nodes_;

  uint64_t resident_set_kb_estimate_ = 0;

  uint64_t private_footprint_kb_estimate_ = 0;

  // Does *not* change when a navigation is committed.
  ObservedProperty::NotifiesOnlyOnChanges<
      LifecycleState,
      &FrameNodeObserver::OnFrameLifecycleStateChanged>
      lifecycle_state_{LifecycleState::kRunning};

  ObservedProperty::
      NotifiesOnlyOnChanges<bool, &FrameNodeObserver::OnIsAdFrameChanged>
          is_ad_frame_{false};

  // Locks held by a frame are tracked independently from navigation
  // (specifically, a few tasks must run in the Web Lock and IndexedDB
  // subsystems after a navigation for locks to be released).
  ObservedProperty::NotifiesOnlyOnChanges<
      bool,
      &FrameNodeObserver::OnFrameIsHoldingWebLockChanged>
      is_holding_weblock_{false};
  ObservedProperty::NotifiesOnlyOnChanges<
      bool,
      &FrameNodeObserver::OnFrameIsHoldingIndexedDBLockChanged>
      is_holding_indexeddb_lock_{false};

  ObservedProperty::
      NotifiesOnlyOnChanges<bool, &FrameNodeObserver::OnIsCurrentChanged>
          is_current_{false};

  // Properties associated with a Document, which are reset when a
  // different-document navigation is committed in the frame.
  //
  // TODO(fdoray): Cleanup this once there is a 1:1 mapping between
  // RenderFrameHost and Document https://crbug.com/936696.
  DocumentProperties document_;

  // The child workers of this frame.
  base::flat_set<WorkerNodeImpl*> child_worker_nodes_;

  // Frame priority information. Set via ExecutionContextPriorityDecorator.
  ObservedProperty::NotifiesOnlyOnChangesWithPreviousValue<
      PriorityAndReason,
      const PriorityAndReason&,
      &FrameNodeObserver::OnPriorityAndReasonChanged>
      priority_and_reason_{PriorityAndReason(base::TaskPriority::LOWEST,
                                             kDefaultPriorityReason)};

  // Indicates if the frame is audible. This is tracked independently of a
  // document, and if a document swap occurs the audio stream monitor machinery
  // will keep this up to date.
  ObservedProperty::
      NotifiesOnlyOnChanges<bool, &FrameNodeObserver::OnIsAudibleChanged>
          is_audible_{false};

  // Tracks the intersection of this frame with the viewport.
  //
  // Note that the viewport intersection for the main frame is always invalid.
  // This is because the main frame always occupies the entirety of the viewport
  // so there is no point in tracking it. To avoid programming mistakes, it is
  // forbidden to query this property for the main frame.
  ObservedProperty::NotifiesOnlyOnChanges<
      absl::optional<gfx::Rect>,
      &FrameNodeObserver::OnViewportIntersectionChanged>
      viewport_intersection_;

  // Indicates if the frame is visible. This is maintained by the
  // FrameVisibilityDecorator.
  ObservedProperty::NotifiesOnlyOnChangesWithPreviousValue<
      Visibility,
      Visibility,
      &FrameNodeObserver::OnFrameVisibilityChanged>
      visibility_{Visibility::kUnknown};

  // Inline storage for ExecutionContext.
  std::unique_ptr<NodeAttachedData> execution_context_;

  base::WeakPtr<FrameNodeImpl> weak_this_;
  base::WeakPtrFactory<FrameNodeImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_H_
