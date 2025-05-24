// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PAGE_NODE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PAGE_NODE_H_

#include <iosfwd>
#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "components/performance_manager/public/graph/node.h"
#include "components/performance_manager/public/graph/node_set_view.h"
#include "components/performance_manager/public/mojom/lifecycle.mojom.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

class GURL;

namespace content {
class WebContents;
}

namespace resource_attribution {
class PageContext;
}

namespace performance_manager {

class FrameNode;
class PageNodeObserver;

enum class PageType {
  // A browser tab.
  kTab,
  // An extension background page.
  kExtension,
  // Anything else.
  kUnknown,
};

// A PageNode represents the root of a FrameTree, or equivalently a WebContents.
// These may correspond to normal tabs, WebViews, Chrome Apps or Extensions.
class PageNode : public TypedNode<PageNode> {
 public:
  using NodeSet = base::flat_set<const Node*>;
  template <class NodeViewPtr>
  using NodeSetView = NodeSetView<NodeSet, NodeViewPtr>;

  using LifecycleState = mojom::LifecycleState;

  // Loading state of a page.
  enum class LoadingState {
    // No top-level document has started loading yet.
    kLoadingNotStarted,
    // A different top-level document is loading. The load started less than 5
    // seconds ago or the initial response was received.
    kLoading,
    // A different top-level document is loading. The load started more than 5
    // seconds ago and no response was received yet. Note: The state will
    // transition back to |kLoading| if a response is received.
    kLoadingTimedOut,
    // A different top-level document finished loading, but the page did not
    // reach CPU and network quiescence since then. Note: A page is considered
    // to have reached CPU and network quiescence after 1 minute, even if the
    // CPU and network are still busy - see page_load_tracker_decorator.h.
    kLoadedBusy,
    // The page reached CPU and network quiescence after loading the current
    // top-level document, or the load failed.
    kLoadedIdle,
  };

  // Returns a string for an enumeration value.
  static const char* ToString(PageType type);
  static const char* ToString(PageNode::LoadingState loading_state);

  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kPage; }

  PageNode();

  PageNode(const PageNode&) = delete;
  PageNode& operator=(const PageNode&) = delete;

  ~PageNode() override;

  // Returns the unique ID of the browser context that this page belongs to.
  virtual const std::string& GetBrowserContextID() const = 0;

  // Returns the opener frame node, if there is one. This may change over the
  // lifetime of this page. See "OnOpenerFrameNodeChanged".
  virtual const FrameNode* GetOpenerFrameNode() const = 0;

  // Returns the embedder frame node, if there is one. This may change over the
  // lifetime of this page. See "OnEmbedderFrameNodeChanged".
  virtual const FrameNode* GetEmbedderFrameNode() const = 0;

  // Gets the unique token identifying this node for resource attribution. This
  // token will not be reused after the node is destroyed.
  virtual resource_attribution::PageContext GetResourceContext() const = 0;

  // Returns the type of the page.
  virtual PageType GetType() const = 0;

  // Returns true if this page has the focus.
  virtual bool IsFocused() const = 0;

  // Returns true if this page is currently visible, false otherwise.
  // See PageNodeObserver::OnIsVisibleChanged.
  virtual bool IsVisible() const = 0;

  // Returns the time of the last visibility change. It is always well defined
  // as the visibility property is set at node creation.
  virtual base::TimeTicks GetLastVisibilityChangeTime() const = 0;

  // Returns true if this page is currently audible, false otherwise.
  // See PageNodeObserver::OnIsAudibleChanged.
  virtual bool IsAudible() const = 0;

  // Returns the time since the last audible change. Unlike
  // GetLastVisibilityChangeTime(), this returns nullopt for a node which has
  // never been audible. If a node is audible when created, it is considered to
  // change from inaudible to audible at that point.
  virtual std::optional<base::TimeDelta> GetTimeSinceLastAudibleChange()
      const = 0;

  // Returns true if this page is displaying content in a picture-in-picture
  // window, false otherwise.
  virtual bool HasPictureInPicture() const = 0;

  // Returns true if this page is opted-out from freezing via origin trial, i.e.
  // if any of its current frames sets the origin trial.
  virtual bool HasFreezingOriginTrialOptOut() const = 0;

  // Returns true if this page is off the record, false otherwise.
  // A tab is off the record when it is open in incognito or guest mode.
  virtual bool IsOffTheRecord() const = 0;

  // Returns the page's loading state.
  virtual LoadingState GetLoadingState() const = 0;

  // Returns the UKM source ID associated with the URL of the main frame of
  // this page.
  // See PageNodeObserver::OnUkmSourceIdChanged.
  virtual ukm::SourceId GetUkmSourceID() const = 0;

