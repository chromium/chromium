// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_subresource_filter_web_contents_helper.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/not_fatal_until.h"
#include "base/supports_user_data.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/shared/browser/utils.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "content/public/browser/frame_type.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/features.h"

namespace subresource_filter {

namespace {

bool WillCreateNewThrottleManager(content::NavigationHandle& handle) {
  return IsInSubresourceFilterRoot(&handle) && !handle.IsSameDocument() &&
         !handle.IsPageActivation();
}

// A small container for holding a ContentSubresourceFilterThrottleManager
// while it's owned by a NavigationHandle. We need this container since
// base::SupportsUserData cannot relinquish ownership and we need to transfer
// the throttle manager to Page. When that happens, we remove the inner pointer
// from this class and transfer that to Page, leaving this empty container to
// be destroyed with NavigationHandle.
// TODO(bokan): Ideally this would be provided by a //content API and this
// class will eventually be removed. See the TODO in the class comment in the
// header file.
class ThrottleManagerInUserDataContainer
    : public content::NavigationHandleUserData<
          ThrottleManagerInUserDataContainer> {
 public:
  explicit ThrottleManagerInUserDataContainer(
      content::NavigationHandle&,
      std::unique_ptr<ContentSubresourceFilterThrottleManager> throttle_manager)
      : throttle_manager_(std::move(throttle_manager)) {}
  ~ThrottleManagerInUserDataContainer() override = default;

  std::unique_ptr<ContentSubresourceFilterThrottleManager> Take() {
    return std::move(throttle_manager_);
  }

  ContentSubresourceFilterThrottleManager* Get() {
    return throttle_manager_.get();
  }

 private:
  friend NavigationHandleUserData;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();

  std::unique_ptr<ContentSubresourceFilterThrottleManager> throttle_manager_;
};

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(ThrottleManagerInUserDataContainer);

}  // namespace

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContentSubresourceFilterWebContentsHelper);

//  static
void ContentSubresourceFilterWebContentsHelper::CreateForWebContents(
    content::WebContents* web_contents,
    SubresourceFilterProfileContext* profile_context,
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    VerifiedRulesetDealer::Handle* dealer_handle) {
  if (!base::FeatureList::IsEnabled(kSafeBrowsingSubresourceFilter))
    return;

  if (FromWebContents(web_contents))
    return;

  content::WebContentsUserData<ContentSubresourceFilterWebContentsHelper>::
      CreateForWebContents(web_contents, profile_context, database_manager,
                           dealer_handle);
}

//  static
ContentSubresourceFilterWebContentsHelper*
ContentSubresourceFilterWebContentsHelper::FromPage(content::Page& page) {
  return FromWebContents(
      content::WebContents::FromRenderFrameHost(&page.GetMainDocument()));
}

ContentSubresourceFilterWebContentsHelper::
    ContentSubresourceFilterWebContentsHelper(
        content::WebContents* web_contents,
        SubresourceFilterProfileContext* profile_context,
        scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
            database_manager,
        VerifiedRulesetDealer::Handle* dealer_handle)
    : content::WebContentsUserData<ContentSubresourceFilterWebContentsHelper>(
          *web_contents),
      content::WebContentsObserver(web_contents),
      profile_context_(profile_context),
      database_manager_(database_manager),
      dealer_handle_(dealer_handle) {
  SubresourceFilterObserverManager::CreateForWebContents(web_contents);
  scoped_observation_.Observe(
      SubresourceFilterObserverManager::FromWebContents(web_contents));
}

ContentSubresourceFilterWebContentsHelper::
    ~ContentSubresourceFilterWebContentsHelper() = default;

// static
ContentSubresourceFilterThrottleManager*
ContentSubresourceFilterWebContentsHelper::GetThrottleManager(
    content::NavigationHandle& handle) {
  // We should never be requesting the throttle manager for a navigation that
  // moves a page into the primary frame tree (e.g. prerender activation,
  // BFCache restoration).
  CHECK(!handle.IsPageActivation(), base::NotFatalUntil::M129);

  if (WillCreateNewThrottleManager(handle)) {
    auto* container =
        ThrottleManagerInUserDataContainer::GetForNavigationHandle(handle);
    if (!container)
      return nullptr;

    ContentSubresourceFilterThrottleManager* throttle_manager =
        container->Get();
    CHECK(throttle_manager, base::NotFatalUntil::M129);
    return throttle_manager;
  }

  // For a cross-document navigation, excluding page activation (this method
  // cannot be called for page activations), a throttle manager is created iff
  // it occurs in a non-fenced main frame. Since a throttle manager wasn't
  // created here, in the cross-document case, we must use the frame's
  // parent/outer-document RFH since subframe navigations are not associated
  // with a RFH until commit. We also use the parent here for same-document
  // non-root navigations to avoid rare issues with navigations that are aborted
  // due to a parent's navigation (where the navigation's handle's RFH may be
  // null); this does not affect the result as both frames have the same
  // throttle manager.
  CHECK(handle.IsSameDocument() || !IsInSubresourceFilterRoot(&handle),
        base::NotFatalUntil::M129);
  content::RenderFrameHost* rfh = IsInSubresourceFilterRoot(&handle)
                                      ? handle.GetRenderFrameHost()
                                      : handle.GetParentFrameOrOuterDocument();
  CHECK(rfh);
  return GetThrottleManager(GetSubresourceFilterRootPage(rfh));
}

