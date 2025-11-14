// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/performance_manager_tab_helper.h"

#include <memory>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "components/guest_view/buildflags/buildflags.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/performance_manager_registry_impl.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/render_process_user_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "components/guest_view/browser/guest_view_base.h"
#endif

namespace performance_manager {

namespace {

BASE_FEATURE(kEarlyPMVisibilityUpdate, base::FEATURE_ENABLED_BY_DEFAULT);

// Returns true if the opener relationship exists, false otherwise.
bool ConnectWindowOpenRelationshipIfExists(PerformanceManagerTabHelper* helper,
                                           content::WebContents* web_contents) {
  // Prefer to use GetOpener() if available, as it is more specific and points
  // directly to the frame that actually called window.open.
  auto* opener_rfh = web_contents->GetOpener();
  if (!opener_rfh) {
    // If the child page is opened with "noopener" then the parent document
    // maintains the ability to close the child, but the child can't reach back
    // and see it's parent. In this case there will be no "opener", but there
    // will be an "original opener".
    if (content::WebContents* original_opener_wc =
            web_contents->GetFirstWebContentsInLiveOriginalOpenerChain()) {
      opener_rfh = original_opener_wc->GetPrimaryMainFrame();
    }
  }

  if (!opener_rfh)
    return false;

  // Connect this new page to its opener.
  auto* opener_wc = content::WebContents::FromRenderFrameHost(opener_rfh);
  auto* opener_helper = PerformanceManagerTabHelper::FromWebContents(opener_wc);
  DCHECK(opener_helper);  // We should already have seen the opener WC.

  // On CrOS the opener can be the ChromeKeyboardWebContents, whose RFHs never
  // make it to a "created" state, so the PM never learns about them.
  // https://crbug.com/1090374
  auto* opener_frame_node = opener_helper->GetFrameNode(opener_rfh);
  if (!opener_frame_node)
    return false;

  helper->primary_page_node()->SetOpenerFrameNode(opener_frame_node);
  return true;
}

}  // namespace

PerformanceManagerTabHelper::PerformanceManagerTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PerformanceManagerTabHelper>(*web_contents) {
  // We have an early WebContents creation hook so should see it when there is
  // only a single frame. We sanity check that here.
#if DCHECK_IS_ON()
  size_t frame_count = 0;
  web_contents->ForEachRenderFrameHost(
      [&frame_count](content::RenderFrameHost* render_frame_host) {
        ++frame_count;
      });
  DCHECK_EQ(1u, frame_count);
#endif

  PagePropertyFlags initial_property_flags;
  if (web_contents->GetVisibility() == content::Visibility::VISIBLE) {
    initial_property_flags.Put(PagePropertyFlag::kIsVisible);
  }
  if (web_contents->IsCurrentlyAudible()) {
    initial_property_flags.Put(PagePropertyFlag::kIsAudible);
  }
  if (web_contents->HasPictureInPictureVideo() ||
      web_contents->HasPictureInPictureDocument()) {
    initial_property_flags.Put(PagePropertyFlag::kHasPictureInPicture);
  }
  if (web_contents->GetBrowserContext()->IsOffTheRecord()) {
    initial_property_flags.Put(PagePropertyFlag::kIsOffTheRecord);
  }

  // Create the page node.
  page_node_ = PerformanceManagerImpl::CreatePageNode(
      web_contents->GetWeakPtr(), web_contents->GetBrowserContext()->UniqueId(),
      web_contents->GetVisibleURL(), initial_property_flags,
      web_contents->GetLastActiveTimeTicks());

  // If the main frame was activated during WebContentsImpl::Init, we missed the
  // RenderFrameCreated notification, so synthesize it now.
  content::RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();
  CHECK(main_rfh);
  if (main_rfh->IsRenderFrameLive()) {
    RenderFrameCreated(main_rfh);
  }

  ConnectWindowOpenRelationshipIfExists(this, web_contents);
}

PerformanceManagerTabHelper::~PerformanceManagerTabHelper() {
  DCHECK(!page_node_);
  DCHECK(frames_.empty());
}

void PerformanceManagerTabHelper::TearDownAndSelfDelete() {
  // Remove the tab helper from the WCUD immediately. After TearDown the tab
  // helper is in an inconsistent state. This will prevent other
  // WCO::WebContentsDestroyed handlers from trying to access the tab helper in
  // this inconsistent state. Doing this before BatchDeleteNodes also prevents
  // accessors in the PerformanceManager class from finding the tab helper in an
  // inconsistent state if called from PageNodeObserver::OnPageNodeRemoved. The
  // tab helper will be deleted when `self` goes out of scope.
  std::unique_ptr<base::SupportsUserData::Data> self =
      web_contents()->TakeUserData(UserDataKey());

  // Ship our page and frame nodes to the PerformanceManagerImpl for
  // incineration.
  std::vector<std::unique_ptr<NodeBase>> nodes;
  for (auto& kv : frames_) {
    std::unique_ptr<FrameNodeImpl> frame_node = std::move(kv.second);

    // Notify observers.
    for (Observer& observer : observers_) {
      observer.OnBeforeFrameNodeRemoved(this, frame_node.get());
    }

    // Ensure the node will be deleted on the graph sequence.
    nodes.push_back(std::move(frame_node));
  }

  nodes.push_back(std::move(page_node_));

  frames_.clear();

  // Delete the page and its entire frame tree from the graph.
  PerformanceManagerImpl::BatchDeleteNodes(std::move(nodes));

  if (destruction_observer_) {
    destruction_observer_->OnPerformanceManagerTabHelperDestroying(
        web_contents());
  }

  MaybeUnsubscribeFromNotificationPermissionStatusChange(
      web_contents()->GetBrowserContext()->GetPermissionController());

  // Unsubscribe from the associated WebContents.
  Observe(nullptr);
}

void PerformanceManagerTabHelper::SetDestructionObserver(
    DestructionObserver* destruction_observer) {
  DCHECK(!destruction_observer || !destruction_observer_);
  destruction_observer_ = destruction_observer;
}

void PerformanceManagerTabHelper::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_NE(nullptr, render_frame_host);
  // This must not exist in the map yet.
  DCHECK(!base::Contains(frames_, render_frame_host));

