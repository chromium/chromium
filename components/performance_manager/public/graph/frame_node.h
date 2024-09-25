// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_FRAME_NODE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_FRAME_NODE_H_

#include <optional>

#include "base/containers/flat_set.h"
#include "base/observer_list_types.h"
#include "base/types/strong_alias.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/graph/node.h"
#include "components/performance_manager/public/graph/node_set_view.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/performance_manager/public/mojom/lifecycle.mojom.h"
#include "components/performance_manager/public/resource_attribution/frame_context.h"
#include "components/performance_manager/public/viewport_intersection.h"
#include "content/public/browser/browsing_instance_id.h"
#include "content/public/browser/site_instance.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/origin.h"

class GURL;

namespace performance_manager {

class FrameNodeObserver;
class PageNode;
class ProcessNode;
class RenderFrameHostProxy;
class WorkerNode;

using execution_context_priority::PriorityAndReason;

// Frame nodes form a tree structure, each FrameNode at most has one parent
// that is a FrameNode. Conceptually, a FrameNode corresponds to a
// content::RenderFrameHost (RFH) in the browser, and a
// content::RenderFrameImpl / blink::LocalFrame in a renderer.
//
// TODO(crbug.com/40182881): The naming is misleading. In the browser,
// FrameTreeNode tracks state about a frame and RenderFrameHost tracks state
// about a document loaded into that frame, which can change over time.
// (Although RFH doesn't exactly track documents 1:1 either - see
// docs/render_document.md for more details.) The PM node types should be
// cleaned up to more accurately reflect this.
//
// Each RFH is part of a frame tree made up of content::FrameTreeNodes (FTNs).
// Note that a document in an FTN can be replaced with another, so it is
// possible to have multiple "sibling" FrameNodes corresponding to RFHs in the
// same FTN. Only one of these may contribute to the content being rendered,
// and this node is designated the "current" node in content terminology.
//
// This can occur, for example, when an in-flight navigation creates a new RFH.
// The new RFH will swap with the previously active RFH when the navigation
// commits, but until then the two will coexist for the same FTN.
//
// A swap is effectively atomic but will take place in two steps in the graph:
// the outgoing frame will first be marked as not current, and the incoming
// frame will be marked as current. As such, the graph invariant is that there
// will be 0 or 1 |is_current| FrameNode's for a given FTN.
//
// It is only valid to access this object on the sequence of the graph that owns
// it.
class FrameNode : public TypedNode<FrameNode> {
 public:
  using NodeSet = base::flat_set<const Node*>;
  template <class ReturnType>
  using NodeSetView = NodeSetView<NodeSet, ReturnType>;

  using LifecycleState = mojom::LifecycleState;
  using Observer = FrameNodeObserver;

  class ObserverDefaultImpl;

  static const char* kDefaultPriorityReason;

  enum class Visibility {
    kUnknown,
    kVisible,
    kNotVisible,
  };

  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kFrame; }

  FrameNode();

  FrameNode(const FrameNode&) = delete;
  FrameNode& operator=(const FrameNode&) = delete;

  ~FrameNode() override;

  // Returns the parent of this frame node. This may be null if this frame node
  // is the main (root) node of a frame tree. This is a constant over the
  // lifetime of the frame.
  virtual const FrameNode* GetParentFrameNode() const = 0;

  // Returns the document owning the frame this RenderFrameHost is located in,
  // which will either be a parent (for <iframe>s) or outer document (for
  // <fencedframe> or an embedder (e.g. GuestViews)).
  // This method is equivalent to
  // RenderFrameHost::GetParentOrOuterDocumentOrEmbedder().
  virtual const FrameNode* GetParentOrOuterDocumentOrEmbedder() const = 0;

  // Returns the page node to which this frame belongs. This is a constant over
  // the lifetime of the frame.
  virtual const PageNode* GetPageNode() const = 0;

  // Returns the process node with which this frame belongs. This is a constant
  // over the lifetime of the frame.
  virtual const ProcessNode* GetProcessNode() const = 0;

  // Gets the unique token associated with this frame. This is a constant over
  // the lifetime of the frame and unique across all frames for all time.
  virtual const blink::LocalFrameToken& GetFrameToken() const = 0;

  // Gets the ID of the browsing instance to which this frame belongs. This is a
  // constant over the lifetime of the frame.
  virtual content::BrowsingInstanceId GetBrowsingInstanceId() const = 0;