  // Returns the lifecycle state of this page. This is aggregated from the
  // lifecycle state of each frame in the frame tree. See
  // PageNodeObserver::OnPageLifecycleStateChanged.
  virtual LifecycleState GetLifecycleState() const = 0;

  // Returns true if at least one of the frame in this page is currently
  // holding a WebLock.
  virtual bool IsHoldingWebLock() const = 0;

  // Returns true if at least one of the frame in this page is currently
  // holding an IndexedDB lock that is blocking another client.
  virtual bool IsHoldingBlockingIndexedDBLock() const = 0;

  // Returns whether at least one frame on this page currently uses WebRTC.
  virtual bool UsesWebRTC() const = 0;

  // Returns the navigation ID associated with the last committed navigation
  // event for the main frame of this page.
  // See PageNodeObserver::OnMainFrameNavigationCommitted.
  virtual int64_t GetNavigationID() const = 0;

  // Returns the MIME type for the last committed main frame navigation.
  virtual const std::string& GetContentsMimeType() const = 0;

  // Returns the notification permission status for the last committed main
  // frame navigation (nullopt if it wasn't retrieved).
  virtual std::optional<blink::mojom::PermissionStatus>
  GetNotificationPermissionStatus() const = 0;

  // Returns "zero" if no navigation has happened, otherwise returns the time
  // since the last navigation commit.
  virtual base::TimeDelta GetTimeSinceLastNavigation() const = 0;

  // Returns the current main frame node (if there is one), otherwise returns
  // any of the potentially multiple main frames that currently exist. If there
  // are no main frames at the moment, returns nullptr.
  virtual const FrameNode* GetMainFrameNode() const = 0;

  // Returns all of the main frame nodes, both current and otherwise. If there
  // are no main frames at the moment, returns the empty set.
  virtual NodeSetView<const FrameNode*> GetMainFrameNodes() const = 0;

  // Returns the URL the main frame last committed a navigation to, or the
  // initial URL of the page before navigation. The latter case is distinguished
  // by a zero navigation ID.
  // See PageNodeObserver::OnMainFrameNavigationCommitted.
  virtual const GURL& GetMainFrameUrl() const = 0;

  // Returns the private memory footprint size of the main frame and its
  // children. This differs from EstimatePrivateFootprintSize which includes
  // all the frames under the page node.
  virtual uint64_t EstimateMainFramePrivateFootprintSize() const = 0;

  // Indicates if at least one of the frames in the page has received some form
  // interactions.
  virtual bool HadFormInteraction() const = 0;

  // Indicates if at least one of the frames in the page has received
  // user-initiated edits. This is a superset of `HadFormInteraction()` that
  // also includes changes to `contenteditable` elements.
  virtual bool HadUserEdits() const = 0;

  // Returns the web contents associated with this page node. It is valid to
  // call this function on any thread but the weak pointer must only be
  // dereferenced on the UI thread.
  virtual base::WeakPtr<content::WebContents> GetWebContents() const = 0;

  virtual uint64_t EstimateResidentSetSize() const = 0;

  virtual uint64_t EstimatePrivateFootprintSize() const = 0;

  // Returns a weak pointer to this page node.
  virtual base::WeakPtr<PageNode> GetWeakPtr() = 0;
  virtual base::WeakPtr<const PageNode> GetWeakPtr() const = 0;
};

// Observer interface for page nodes.
class PageNodeObserver : public base::CheckedObserver {
 public:
  PageNodeObserver();

  PageNodeObserver(const PageNodeObserver&) = delete;
  PageNodeObserver& operator=(const PageNodeObserver&) = delete;

  ~PageNodeObserver() override;

  // Node lifetime notifications.

  // Called before a `page_node` is added to the graph. OnPageNodeAdded() is
  // better for most purposes, but this can be useful if an observer needs to
  // check the state of the graph without including `page_node`, or to set
  // initial properties on the node that should be visible to other observers in
  // OnPageNodeAdded().
  //
  // Observers may make property changes during the scope of this call, as long
  // as they don't cause notifications to be sent and don't modify pointers
  // to/from other nodes, since the node is still isolated from the graph. To
  // change a property that causes notifications, post a task (which will run
  // after OnPageNodeAdded().
  //
  // Note that observers are notified in an arbitrary order, so property changes
  // made here may or may not be visible to other observers in
  // OnBeforePageNodeAdded().
  virtual void OnBeforePageNodeAdded(const PageNode* page_node) {}

  // Called after a `page_node` is added to the graph. Observers may *not* make
  // property changes during the scope of this call. To change a property, post
  // a task which will run after all observers.
  virtual void OnPageNodeAdded(const PageNode* page_node) {}

  // Called before a `page_node` is removed from the graph. Observers may *not*
  // make property changes during the scope of this call. The node will be
  // deleted before any task posted from this scope runs.
  virtual void OnBeforePageNodeRemoved(const PageNode* page_node) {}