  content::RenderFrameHost* parent = render_frame_host->GetParent();
  FrameNodeImpl* parent_frame_node = nullptr;
  // Get the outer document for a <fencedframe>, MPArch <webview>.
  FrameNodeImpl* outer_document_for_inner_frame_root = nullptr;
  if (parent) {
    parent_frame_node = GetExistingFrameNode(parent);
  } else if (render_frame_host->IsFencedFrameRoot()) {
    content::RenderFrameHost* outer_document =
        render_frame_host->GetParentOrOuterDocument();
    CHECK(outer_document);
    outer_document_for_inner_frame_root = GetExistingFrameNode(outer_document);
  }
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  else if (auto* guest = guest_view::GuestViewBase::FromRenderFrameHost(
               render_frame_host)) {
    if (base::FeatureList::IsEnabled(::features::kGuestViewMPArch)) {
      content::RenderFrameHost* outer_document = guest->owner_rfh();
      CHECK(outer_document);
      outer_document_for_inner_frame_root =
          GetExistingFrameNode(outer_document);
    }
  }
#endif

  // Ideally, creation would not be required here, but it is possible in tests
  // for the RenderProcessUserData to not have attached at this point.
  PerformanceManagerRegistryImpl::GetInstance()
      ->EnsureProcessNodeForRenderProcessHost(render_frame_host->GetProcess());

  auto* process_node = RenderProcessUserData::GetForRenderProcessHost(
                           render_frame_host->GetProcess())
                           ->process_node();

  auto* site_instance = render_frame_host->GetSiteInstance();