  // Gets the ID of the SiteInstanceGroup to which this frame belongs. This is a
  // constant over the lifetime of the frame.
  virtual content::SiteInstanceGroupId GetSiteInstanceGroupId() const = 0;

  // Gets the unique token identifying this node for resource attribution. This
  // token will not be reused after the node is destroyed.
  virtual resource_attribution::FrameContext GetResourceContext() const = 0;

  // A frame is a main frame if it has no parent FrameNode. This can be called
  // from any thread.
  //
  // Note that a frame can be considered a main frame without being the
  // outermost frame node. This can happen if this is the main frame of an inner
  // WebContents (Guest view), or if this is a <fencedframe>.
  virtual bool IsMainFrame() const = 0;

  // Returns the set of child frames associated with this frame.
  virtual NodeSetView<const FrameNode*> GetChildFrameNodes() const = 0;

  // Returns the set of opened pages associated with this frame. This can change
  // over the lifetime of the frame.
  virtual NodeSetView<const PageNode*> GetOpenedPageNodes() const = 0;

  // Returns the set of embedded pages associated with this frame. This can
  // change over the lifetime of the frame.
  virtual NodeSetView<const PageNode*> GetEmbeddedPageNodes() const = 0;

  // Returns the current lifecycle state of this frame. See
  // FrameNodeObserver::OnFrameLifecycleStateChanged.
  virtual LifecycleState GetLifecycleState() const = 0;

  // Returns true if this frame had a non-empty before-unload handler at the
  // time of its last transition to the frozen lifecycle state. This is only
  // meaningful while the object is frozen.
  virtual bool HasNonemptyBeforeUnload() const = 0;

  // Returns the last committed URL for this frame.
  // See FrameNodeObserver::OnURLChanged.
  virtual const GURL& GetURL() const = 0;

  // Returns the last committed origin for this frame. nullopt if no navigation
  // was committed. See FrameNodeObserver::OnOriginChanged.
  virtual const std::optional<url::Origin>& GetOrigin() const = 0;

  // Returns true if this frame is current (is part of a content::FrameTree).
  // See FrameNodeObserver::OnCurrentFrameChanged.
  virtual bool IsCurrent() const = 0;

  // Returns the current priority of the frame, and the reason for the frame
  // having that particular priority.
  virtual const PriorityAndReason& GetPriorityAndReason() const = 0;

  // Returns true if this frames use of the network is "almost idle", indicating
  // that it is not doing any heavy loading work.
  virtual bool GetNetworkAlmostIdle() const = 0;

  // Returns true if this frame is ad frame. This can change from false to true
  // over the lifetime of the frame, but once it is true it will always remain
  // true.
  virtual bool IsAdFrame() const = 0;

  // Returns true if this frame holds at least one Web Lock.
  virtual bool IsHoldingWebLock() const = 0;

  // Returns true if this frame holds at least one IndexedDB lock. An IndexedDB
  // lock is held by an active transaction or an active DB open request.
  virtual bool IsHoldingIndexedDBLock() const = 0;

  // Returns true if this frame currently uses WebRTC.
  virtual bool UsesWebRTC() const = 0;

  // Returns the child workers of this frame. These are either dedicated workers
  // or shared workers created by this frame, or a service worker that handles
  // this frame's network requests.
  virtual NodeSetView<const WorkerNode*> GetChildWorkerNodes() const = 0;

  // Returns true if the frame has been interacted with at least once.
  virtual bool HadUserActivation() const = 0;

  // Returns true if at least one form of the frame has been interacted with.
  virtual bool HadFormInteraction() const = 0;

  // Returns true if the user has made edits to the page. This is a superset of
  // `HadFormInteraction()` but also includes changes to `contenteditable`
  // elements.
  virtual bool HadUserEdits() const = 0;

  // Returns true if the frame is audible, false otherwise.
  virtual bool IsAudible() const = 0;

  // Returns true if the frame is capturing a media stream (audio or video).
  virtual bool IsCapturingMediaStream() const = 0;

  // Returns the ViewportIntersection of this frame. For the outermost main
  // frame, this always returns a valid value indicating that the frame fully
  // intersects with the viewport. For child frames, this is initially null on
  // node creation and is initialized during layout when the viewport
  // intersection is first calculated.
  virtual std::optional<ViewportIntersection> GetViewportIntersection()
      const = 0;

