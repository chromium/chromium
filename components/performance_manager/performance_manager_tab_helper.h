// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_TAB_HELPER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_TAB_HELPER_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom-forward.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom-forward.h"

namespace performance_manager {

class FrameNodeImpl;

// This tab helper maintains a page node, and its associated tree of frame nodes
// in the performance manager graph. It also sources a smattering of attributes
// into the graph, including visibility, title, and favicon bits.
// In addition it handles forwarding interface requests from the render frame
// host to the frame graph entity.
class PerformanceManagerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PerformanceManagerTabHelper> {
 public:
  // Observer interface to be notified when a PerformanceManagerTabHelper is
  // being teared down.
  class DestructionObserver {
   public:
    virtual ~DestructionObserver() = default;
    virtual void OnPerformanceManagerTabHelperDestroying(
        content::WebContents*) = 0;
  };

  PerformanceManagerTabHelper(const PerformanceManagerTabHelper&) = delete;
  PerformanceManagerTabHelper& operator=(const PerformanceManagerTabHelper&) =
      delete;

  ~PerformanceManagerTabHelper() override;

  // Returns the PageNode associated with this WebContents.
  // TODO(crbug.com/40182881): Rename to `page_node()` since there is only one
  // `PageNode` per `WebContents`.
  PageNodeImpl* primary_page_node() { return page_node_.get(); }

  // Registers an observer that is notified when the PerformanceManagerTabHelper
  // is destroyed. Can only be set to non-nullptr if it was previously nullptr,
  // and vice-versa.
  void SetDestructionObserver(DestructionObserver* destruction_observer);

  // Detaches the tab helper from the WebContents and deletes it. This must be
  // used instead of calling WebContents::RemoveUserData directly to be sure
  // resources are cleaned up in the right order.
  void TearDownAndSelfDelete();

  // WebContentsObserver overrides.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void RenderFrameHostStateChanged(
      content::RenderFrameHost* render_frame_host,
      content::RenderFrameHost::LifecycleState old_state,
      content::RenderFrameHost::LifecycleState new_state) override;
  void OnVisibilityWillChange(content::Visibility visibility) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void OnAudioStateChanged(bool audible) override;
  void OnFrameAudioStateChanged(content::RenderFrameHost* render_frame_host,
                                bool is_audible) override;
  void OnRemoteSubframeViewportIntersectionStateChanged(
      content::RenderFrameHost* render_frame_host,
      const blink::mojom::ViewportIntersectionState&
          viewport_intersection_state) override;
  void OnFrameVisibilityChanged(
      content::RenderFrameHost* render_frame_host,
      blink::mojom::FrameVisibility visibility) override;
  void OnFrameIsCapturingMediaStreamChanged(
      content::RenderFrameHost* render_frame_host,
      bool is_capturing_media_stream) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void FrameReceivedUserActivation(
      content::RenderFrameHost* render_frame_host) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void InnerWebContentsAttached(
      content::WebContents* inner_web_contents,
      content::RenderFrameHost* render_frame_host) override;
  void SurfaceEmbedChildWebContentsAttached(
      content::WebContents* inner_web_contents,
      content::RenderFrameHost* embedder_render_frame_host) override;
  void SurfaceEmbedChildWebContentsDetached(
      content::WebContents* inner_web_contents) override;
  void WebContentsDestroyed() override;
  void DidUpdateFaviconURL(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates,
      blink::mojom::FaviconUpdateReason reason) override;
  void MediaPictureInPictureChanged(bool is_picture_in_picture) override;
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;
  void OnWebContentsLostFocus(
      content::RenderWidgetHost* render_widget_host) override;
  void AboutToBeDiscarded(content::WebContents* new_contents) override;

  void BindDocumentCoordinationUnit(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::DocumentCoordinationUnit> receiver);

  // Retrieves the frame node associated with |render_frame_host|. Returns
  // nullptr if none exist for that frame.
  FrameNodeImpl* GetFrameNode(
      content::RenderFrameHost* render_frame_host) const;