  // Create and initialize the frame node. This doesn't call `CreateFrameNode`
  // because that automatically calls GraphImpl::AddNewNode(), which notifies
  // observers, before the node is added to `frames_`.
  // TODO(crbug.com/40182881): Actually look up the appropriate page to wire
  // this frame up to!
  auto frame_node = std::make_unique<FrameNodeImpl>(
      process_node, page_node_.get(), parent_frame_node,
      outer_document_for_inner_frame_root, render_frame_host->GetRoutingID(),
      blink::LocalFrameToken(render_frame_host->GetFrameToken()),
      site_instance->GetBrowsingInstanceId(),
      site_instance->GetSiteInstanceGroupId(), render_frame_host->IsActive(),
      render_frame_host->IsActive());
  FrameNodeImpl* frame = frame_node.get();
  frames_[render_frame_host] = std::move(frame_node);
  PerformanceManagerImpl::GetGraphImpl()->AddNewNode(frame);

  GURL url = render_frame_host->GetLastCommittedURL();
  if (!url.is_empty()) {
    frame->OnNavigationCommitted(std::move(url),
                                 render_frame_host->GetLastCommittedOrigin(),
                                 /*same_document=*/false,
                                 /*is_served_from_back_forward_cache=*/false);
  }
}

void PerformanceManagerTabHelper::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  auto it = frames_.find(render_frame_host);
  CHECK(it != frames_.end());

  std::unique_ptr<FrameNodeImpl> frame_node = std::move(it->second);

  // Notify observers.
  for (Observer& observer : observers_)
    observer.OnBeforeFrameNodeRemoved(this, frame_node.get());

  // Then delete the node.
  PerformanceManagerImpl::DeleteNode(std::move(frame_node));
  frames_.erase(it);
}

void PerformanceManagerTabHelper::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  // |old_host| is null when a new frame tree position is being created and a
  // new frame is its first occupant.
  // Note that this notification can be received for a frame that hasn't yet
  // been created (i.e. old_host != null but GetFrameNode() == null). We can
  // safely ignore this. It would be nice to track those frames too, but since
  // they're not yet "created" we'd have no guarantee of seeing a corresponding
  // delete and the frames can be leaked.
  FrameNodeImpl* old_frame = old_host ? GetFrameNode(old_host) : nullptr;

  // It's entirely possible that this is the first time we're seeing this frame.
  // We'll eventually see a corresponding RenderFrameCreated if the frame ends
  // up actually being needed, so we can ignore it until that time. Artificially
  // creating the frame causes problems because we aren't guaranteed to see a
  // subsequent RenderFrameCreated call, meaning we won't see a
  // RenderFrameDeleted, and the frame node will be leaked until process tear
  // down.
  DCHECK(new_host);
  FrameNodeImpl* new_frame = GetFrameNode(new_host);
  if (!new_frame) {
    DCHECK(!new_host->IsRenderFrameLive())
        << "There shouldn't be a case where RenderFrameHostChanged is "
           "dispatched before RenderFrameCreated with a live RenderFrame\n";
  }
  // If neither frame could be looked up there's nothing to do.
  if (!old_frame && !new_frame) {
    return;
  }

  FrameNodeImpl::UpdateCurrentFrame(old_frame, new_frame,
                                    PerformanceManagerImpl::GetGraphImpl());
}

void PerformanceManagerTabHelper::RenderFrameHostStateChanged(
    content::RenderFrameHost* render_frame_host,
    content::RenderFrameHost::LifecycleState old_state,
    content::RenderFrameHost::LifecycleState new_state) {
  FrameNodeImpl* frame_node = GetFrameNode(render_frame_host);
  if (!frame_node) {
    return;
  }
  frame_node->SetIsActive(render_frame_host->IsActive());
}

void PerformanceManagerTabHelper::OnVisibilityWillChange(
    content::Visibility visibility) {
  // Under EarlyPMVisibilityUpdate, going from hidden to visible is forwarded
  // early so that process priority happens before any handler.
  if (visibility == content::Visibility::VISIBLE &&
      base::FeatureList::IsEnabled(kEarlyPMVisibilityUpdate)) {
    page_node_->SetIsVisible(true);
  }
}

void PerformanceManagerTabHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility != content::Visibility::VISIBLE ||
      !base::FeatureList::IsEnabled(kEarlyPMVisibilityUpdate)) {
    page_node_->SetIsVisible(visibility == content::Visibility::VISIBLE);
  }
}