  // Returns true if the frame is visible. This value is based on the viewport
  // intersection of the frame, and the visibility of the page.
  //
  // Note that for the visibility of the page, page mirroring *is* taken into
  // account, as opposed to `PageNode::IsVisible()`.
  virtual Visibility GetVisibility() const = 0;

  // Returns true if the frame is deemed important. This means that the frame
  // had been interacted with by the user, or is intersecting with a large area
  // of the viewport. Note that this is the importance in the context of the
  // containing page. If the page is not visible, the frame should not be
  // considered important, regardless of this value.
  virtual bool IsImportant() const = 0;

  // Returns a proxy to the RenderFrameHost associated with this node. The
  // proxy may only be dereferenced on the UI thread.
  virtual const RenderFrameHostProxy& GetRenderFrameHostProxy() const = 0;

  // TODO(joenotcharles): Move the resource usage estimates to a separate
  // class.

  // Returns the most recently estimated resident set of the frame, in
  // kilobytes. This is an estimate because RSS is computed by process, and a
  // process can host multiple frames.
  virtual uint64_t GetResidentSetKbEstimate() const = 0;

  // Returns the most recently estimated private footprint of the frame, in
  // kilobytes. This is an estimate because it is computed by process, and a
  // process can host multiple frames.
  virtual uint64_t GetPrivateFootprintKbEstimate() const = 0;
};

// Pure virtual observer interface. Derive from this if you want to be forced to
// implement the entire interface.
class FrameNodeObserver : public base::CheckedObserver {
 public:
  FrameNodeObserver();

  FrameNodeObserver(const FrameNodeObserver&) = delete;
  FrameNodeObserver& operator=(const FrameNodeObserver&) = delete;

  ~FrameNodeObserver() override;

  // Node lifetime notifications.

  // Called when a |frame_node| is added to the graph. Observers must not make
  // any property changes or cause re-entrant notifications during the scope of
  // this call. Instead, make property changes via a separate posted task.
  virtual void OnFrameNodeAdded(const FrameNode* frame_node) = 0;

  // Called before a |frame_node| is removed from the graph. Observers must not
  // make any property changes or cause re-entrant notifications during the
  // scope of this call.
  virtual void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) = 0;

  // Notifications of property changes.

  // Invoked when the current frame changes. Both arguments can be nullptr.
  virtual void OnCurrentFrameChanged(
      const FrameNode* previous_frame_node,
      const FrameNode* current_current_frame) = 0;

  // Invoked when the NetworkAlmostIdle property changes.
  virtual void OnNetworkAlmostIdleChanged(const FrameNode* frame_node) = 0;

  // Invoked when the LifecycleState property changes.
  virtual void OnFrameLifecycleStateChanged(const FrameNode* frame_node) = 0;

  // Invoked when the URL property changes.
  virtual void OnURLChanged(const FrameNode* frame_node,
                            const GURL& previous_value) = 0;

  // Invoked when the origin property changes.
  virtual void OnOriginChanged(
      const FrameNode* frame_node,
      const std::optional<url::Origin>& previous_value) = 0;

  // Invoked when the IsAdFrame property changes.
  virtual void OnIsAdFrameChanged(const FrameNode* frame_node) = 0;

  // Invoked when the IsHoldingWebLock() property changes.
  virtual void OnFrameIsHoldingWebLockChanged(const FrameNode* frame_node) = 0;

  // Invoked when the IsHoldingIndexedDBLock() property changes.
  virtual void OnFrameIsHoldingIndexedDBLockChanged(
      const FrameNode* frame_node) = 0;

  // Invoked when the frame priority and reason changes.
  virtual void OnPriorityAndReasonChanged(
      const FrameNode* frame_node,
      const PriorityAndReason& previous_value) = 0;

  // Called when the frame is interacted with by the user.
  virtual void OnHadUserActivationChanged(const FrameNode* frame_node) = 0;

  // Called when the frame receives a form interaction.
  virtual void OnHadFormInteractionChanged(const FrameNode* frame_node) = 0;

  // Called the first time the user has edited the content of an element. This
  // is a superset of `OnHadFormInteractionChanged()`: form interactions trigger
  // both events but changes to e.g. a `<div>` with the `contenteditable`
  // property will only trigger `OnHadUserEditsChanged()`.
  virtual void OnHadUserEditsChanged(const FrameNode* frame_node) = 0;

  // Called when the frame starts or stops using WebRTC.
  virtual void OnFrameUsesWebRTCChanged(const FrameNode* frame_node) = 0;

  // Invoked when the IsAudible property changes.
  virtual void OnIsAudibleChanged(const FrameNode* frame_node) = 0;

  // Invoked when the IsCapturingMediaStream property changes.
  virtual void OnIsCapturingMediaStreamChanged(const FrameNode* frame_node) = 0;

  // Invoked when a frame's intersection with the viewport changes. Will only be
  // invoked for a child frame, as the outermost main frame is always considered
  // to be fully intersecting with the viewport.
  virtual void OnViewportIntersectionChanged(const FrameNode* frame_node) = 0;

  // Invoked when the visibility property changes.
  virtual void OnFrameVisibilityChanged(
      const FrameNode* frame_node,
      FrameNode::Visibility previous_value) = 0;

  // Invoked when the `IsImportant` property changes.
  virtual void OnIsImportantChanged(const FrameNode* frame_node) = 0;

  // Events with no property changes.

  // Invoked when a non-persistent notification has been issued by the frame.
  virtual void OnNonPersistentNotificationCreated(
      const FrameNode* frame_node) = 0;

  // Invoked when the frame has had a first contentful paint, as defined here:
  // https://developers.google.com/web/tools/lighthouse/audits/first-contentful-paint
  // This may not fire for all frames, depending on if the load is interrupted
  // or if the content is even visible. It will fire at most once for a given
  // frame. It will only fire for main-frame nodes.
  virtual void OnFirstContentfulPaint(
      const FrameNode* frame_node,
      base::TimeDelta time_since_navigation_start) = 0;
};

