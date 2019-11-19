// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/unguessable_token.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/gurl.h"

namespace performance_manager {

class FrameNodeImpl;
class PageNodeImpl;
class ProcessNodeImpl;
class WorkerNodeImpl;

// Frame nodes form a tree structure, each FrameNode at most has one parent that
// is a FrameNode. Conceptually, a frame corresponds to a
// content::RenderFrameHost in the browser, and a content::RenderFrameImpl /
// blink::LocalFrame in a renderer.
//
// Note that a frame in a frame tree can be replaced with another, with the
// continuity of that position represented via the |frame_tree_node_id|. It is
// possible to have multiple "sibling" nodes that share the same
// |frame_tree_node_id|. Only one of these may contribute to the content being
// rendered, and this node is designated the "current" node in content
// terminology. A swap is effectively atomic but will take place in two steps
// in the graph: the outgoing frame will first be marked as not current, and the
// incoming frame will be marked as current. As such, the graph invariant is
// that there will be 0 or 1 |is_current| frames with a given
// |frame_tree_node_id|.
//
// This occurs when a frame is navigated and the existing frame can't be reused.
// In that case a "provisional" frame is created to start the navigation. Once
// the navigation completes (which may actually involve a redirect to another
// origin meaning the frame has to be destroyed and another one created in
// another process!) and commits, the frame will be swapped with the previously
// active frame.
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
  FrameNodeImpl(GraphImpl* graph,
                ProcessNodeImpl* process_node,
                PageNodeImpl* page_node,
                FrameNodeImpl* parent_frame_node,
                int frame_tree_node_id,
                int render_frame_id,
                const base::UnguessableToken& dev_tools_token,
                int32_t browsing_instance_id,
                int32_t site_instance_id);
  ~FrameNodeImpl() override;

  void Bind(mojo::PendingReceiver<mojom::DocumentCoordinationUnit> receiver);

  // mojom::DocumentCoordinationUnit implementation.
  void SetNetworkAlmostIdle() override;
  void SetLifecycleState(LifecycleState state) override;
  void SetHasNonEmptyBeforeUnload(bool has_nonempty_beforeunload) override;
  void SetOriginTrialFreezePolicy(mojom::InterventionPolicy policy) override;
  void SetIsAdFrame() override;
  void OnNonPersistentNotificationCreated() override;

  // Partial FrameNode implementation:
  bool IsMainFrame() const override;

  // Getters for const properties. These can be called from any thread.
  FrameNodeImpl* parent_frame_node() const;
  PageNodeImpl* page_node() const;
  ProcessNodeImpl* process_node() const;
  int frame_tree_node_id() const;
  int render_frame_id() const;
  const base::UnguessableToken& dev_tools_token() const;
  int32_t browsing_instance_id() const;
  int32_t site_instance_id() const;

  // Getters for non-const properties. These are not thread safe.
  const base::flat_set<FrameNodeImpl*>& child_frame_nodes() const;
  LifecycleState lifecycle_state() const;
  InterventionPolicy origin_trial_freeze_policy() const;
  bool has_nonempty_beforeunload() const;
  const GURL& url() const;
  bool is_current() const;
  bool network_almost_idle() const;
  bool is_ad_frame() const;
  bool is_holding_weblock() const;
  bool is_holding_indexeddb_lock() const;
  const base::flat_set<WorkerNodeImpl*>& child_worker_nodes() const;
  const PriorityAndReason& priority_and_reason() const;

  // Setters are not thread safe.
  void SetIsCurrent(bool is_current);
  void SetIsHoldingWebLock(bool is_holding_weblock);
  void SetIsHoldingIndexedDBLock(bool is_holding_indexeddb_lock);

  // Invoked when a navigation is committed in the frame.
  void OnNavigationCommitted(const GURL& url, bool same_document);

  // Invoked by |worker_node| when it starts/stops being a child of this frame.
  void AddChildWorker(WorkerNodeImpl* worker_node);
  void RemoveChildWorker(WorkerNodeImpl* worker_node);

  // Invoked to set the frame priority, and the reason behind it.
  void SetPriorityAndReason(const PriorityAndReason& priority_and_reason);

 private:
  friend class FramePriorityAccess;
  friend class PageNodeImpl;
  friend class ProcessNodeImpl;

