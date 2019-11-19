// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_FRAME_NODE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_FRAME_NODE_H_

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "components/performance_manager/public/frame_priority/frame_priority.h"
#include "components/performance_manager/public/graph/node.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/performance_manager/public/mojom/lifecycle.mojom.h"

class GURL;

namespace base {
class UnguessableToken;
}  // namespace base

namespace performance_manager {

class FrameNodeObserver;
class PageNode;
class ProcessNode;
class WorkerNode;

// Frame nodes form a tree structure, each FrameNode at most has one parent that
// is a FrameNode. Conceptually, a frame corresponds to a
// content::RenderFrameHost in the browser, and a content::RenderFrameImpl /
// blink::LocalFrame/blink::Document in a renderer.
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
//
// It is only valid to access this object on the sequence of the graph that owns
// it.
class FrameNode : public Node {
 public:
  using LifecycleState = mojom::LifecycleState;
  using InterventionPolicy = mojom::InterventionPolicy;
  using Observer = FrameNodeObserver;
  using PriorityAndReason = frame_priority::PriorityAndReason;

  class ObserverDefaultImpl;

  static const char* kDefaultPriorityReason;

  FrameNode();
  ~FrameNode() override;

  // Returns the parent of this frame node. This may be null if this frame node
  // is the main (root) node of a frame tree. This is a constant over the
  // lifetime of the frame.
  virtual const FrameNode* GetParentFrameNode() const = 0;

  // Returns the page node to which this frame belongs. This is a constant over
  // the lifetime of the frame.
  virtual const PageNode* GetPageNode() const = 0;

  // Returns the process node with which this frame belongs. This is a constant
  // over the lifetime of the frame.
  virtual const ProcessNode* GetProcessNode() const = 0;

  // Gets the FrameTree node ID associated with this node. There may be multiple
  // sibling nodes with the same frame tree node ID, but at most 1 of them may
  // be current at a time. This is a constant over the lifetime of the frame.
  virtual int GetFrameTreeNodeId() const = 0;

  // Gets the devtools token associated with this frame. This is a constant over
  // the lifetime of the frame.
  virtual const base::UnguessableToken& GetDevToolsToken() const = 0;

  // Gets the ID of the browsing instance to which this frame belongs. This is a
  // constant over the lifetime of the frame.
  virtual int32_t GetBrowsingInstanceId() const = 0;

  // Gets the ID of the site instance to which this frame belongs. This is a
  // constant over the lifetime of the frame.
  virtual int32_t GetSiteInstanceId() const = 0;

  // A frame is a main frame if it has no parent FrameNode. This can be
  // called from any thread.
  virtual bool IsMainFrame() const = 0;

  // Returns the set of child frame associated with this frame. Note that this
  // incurs a full container copy of all child nodes. Please use ForEach when
  // that makes sense.
  virtual const base::flat_set<const FrameNode*> GetChildFrameNodes() const = 0;

  // Returns the current lifecycle state of this frame. See
  // FrameNodeObserver::OnFrameLifecycleStateChanged.
  virtual LifecycleState GetLifecycleState() const = 0;

  // Returns the freeze policy set via origin trial. kDefault when no freeze
  // policy is set via origin trial.
  virtual InterventionPolicy GetOriginTrialFreezePolicy() const = 0;

  // Returns true if this frame had a non-empty before-unload handler at the
  // time of its last transition to the frozen lifecycle state. This is only
  // meaningful while the object is frozen.
  virtual bool HasNonemptyBeforeUnload() const = 0;

  // Returns the URL associated with this frame.
  // See FrameNodeObserver::OnURLChanged.
  virtual const GURL& GetURL() const = 0;

  // Returns true if this frame is current (is part of a content::FrameTree).
  // See FrameNodeObserver::OnIsCurrentChanged.
  virtual bool IsCurrent() const = 0;

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

  // Returns the child workers of this frame. These are either dedicated workers
  // or shared workers created by this frame, or a service worker that handles
  // this frame's network requests.
  virtual const base::flat_set<const WorkerNode*> GetChildWorkerNodes()
      const = 0;

  // Returns the current priority of the frame, and the reason for the frame
  // having that particular priority.
  virtual const PriorityAndReason& GetPriorityAndReason() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameNode);
};

// Pure virtual observer interface. Derive from this if you want to be forced to
// implement the entire interface.
class FrameNodeObserver {
 public:
  using InterventionPolicy = mojom::InterventionPolicy;
  using PriorityAndReason = frame_priority::PriorityAndReason;

  FrameNodeObserver();
  virtual ~FrameNodeObserver();

  // Node lifetime notifications.

  // Called when a |frame_node| is added to the graph.
  virtual void OnFrameNodeAdded(const FrameNode* frame_node) = 0;

  // Called before a |frame_node| is removed from the graph.
  virtual void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) = 0;

  // Notifications of property changes.

  // Invoked when the IsCurrent property changes.
  virtual void OnIsCurrentChanged(const FrameNode* frame_node) = 0;

  // Invoked when the NetworkAlmostIdle property changes.
  virtual void OnNetworkAlmostIdleChanged(const FrameNode* frame_node) = 0;

  // Invoked when the LifecycleState property changes.
  virtual void OnFrameLifecycleStateChanged(const FrameNode* frame_node) = 0;

  // Invoked when the OriginTrialFreezePolicy changes.
  virtual void OnOriginTrialFreezePolicyChanged(
      const FrameNode* frame_node,
      const InterventionPolicy& previous_value) = 0;

  // Invoked when the URL property changes.
  virtual void OnURLChanged(const FrameNode* frame_node,
                            const GURL& previous_value) = 0;

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

  // Events with no property changes.

  // Invoked when a non-persistent notification has been issued by the frame.
  virtual void OnNonPersistentNotificationCreated(
      const FrameNode* frame_node) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameNodeObserver);
};

// Default implementation of observer that provides dummy versions of each
// function. Derive from this if you only need to implement a few of the
// functions.
class FrameNode::ObserverDefaultImpl : public FrameNodeObserver {
 public:
  ObserverDefaultImpl();
  ~ObserverDefaultImpl() override;

  // FrameNodeObserver implementation:
  void OnFrameNodeAdded(const FrameNode* frame_node) override {}
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override {}
  void OnIsCurrentChanged(const FrameNode* frame_node) override {}
  void OnNetworkAlmostIdleChanged(const FrameNode* frame_node) override {}
  void OnFrameLifecycleStateChanged(const FrameNode* frame_node) override {}
  void OnOriginTrialFreezePolicyChanged(
      const FrameNode* frame_node,
      const InterventionPolicy& previous_value) override {}
  void OnURLChanged(const FrameNode* frame_node,
                    const GURL& previous_value) override {}
  void OnIsAdFrameChanged(const FrameNode* frame_node) override {}
  void OnFrameIsHoldingWebLockChanged(const FrameNode* frame_node) override {}
  void OnFrameIsHoldingIndexedDBLockChanged(
      const FrameNode* frame_node) override {}
  void OnNonPersistentNotificationCreated(
      const FrameNode* frame_node) override {}
  void OnPriorityAndReasonChanged(
      const FrameNode* frame_node,
      const PriorityAndReason& previous_value) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ObserverDefaultImpl);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_FRAME_NODE_H_