  // Called after a `page_node` is removed from the graph.
  // OnBeforePageNodeRemoved() is better for most purposes, but this can be
  // useful if an observer needs to check the state of the graph without
  // including `page_node`.
  //
  // Observers may *not* make property changes during the scope of this call.
  // The node will be deleted before any task posted from this scope runs.
  virtual void OnPageNodeRemoved(const PageNode* page_node) {}

  // Notifications of property changes.

  // Invoked when this page has been assigned an opener, had the opener
  // change, or had the opener removed. This happens when a page is opened
  // via window.open, or when that relationship is subsequently severed or
  // reparented.
  virtual void OnOpenerFrameNodeChanged(const PageNode* page_node,
                                        const FrameNode* previous_opener) {}

  // Invoked when this page has been assigned an embedder, had the embedder
  // change, or had the embedder removed. This can happen if a page is opened
  // via webviews, guestviews etc, or when that relationship is subsequently
  // severed or reparented.
  virtual void OnEmbedderFrameNodeChanged(const PageNode* page_node,
                                          const FrameNode* previous_embedder) {}

  // Invoked when the GetType property changes.
  virtual void OnTypeChanged(const PageNode* page_node,
                             PageType previous_type) {}

  // Invoked when the IsFocused property changes.
  virtual void OnIsFocusedChanged(const PageNode* page_node) {}

  // Invoked when the IsVisible property changes.
  //
  // GetLastVisibilityChangeTime() will return the time of the previous
  // IsVisible change. After all observers have fired it will return the time of
  // this property change.
  virtual void OnIsVisibleChanged(const PageNode* page_node) {}

  // Invoked when the IsAudible property changes.
  //
  // GetTimeSinceLastAudibleChange() will return the time since the previous
  // IsAudible change. After all observers have fired it will return the time of
  // this property change.
  virtual void OnIsAudibleChanged(const PageNode* page_node) {}

  // Invoked when the HasPictureInPicture property changes.
  virtual void OnHasPictureInPictureChanged(const PageNode* page_node) {}

  // Invoked when the HasFreezingOriginTrialOptOut() property changes.
  virtual void OnPageHasFreezingOriginTrialOptOutChanged(
      const PageNode* page_node) {}

  // Invoked when the GetLoadingState property changes.
  virtual void OnLoadingStateChanged(const PageNode* page_node,
                                     PageNode::LoadingState previous_state) {}

  // Invoked when the UkmSourceId property changes.
  virtual void OnUkmSourceIdChanged(const PageNode* page_node) {}

  // Invoked when the PageLifecycleState property changes.
  virtual void OnPageLifecycleStateChanged(const PageNode* page_node) {}

  // Invoked when the IsHoldingWebLock property changes.
  virtual void OnPageIsHoldingWebLockChanged(const PageNode* page_node) {}

  // Invoked when the IsHoldingBlockingIndexedDBLock property changes.
  virtual void OnPageIsHoldingBlockingIndexedDBLockChanged(
      const PageNode* page_node) {}

  // Invoked when the UsesWebRTC property changes.
  virtual void OnPageUsesWebRTCChanged(const PageNode* page_node) {}

  // Invoked when the GetNotificationPermissionStatus property changes.
  virtual void OnPageNotificationPermissionStatusChange(
      const PageNode* page_node,
      std::optional<blink::mojom::PermissionStatus> previous_status) {}

  // Invoked when the MainFrameUrl property changes.
  virtual void OnMainFrameUrlChanged(const PageNode* page_node) {}

  // This is fired when a non-same document navigation commits in the main
  // frame. It indicates that the the |NavigationId| property and possibly the
  // |MainFrameUrl| properties have changed.
  virtual void OnMainFrameDocumentChanged(const PageNode* page_node) {}

  // Invoked when the HadFormInteraction property changes.
  virtual void OnHadFormInteractionChanged(const PageNode* page_node) {}

  // Invoked when the HadUserEdits property changes.
  virtual void OnHadUserEditsChanged(const PageNode* page_node) {}

  // Events with no property changes.

  // Fired when the tab title associated with a page changes. This property is
  // not directly reflected on the node.
  virtual void OnTitleUpdated(const PageNode* page_node) {}

  // Fired when the favicon associated with a page is updated. This property is
  // not directly reflected on the node.
  virtual void OnFaviconUpdated(const PageNode* page_node) {}

  // Fired after `new_page_node` is created but before `page_node` is deleted
  // from being discarded. See the equivalent function on `WebContentsObserver`
  // for more detail.
  virtual void OnAboutToBeDiscarded(const PageNode* page_node,
                                    const PageNode* new_page_node) {}
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PAGE_NODE_H_
