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
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom-forward.h"
#include "content/public/browser/permission_controller.h"
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

  // Must be invoked prior to detaching a PerformanceManagerTabHelper from its
  // WebContents.
  void TearDown();

  // WebContentsObserver overrides.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
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
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void FrameReceivedUserActivation(
      content::RenderFrameHost* render_frame_host) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void InnerWebContentsAttached(
      content::WebContents* inner_web_contents,
      content::RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;
  void DidUpdateFaviconURL(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override;
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
  FrameNodeImpl* GetFrameNode(content::RenderFrameHost* render_frame_host);

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

  // Returns the notification permission status for the current main frame and
  // subscribes to changes.
  std::optional<blink::mojom::PermissionStatus>
  GetNotificationPermissionStatusAndObserveChanges();

  // Callback invoked when the current main frame's notification permission
  // status changes.
  void OnNotificationPermissionStatusChange(
      blink::mojom::PermissionStatus permission_status);

  // Unsubscribe from changes to the current main frame's notification
  // permission status, or no-op if there is no subscription.
  void MaybeUnsubscribeFromNotificationPermissionStatusChange(
      content::PermissionController* permission_controller);

  // Returns the FrameNodeImpl* associated with `render_frame_host`. This
  // CHECKs that it exists.
  FrameNodeImpl* GetExistingFrameNode(
      content::RenderFrameHost* render_frame_host) const;

  // The actual page node.
  std::unique_ptr<PageNodeImpl> page_node_;

  // The UKM source ID for this page.
  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  // Favicon and title are set when a page is loaded, we only want to send
  // signals to the page node about title and favicon update from the previous
  // title and favicon, thus we want to ignore the very first update since it is
  // always supposed to happen.
  bool first_time_favicon_set_ = false;
  bool first_time_title_set_ = false;

  // Maps from RenderFrameHost to the associated PM node. This is a single
  // map across all pages associated with this WebContents.
  std::map<content::RenderFrameHost*, std::unique_ptr<FrameNodeImpl>> frames_;

  // Subscription to current main frame's notification permission status. May be
  // null.
  content::PermissionController::SubscriptionId
      permission_controller_subscription_id_;

  raw_ptr<DestructionObserver> destruction_observer_ = nullptr;
  base::ObserverList<Observer, true, false> observers_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_TAB_HELPER_H_