// static
ContentSubresourceFilterThrottleManager*
ContentSubresourceFilterWebContentsHelper::GetThrottleManager(
    content::Page& page) {
  content::Page& filter_root_page =
      GetSubresourceFilterRootPage(&page.GetMainDocument());
  auto* throttle_manager =
      static_cast<ContentSubresourceFilterThrottleManager*>(
          filter_root_page.GetUserData(
              &ContentSubresourceFilterThrottleManager::kUserDataKey));
  return throttle_manager;
}

void ContentSubresourceFilterWebContentsHelper::SetDatabaseManagerForTesting(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
        database_manager) {
  database_manager_ = std::move(database_manager);
}

void ContentSubresourceFilterWebContentsHelper::WillDestroyThrottleManager(
    ContentSubresourceFilterThrottleManager* throttle_manager) {
  bool was_erased = throttle_managers_.erase(throttle_manager);
  CHECK(was_erased, base::NotFatalUntil::M129);
}

void ContentSubresourceFilterWebContentsHelper::RenderFrameDeleted(
    content::RenderFrameHost* frame_host) {
  ContentSubresourceFilterThrottleManager* throttle_manager =
      GetThrottleManager(frame_host->GetPage());
  if (!throttle_manager)
    return;

  throttle_manager->RenderFrameDeleted(frame_host);
}

void ContentSubresourceFilterWebContentsHelper::FrameDeleted(
    content::FrameTreeNodeId frame_tree_node_id) {
  navigated_frames_.erase(frame_tree_node_id);

  // TODO(bokan): Not sure how to go from frame tree node id to a Page. The
  // frame is basically deleted so I think its RFH will be cleared and we can't
  // access frame trees from outside of content/. For now, since the frame tree
  // node id is global, we'll just call FrameDeleted on all the throttle
  // managers and it'll be a no-op for those that don't have this FTN id in
  // their state. A //content API would make this possible.
  for (ContentSubresourceFilterThrottleManager* it : throttle_managers_) {
    it->FrameDeleted(frame_tree_node_id);
  }
}

void ContentSubresourceFilterWebContentsHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!WillCreateNewThrottleManager(*navigation_handle))
    return;

  std::unique_ptr<ContentSubresourceFilterThrottleManager> new_manager =
      ContentSubresourceFilterThrottleManager::CreateForNewPage(
          profile_context_, database_manager_, dealer_handle_, *this,
          *navigation_handle);

  throttle_managers_.insert(new_manager.get());

  ThrottleManagerInUserDataContainer::CreateForNavigationHandle(
      *navigation_handle, std::move(new_manager));
}

void ContentSubresourceFilterWebContentsHelper::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsPrerenderedPageActivation() ||
      navigation_handle->IsServedFromBackForwardCache()) {
    return;
  }

  if (ContentSubresourceFilterThrottleManager* throttle_manager =
          GetThrottleManager(*navigation_handle)) {
    throttle_manager->ReadyToCommitInFrameNavigation(navigation_handle);
  }
}

void ContentSubresourceFilterWebContentsHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsPrerenderedPageActivation() ||
      navigation_handle->IsServedFromBackForwardCache()) {
    // TODO(bokan): Once the BFCache restoration navigation is made synchronous
    // like prerender activation we can remove this special case.
    if (!navigation_handle->HasCommitted()) {
      CHECK(navigation_handle->IsServedFromBackForwardCache(),
            base::NotFatalUntil::M129);
      return;
    }

    CHECK(navigation_handle->HasCommitted(), base::NotFatalUntil::M129);
    CHECK(navigation_handle->GetRenderFrameHost(), base::NotFatalUntil::M129);

    ContentSubresourceFilterThrottleManager* throttle_manager =
        GetThrottleManager(navigation_handle->GetRenderFrameHost()->GetPage());

    // TODO(crbug.com/40781366): This shouldn't be possible but, from
    // the investigation in https://crbug.com/1264667, this is likely a symptom
    // of navigating a detached WebContents so (very rarely) was causing
    // crashes.
    if (!throttle_manager) {
      return;
    }

    throttle_manager->DidBecomePrimaryPage();

    return;
  }

  ContentSubresourceFilterThrottleManager* throttle_manager =
      GetThrottleManager(*navigation_handle);

  // If the initial navigation doesn't commit - we'll attach the throttle
  // manager to the existing page in the frame.
  const bool is_initial_navigation =
      !navigation_handle->IsSameDocument() &&
      navigated_frames_.insert(navigation_handle->GetFrameTreeNodeId()).second;

  if (WillCreateNewThrottleManager(*navigation_handle)) {
    auto* container =
        ThrottleManagerInUserDataContainer::GetForNavigationHandle(
            *navigation_handle);

    // TODO(crbug.com/40781366): It is theoretically possible to start a
    // navigation in an unattached WebContents (so the WebContents doesn't yet
    // have any WebContentsHelpers such as this class) but attach it before a
    // navigation completes. If that happened we won't have a throttle manager
    // for the navigation. Not sure this would ever happen in real usage but it
    // does happen in some tests.
    if (!container) {
      return;
    }

    CHECK(throttle_manager, base::NotFatalUntil::M129);

    // If the navigation was successful it will have created a new page,
    // transfer the throttle manager to Page user data. If it failed, but it's
    // the first navigation in the frame, we should transfer it to the existing
    // Page since it won't have a throttle manager and will remain in the
    // frame. In all other cases, the throttle manager will be destroyed.
    content::Page* page = nullptr;
    if (navigation_handle->HasCommitted()) {
      page = &navigation_handle->GetRenderFrameHost()->GetPage();
    } else if (is_initial_navigation) {
      if (auto* rfh = content::RenderFrameHost::FromID(
              navigation_handle->GetPreviousRenderFrameHostId())) {
        // TODO(crbug.com/40781366): Ideally this should only happen on
        // the first navigation in a frame, however, in some cases we actually
        // attach this TabHelper after a navigation has occurred (possibly
        // before it has finished). See
        // https://groups.google.com/a/chromium.org/g/navigation-dev/c/cY5V-w-xPRM/m/uC1Nsg_KAwAJ.
        // CHECK(rfh->GetLastCommittedURL().is_empty() ||
        //       rfh->GetLastCommittedURL().IsAboutBlank());
        // CHECK(!GetThrottleManager(rfh->GetPage()));
        page = &rfh->GetPage();
      }
    }

    if (page) {
      std::unique_ptr<ContentSubresourceFilterThrottleManager>
          throttle_manager_user_data = container->Take();
      page->SetUserData(&ContentSubresourceFilterThrottleManager::kUserDataKey,
                        std::move(throttle_manager_user_data));
      throttle_manager->OnPageCreated(*page);
    }
  }

  // Call DidFinishInFrameNavigation on the throttle manager after performing
  // the transfer as that method assumes a Page already owns the throttle
  // manager (see the |opener_rfh| case in FilterForFinishedNavigation).
  if (throttle_manager) {
    throttle_manager->DidFinishInFrameNavigation(navigation_handle,
                                                 is_initial_navigation);
  }
}

void ContentSubresourceFilterWebContentsHelper::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (ContentSubresourceFilterThrottleManager* throttle_manager =
          GetThrottleManager(render_frame_host->GetPage())) {
    throttle_manager->DidFinishLoad(render_frame_host, validated_url);
  }
}

void ContentSubresourceFilterWebContentsHelper::OnSubresourceFilterGoingAway() {
  // Stop observing here because the observer manager could be destroyed by the
  // time this class is destroyed.
  CHECK(scoped_observation_.IsObserving(), base::NotFatalUntil::M129);
  scoped_observation_.Reset();
}

void ContentSubresourceFilterWebContentsHelper::OnPageActivationComputed(
    content::NavigationHandle* navigation_handle,
    const mojom::ActivationState& activation_state) {
  if (ContentSubresourceFilterThrottleManager* throttle_manager =
          GetThrottleManager(*navigation_handle)) {
    throttle_manager->OnPageActivationComputed(navigation_handle,
                                               activation_state);
  }
}

void ContentSubresourceFilterWebContentsHelper::OnChildFrameNavigationEvaluated(
    content::NavigationHandle* navigation_handle,
    LoadPolicy load_policy) {
  CHECK(!IsInSubresourceFilterRoot(navigation_handle),
        base::NotFatalUntil::M129);
  if (ContentSubresourceFilterThrottleManager* throttle_manager =
          GetThrottleManager(*navigation_handle)) {
    throttle_manager->OnChildFrameNavigationEvaluated(navigation_handle,
                                                      load_policy);
  }
}

}  // namespace subresource_filter