  // Rest of FrameNode implementation. These are private so that users of the
  // impl use the private getters rather than the public interface.
  const FrameNode* GetParentFrameNode() const override;
  const PageNode* GetPageNode() const override;
  const ProcessNode* GetProcessNode() const override;
  int GetFrameTreeNodeId() const override;
  const base::UnguessableToken& GetDevToolsToken() const override;
  int32_t GetBrowsingInstanceId() const override;
  int32_t GetSiteInstanceId() const override;
  const base::flat_set<const FrameNode*> GetChildFrameNodes() const override;
  LifecycleState GetLifecycleState() const override;
  InterventionPolicy GetOriginTrialFreezePolicy() const override;
  bool HasNonemptyBeforeUnload() const override;
  const GURL& GetURL() const override;
  bool IsCurrent() const override;
  bool GetNetworkAlmostIdle() const override;
  bool IsAdFrame() const override;
  bool IsHoldingWebLock() const override;
  bool IsHoldingIndexedDBLock() const override;
  const base::flat_set<const WorkerNode*> GetChildWorkerNodes() const override;
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

    // Opt-in or opt-out of freezing via origin trial.
    ObservedProperty::NotifiesOnlyOnChangesWithPreviousValue<
        mojom::InterventionPolicy,
        const mojom::InterventionPolicy&,
        &FrameNodeObserver::OnOriginTrialFreezePolicyChanged>
        origin_trial_freeze_policy{mojom::InterventionPolicy::kUnknown};
  };

  // Invoked by subframes on joining/leaving the graph.
  void AddChildFrame(FrameNodeImpl* frame_node);
  void RemoveChildFrame(FrameNodeImpl* frame_node);

  void JoinGraph() override;
  void LeaveGraph() override;

  bool HasFrameNodeInAncestors(FrameNodeImpl* frame_node) const;
  bool HasFrameNodeInDescendants(FrameNodeImpl* frame_node) const;

  mojo::Receiver<mojom::DocumentCoordinationUnit> receiver_{this};

  FrameNodeImpl* const parent_frame_node_;
  PageNodeImpl* const page_node_;
  ProcessNodeImpl* const process_node_;
  // Can be used to tie together "sibling" frames, where a navigation is ongoing
  // in a new frame that will soon replace the existing one.
  const int frame_tree_node_id_;
  // The routing id of the frame.
  const int render_frame_id_;
  // A unique identifier shared with all representations of this node across
  // content and blink. The token is only defined by the browser process and
  // is never sent back from the renderer in control calls. It should never be
  // used to look up the FrameTreeNode instance.
  const base::UnguessableToken dev_tools_token_;
  // The unique ID of the BrowsingInstance this frame belongs to. Frames in the
  // same BrowsingInstance are allowed to script each other at least
  // asynchronously (if cross-site), and sometimes synchronously (if same-site,
  // and thus same SiteInstance).
  const int32_t browsing_instance_id_;
  // The unique ID of the SiteInstance this frame belongs to. Frames in the
  // same SiteInstance may sychronously script each other. Frames with the
  // same |site_instance_id_| will also have the same |browsing_instance_id_|.
  const int32_t site_instance_id_;

  base::flat_set<FrameNodeImpl*> child_frame_nodes_;

  // Does *not* change when a navigation is committed.
  ObservedProperty::NotifiesOnlyOnChanges<
      LifecycleState,
      &FrameNodeObserver::OnFrameLifecycleStateChanged>
      lifecycle_state_{LifecycleState::kRunning};

  // This is a one way switch. Once marked an ad-frame, always an ad-frame.
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

  // Frame priority information. Set via FramePriorityDecorator.
  ObservedProperty::NotifiesOnlyOnChangesWithPreviousValue<
      PriorityAndReason,
      const PriorityAndReason&,
      &FrameNodeObserver::OnPriorityAndReasonChanged>
      priority_and_reason_{PriorityAndReason(base::TaskPriority::LOWEST,
                                             kDefaultPriorityReason)};

  // Inline storage for FramePriorityDecorator data.
  frame_priority::AcceptedVote accepted_vote_;

  DISALLOW_COPY_AND_ASSIGN(FrameNodeImpl);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_H_