  class Observer : public base::CheckedObserver {
   public:
    // Invoked when a frame node is about to be removed from the graph.
    virtual void OnBeforeFrameNodeRemoved(
        PerformanceManagerTabHelper* performance_manager,
        FrameNodeImpl* frame_node) = 0;
  };

  // Adds/removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Overrides the clock used to detect client redirects. Passing nullptr
  // restores the default clock.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

 private:
  friend class content::WebContentsUserData<PerformanceManagerTabHelper>;
  friend class PerformanceManagerRegistryImpl;
  FRIEND_TEST_ALL_PREFIXES(PerformanceManagerFencedFrameBrowserTest,
                           FencedFrameDoesNotHaveParentFrameNode);

  explicit PerformanceManagerTabHelper(content::WebContents* web_contents);

  // Make CreateForWebContents private to restrict usage to
  // PerformanceManagerRegistry.
  using WebContentsUserData<PerformanceManagerTabHelper>::CreateForWebContents;

  void OnMainFrameNavigation(int64_t navigation_id);

  // Returns true if a navigation that isn't user- or browser-initiated,
  // starting now, is a client redirect of a current document that was user- or
  // browser-initiated, i.e. it starts shortly after that document committed.
  bool IsClientRedirectOfUserOrBrowserInitiatedDocument() const;

  // Sets the page node's IsUserOrBrowserInitiatedLoad() property to the value
  // of the most recently started pending navigation, or of the current
  // document if no navigation is pending.
  void UpdateIsUserOrBrowserInitiatedLoad();

  // Returns the notification permission status for the current main frame and
  // subscribes to changes.
  std::optional<blink::mojom::PermissionStatus>
  GetNotificationPermissionStatusAndObserveChanges();

  // Callback invoked when the current main frame's notification permission
  // status changes.
  void OnNotificationPermissionResultChange(
      content::PermissionResult permission_result);

  // Returns the FrameNodeImpl* associated with `render_frame_host`. This
  // CHECKs that it exists.
  FrameNodeImpl* GetExistingFrameNode(
      content::RenderFrameHost* render_frame_host) const;

  // The actual page node.
  std::unique_ptr<PageNodeImpl> page_node_;

  // The UKM source ID for this page.
  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  // Whether the navigation that committed the current document of the primary
  // main frame was user- or browser-initiated. Reflected on the page node while
  // no navigation is pending. Defaults to true to match the PageNode default.
  bool is_current_document_user_or_browser_initiated_ = true;

  // When the current document of the primary main frame committed. Used to
  // detect client redirects. Unset until the first commit.
  std::optional<base::TimeTicks> current_document_commit_time_;

  struct PendingNavigation {
    int64_t navigation_id;
    // Computed when the navigation started, including client redirect
    // inheritance.
    bool is_user_or_browser_initiated;
  };

  // Pending primary main frame cross-document navigations, in the order they
  // started. The page node reflects the most recently started one.
  std::vector<PendingNavigation> pending_navigations_;

  raw_ptr<const base::TickClock> tick_clock_ =
      base::DefaultTickClock::GetInstance();

  // When the feature
  // `kUseLoadingStateToDetectBackgroundTitleOrFaviconUpdate` is disabled,
  // PerformanceManagerTabHelper ignores the first title/favicon update after a
  // navigation to avoid treating initial-load churn as background activity.
  //
  // TODO(crbug.com/497577319): Remove these fields when
  // `kUseLoadingStateToDetectBackgroundTitleOrFaviconUpdate` is removed.
  bool first_time_favicon_set_ = false;
  bool first_time_title_set_ = false;

  // Maps from RenderFrameHost to the associated PM node. This is a single
  // map across all pages associated with this WebContents.
  std::map<content::RenderFrameHost*, std::unique_ptr<FrameNodeImpl>> frames_;

  // Subscription to current main frame's notification permission status. May be
  // null.
  std::unique_ptr<content::PermissionController::PermissionSubscription>
      permission_controller_subscription_;

  raw_ptr<DestructionObserver> destruction_observer_ = nullptr;
  base::ObserverList<Observer,
                     true,
                     base::ObserverListReentrancyPolicy::kDisallowReentrancy>
      observers_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_TAB_HELPER_H_