// Default implementation of observer that provides dummy versions of each
// function. Derive from this if you only need to implement a few of the
// functions.
class FrameNode::ObserverDefaultImpl : public FrameNodeObserver {
 public:
  ObserverDefaultImpl();

  ObserverDefaultImpl(const ObserverDefaultImpl&) = delete;
  ObserverDefaultImpl& operator=(const ObserverDefaultImpl&) = delete;

  ~ObserverDefaultImpl() override;

  // FrameNodeObserver implementation:
  void OnFrameNodeAdded(const FrameNode* frame_node) override {}
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override {}
  void OnCurrentFrameChanged(const FrameNode* previous_frame_node,
                             const FrameNode* current_frame_node) override {}
  void OnNetworkAlmostIdleChanged(const FrameNode* frame_node) override {}
  void OnFrameLifecycleStateChanged(const FrameNode* frame_node) override {}
  void OnURLChanged(const FrameNode* frame_node,
                    const GURL& previous_value) override {}
  void OnOriginChanged(
      const FrameNode* frame_node,
      const std::optional<url::Origin>& previous_value) override {}
  void OnIsAdFrameChanged(const FrameNode* frame_node) override {}
  void OnFrameIsHoldingWebLockChanged(const FrameNode* frame_node) override {}
  void OnFrameIsHoldingIndexedDBLockChanged(
      const FrameNode* frame_node) override {}
  void OnPriorityAndReasonChanged(
      const FrameNode* frame_node,
      const PriorityAndReason& previous_value) override {}
  void OnHadUserActivationChanged(const FrameNode* frame_node) override {}
  void OnHadFormInteractionChanged(const FrameNode* frame_node) override {}
  void OnHadUserEditsChanged(const FrameNode* frame_node) override {}
  void OnFrameUsesWebRTCChanged(const FrameNode* frame_node) override {}
  void OnIsAudibleChanged(const FrameNode* frame_node) override {}
  void OnIsCapturingMediaStreamChanged(const FrameNode* frame_node) override {}
  void OnViewportIntersectionChanged(const FrameNode* frame_node) override {}
  void OnFrameVisibilityChanged(const FrameNode* frame_node,
                                FrameNode::Visibility previous_value) override {
  }
  void OnIsImportantChanged(const FrameNode* frame_node) override {}
  void OnNonPersistentNotificationCreated(
      const FrameNode* frame_node) override {}
  void OnFirstContentfulPaint(
      const FrameNode* frame_node,
      base::TimeDelta time_since_navigation_start) override {}
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_FRAME_NODE_H_