void PerformanceManagerTabHelper::OnAudioStateChanged(bool audible) {
  page_node_->SetIsAudible(audible);
}

void PerformanceManagerTabHelper::OnFrameAudioStateChanged(
    content::RenderFrameHost* render_frame_host,
    bool is_audible) {
  // Ideally this would call `GetExistingFrameNode`, but it's possible to
  // receive a notification for an unknown frame.
  // TODO(crbug.com/40940232): Figure out how.
  FrameNodeImpl* frame_node = GetFrameNode(render_frame_host);
  if (!frame_node) {
    // We should only ever see this for a frame transitioning to *not* audible.
    DCHECK(!is_audible);
    return;
  }
  frame_node->SetIsAudible(is_audible);
}

void PerformanceManagerTabHelper::
    OnRemoteSubframeViewportIntersectionStateChanged(
        content::RenderFrameHost* render_frame_host,
        const blink::mojom::ViewportIntersectionState&
            viewport_intersection_state) {
  FrameNodeImpl* frame_node = GetFrameNode(render_frame_host);
  if (!frame_node) {
    // This can be invoked for a crashed RenderFrameHost, as its view still
    // occupies space on the page. Just ignore it as clearly its content is not
    // visible.
    CHECK(!render_frame_host->IsRenderFrameLive());
    return;
  }
  CHECK(render_frame_host->IsRenderFrameLive());

  bool is_intersecting_large_area = [&]() {
    const gfx::Rect& viewport_intersection =
        viewport_intersection_state.viewport_intersection;

    if (viewport_intersection.IsEmpty()) {
      return false;
    }

    int viewport_intersect_area =
        viewport_intersection.size().GetCheckedArea().ValueOrDefault(INT_MAX);
    int outermost_main_frame_area =
        viewport_intersection_state.outermost_main_frame_size.GetCheckedArea()
            .ValueOrDefault(INT_MAX);
    if (outermost_main_frame_area == 0) {
      return false;
    }
    float ratio = 1.0f * viewport_intersect_area / outermost_main_frame_area;
    const float ratio_threshold =
        blink::features::kLargeFrameSizePercentThreshold.Get() / 100.f;
    return ratio > ratio_threshold;
  }();

  frame_node->SetIsIntersectingLargeArea(is_intersecting_large_area);
}

void PerformanceManagerTabHelper::OnFrameVisibilityChanged(
    content::RenderFrameHost* render_frame_host,
    blink::mojom::FrameVisibility visibility) {
  FrameNodeImpl* frame_node = GetFrameNode(render_frame_host);
  if (!frame_node) {
    // This can be invoked for a crashed RenderFrameHost, as its view still
    // occupies space on the page. Just ignore it as clearly its content is not
    // visible.
    CHECK(!render_frame_host->IsRenderFrameLive());
    return;
  }
  CHECK(render_frame_host->IsRenderFrameLive());

  frame_node->SetIsRendered(visibility !=
                            blink::mojom::FrameVisibility::kNotRendered);

  ViewportIntersection viewport_intersection = [&]() {
    switch (visibility) {
      case blink::mojom::FrameVisibility::kNotRendered:
        return ViewportIntersection::kNotIntersecting;
      case blink::mojom::FrameVisibility::kRenderedOutOfViewport:
        if (!features::kRenderedOutOfViewIsNotVisible.Get()) {
          // Old, seemingly incorrect behavior. Treat an out of view frame as
          // intersecting with the viewport.
          return ViewportIntersection::kIntersecting;
        }
        return ViewportIntersection::kNotIntersecting;
      case blink::mojom::FrameVisibility::kRenderedInViewport:
        return ViewportIntersection::kIntersecting;
    }
    NOTREACHED();
  }();

  frame_node->SetViewportIntersection(viewport_intersection);
}

void PerformanceManagerTabHelper::OnFrameIsCapturingMediaStreamChanged(
    content::RenderFrameHost* render_frame_host,
    bool is_capturing_media_stream) {
  FrameNodeImpl* frame_node = GetFrameNode(render_frame_host);
  if (!frame_node) {
    // Ignore notifications that are received after the frame was deleted.
    return;
  }

  frame_node->SetIsCapturingMediaStream(is_capturing_media_stream);
}

void PerformanceManagerTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted())
    return;

  // Grab the current time up front, as this is as close as we'll get to the
  // original commit time.
  base::TimeTicks navigation_committed_time = base::TimeTicks::Now();

  // Find the associated frame node.
  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();
  FrameNodeImpl* frame_node = GetFrameNode(render_frame_host);
  if (!frame_node) {
    // TODO(siggi): Ideally this would call `GetExistingFrameNode`, but it seems
    //     it's possible to get a DidFinishNavigation notification for a deleted
    //     frame with the network service.
    return;
  }

  // Notify the frame of the committed URL.
  frame_node->OnNavigationCommitted(
      render_frame_host->GetLastCommittedURL(),
      render_frame_host->GetLastCommittedOrigin(),
      navigation_handle->IsSameDocument(),
      navigation_handle->IsServedFromBackForwardCache());

  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  // Make sure the hierarchical structure is constructed before sending signal
  // to the performance manager.
  OnMainFrameNavigation(navigation_handle->GetNavigationId());

  page_node_->OnMainFrameNavigationCommitted(
      navigation_handle->IsSameDocument(), navigation_committed_time,
      navigation_handle->GetNavigationId(),
      render_frame_host->GetLastCommittedURL(),
      navigation_handle->GetWebContents()->GetContentsMimeType(),
      GetNotificationPermissionStatusAndObserveChanges());
}

std::optional<blink::mojom::PermissionStatus> PerformanceManagerTabHelper::
    GetNotificationPermissionStatusAndObserveChanges() {
  // Don't get the content settings on android on each navigation because it may
  // induce scroll jank. There are many same-document navigations while
  // scrolling and getting the settings can invoke expensive platform APIs on
  // Android. Moreover, this information is only used to decide if a tab should
  // be discarded, which doesn't happen through Chrome code on that platform.
#if BUILDFLAG(IS_ANDROID)
  return std::nullopt;
#else
  content::PermissionController* permission_controller =
      web_contents()->GetBrowserContext()->GetPermissionController();
  if (!permission_controller) {
    CHECK(permission_controller_subscription_id_.is_null());
    return std::nullopt;
  }

  // Cancel previous change subscription.
  MaybeUnsubscribeFromNotificationPermissionStatusChange(permission_controller);

  // Create new change subscription.
  permission_controller_subscription_id_ =
      permission_controller->SubscribeToPermissionResultChange(
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  blink::PermissionType::NOTIFICATIONS),
          /*render_process_host=*/nullptr,
          web_contents()->GetPrimaryMainFrame(),
          url::Origin::Create(web_contents()->GetLastCommittedURL()).GetURL(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PerformanceManagerTabHelper::
                                  OnNotificationPermissionResultChange,
                              // Unretained is safe because the subscription
                              // is removed when `this` is deleted.
                              base::Unretained(this)));

  // Return current status.
  return permission_controller->GetPermissionStatusForCurrentDocument(
      content::PermissionDescriptorUtil::
          CreatePermissionDescriptorForPermissionType(
              blink::PermissionType::NOTIFICATIONS),
      web_contents()->GetPrimaryMainFrame());
#endif  // BUILDFLAG(IS_ANDROID)
}

void PerformanceManagerTabHelper::OnNotificationPermissionResultChange(
    content::PermissionResult permission_result) {
  page_node_->OnNotificationPermissionStatusChange(permission_result.status);
}

void PerformanceManagerTabHelper::
    MaybeUnsubscribeFromNotificationPermissionStatusChange(
        content::PermissionController* permission_controller) {
  if (permission_controller_subscription_id_.is_null()) {
    return;
  }

  CHECK(permission_controller);
  permission_controller->UnsubscribeFromPermissionResultChange(
      permission_controller_subscription_id_);
}

void PerformanceManagerTabHelper::FrameReceivedUserActivation(
    content::RenderFrameHost* render_frame_host) {
  FrameNodeImpl* frame_node = GetFrameNode(render_frame_host);
  if (!frame_node) {
    // Ignore notifications that are received after the frame was deleted.
    return;
  }

  frame_node->SetHadUserActivation();
}

void PerformanceManagerTabHelper::TitleWasSet(content::NavigationEntry* entry) {
  DCHECK(page_node_);

  // TODO(crbug.com/40894717): This logic belongs in the policy layer rather
  // than here. If a page has no <title> element on first load, the first change
  // of title will be ignored no matter much later it happens.
  if (!first_time_title_set_) {
    first_time_title_set_ = true;
    return;
  }
  page_node_->OnTitleUpdated();
}

void PerformanceManagerTabHelper::InnerWebContentsAttached(
    content::WebContents* inner_web_contents,
    content::RenderFrameHost* render_frame_host) {
  auto* helper = FromWebContents(inner_web_contents);
  CHECK(helper);
  auto* page = helper->page_node_.get();
  CHECK(page);
  auto* frame = GetFrameNode(render_frame_host);

  // For a guest view, the RFH should already have been seen.
  // Note that guest views can simultaneously have openers *and* be embedded.
  CHECK(frame);
  page->SetEmbedderFrameNode(frame);
}

void PerformanceManagerTabHelper::WebContentsDestroyed() {
  TearDownAndSelfDelete();
  // `this` is now invalid.
}

void PerformanceManagerTabHelper::DidUpdateFaviconURL(
    content::RenderFrameHost* render_frame_host,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  DCHECK(page_node_);

  // This favicon change might have been initiated by a different frame some
  // time ago and the main frame might have changed.
  if (!render_frame_host->IsActive())
    return;

  // TODO(crbug.com/40894717): This logic belongs in the policy layer rather
  // than here. If a page has no favicon on first load, the first change of
  // favicon will be ignored no matter much later it happens.
  if (!first_time_favicon_set_) {
    first_time_favicon_set_ = true;
    return;
  }
  page_node_->OnFaviconUpdated();
}

void PerformanceManagerTabHelper::MediaPictureInPictureChanged(
    bool is_picture_in_picture) {
  page_node_->SetHasPictureInPicture(is_picture_in_picture);
}

void PerformanceManagerTabHelper::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  page_node_->SetIsFocused(/*is_focused=*/true);
}

void PerformanceManagerTabHelper::OnWebContentsLostFocus(
    content::RenderWidgetHost* render_widget_host) {
  page_node_->SetIsFocused(/*is_focused=*/false);
}

void PerformanceManagerTabHelper::AboutToBeDiscarded(
    content::WebContents* new_contents) {
  DCHECK(page_node_);

  base::WeakPtr<PageNode> new_page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(new_contents);
  CHECK(new_page_node);
  page_node_->OnAboutToBeDiscarded(new_page_node);
}

void PerformanceManagerTabHelper::BindDocumentCoordinationUnit(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::DocumentCoordinationUnit> receiver) {
  auto* frame_node = GetExistingFrameNode(render_frame_host);
  frame_node->Bind(std::move(receiver));
}

FrameNodeImpl* PerformanceManagerTabHelper::GetFrameNode(
    content::RenderFrameHost* render_frame_host) const {
  auto it = frames_.find(render_frame_host);
  return it != frames_.end() ? it->second.get() : nullptr;
}

void PerformanceManagerTabHelper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PerformanceManagerTabHelper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PerformanceManagerTabHelper::OnMainFrameNavigation(int64_t navigation_id) {
  DCHECK(page_node_);

  ukm_source_id_ =
      ukm::ConvertToSourceId(navigation_id, ukm::SourceIdType::NAVIGATION_ID);
  page_node_->SetUkmSourceId(ukm_source_id_);

  first_time_title_set_ = false;
  first_time_favicon_set_ = false;
}

FrameNodeImpl* PerformanceManagerTabHelper::GetExistingFrameNode(
    content::RenderFrameHost* render_frame_host) const {
  FrameNodeImpl* frame_node = GetFrameNode(render_frame_host);
  CHECK(frame_node);
  return frame_node;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PerformanceManagerTabHelper);

}  // namespace performance_manager
