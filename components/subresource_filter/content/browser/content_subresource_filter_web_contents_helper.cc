// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_subresource_filter_web_contents_helper.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "base/supports_user_data.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
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

// =============================================================================
// TODO(bokan): Temporary to help debug crash in crbug.com/1264667.

DebugCrashWebContentsObserver::CrashKeyData::CrashKeyData() = default;
DebugCrashWebContentsObserver::CrashKeyData::~CrashKeyData() = default;
DebugCrashWebContentsObserver::CrashKeyData::CrashKeyData(const CrashKeyData&) =
    default;

DebugCrashWebContentsObserver::DebugCrashWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

DebugCrashWebContentsObserver::~DebugCrashWebContentsObserver() = default;

void DebugCrashWebContentsObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->GetParent()) {
    rfh_crash_data_.emplace(render_frame_host, CrashKeyData());
  }
}

void DebugCrashWebContentsObserver::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->GetParent()) {
    rfh_crash_data_.erase(render_frame_host);
  }
}

void DebugCrashWebContentsObserver::ReadyToCommitNavigation(
    content::NavigationHandle* handle) {
  if (!WillCreateNewThrottleManager(*handle))
    return;

  auto* rfh = handle->GetRenderFrameHost();
  DCHECK(rfh);

  auto itr = rfh_crash_data_.find(rfh);
  if (itr == rfh_crash_data_.end())
    return;

  CrashKeyData& data = itr->second;
  data.num_commits++;
  data.last_nav_had_manager =
      ThrottleManagerInUserDataContainer::GetForNavigationHandle(*handle);
}

void DebugCrashWebContentsObserver::DidFinishNonActivatingNavigation(
    content::NavigationHandle* handle,
    content::Page* page) {
  DCHECK(WillCreateNewThrottleManager(*handle));

  content::RenderFrameHost* rfh = nullptr;
  if (handle->HasCommitted()) {
    rfh = handle->GetRenderFrameHost();
  } else if (page) {
    rfh = &page->GetMainDocument();
  }

  // This navigation either didn't commit or wasn't the initial_document edge
  // case where we move the thottle manager even if we didn't commit. Outside
  // of these two cases, the navigation isn't interesting.
  if (!rfh)
    return;

  auto itr = rfh_crash_data_.find(rfh);
  if (itr == rfh_crash_data_.end())
    return;

  CrashKeyData& data = itr->second;
  data.page_at_last_nav_finish = page ? page->GetWeakPtr() : nullptr;
  data.last_nav_manager_moved_to_page =
      page &&
      page->GetUserData(&ContentSubresourceFilterThrottleManager::kUserDataKey);
}

void DebugCrashWebContentsObserver::RenderFrameHostStateChanged(
    content::RenderFrameHost* render_frame_host,
    content::RenderFrameHost::LifecycleState old_state,
    content::RenderFrameHost::LifecycleState new_state) {
  if (render_frame_host->GetParent())
    return;

  if (new_state ==
      content::RenderFrameHost::LifecycleState::kInBackForwardCache) {
    auto itr = rfh_crash_data_.find(render_frame_host);
    if (itr == rfh_crash_data_.end())
      return;
    CrashKeyData& data = itr->second;
    data.entered_bf_cache = true;
    data.page_at_bf_cache_entry = render_frame_host->GetPage().GetWeakPtr();
  }
}

void DebugCrashWebContentsObserver::DidFinishPageActivatingNavigation(
    content::NavigationHandle* handle) {
  DCHECK(handle->HasCommitted());
  auto* rfh = handle->GetRenderFrameHost();
  DCHECK(rfh);
  DCHECK(!rfh->GetParent());

  auto itr = rfh_crash_data_.find(rfh);
  if (itr == rfh_crash_data_.end())
    return;

  CrashKeyData& data = itr->second;
  data.is_bfcache_restore = handle->IsServedFromBackForwardCache();
  data.page_at_restore = rfh->GetPage().GetWeakPtr();
}

void DebugCrashWebContentsObserver::GetScopedCrashKeyStrings(
    content::RenderFrameHost* rfh,
    std::vector<std::unique_ptr<crash_reporter::ScopedCrashKeyString>>& keys) {
  constexpr size_t kCrashKeyBufferSize = 32;
  static crash_reporter::CrashKeyString<kCrashKeyBufferSize> crash_key_1(
      "sf-crash-number-commits");
  static crash_reporter::CrashKeyString<kCrashKeyBufferSize> crash_key_2(
      "sf-crash-had-manager");
  static crash_reporter::CrashKeyString<kCrashKeyBufferSize> crash_key_3(
      "sf-crash-moved-to-page");
  static crash_reporter::CrashKeyString<kCrashKeyBufferSize> crash_key_4(
      "sf-crash-entered-bfcache");
  static crash_reporter::CrashKeyString<kCrashKeyBufferSize> crash_key_5(
      "sf-crash-bfcache-restore");
  static crash_reporter::CrashKeyString<kCrashKeyBufferSize> crash_key_6(
      "sf-crash-page-nav-finish");
  static crash_reporter::CrashKeyString<kCrashKeyBufferSize> crash_key_7(
      "sf-crash-page-bfcache-store");
  static crash_reporter::CrashKeyString<kCrashKeyBufferSize> crash_key_8(
      "sf-crash-page-bfcache-restore");
  static crash_reporter::CrashKeyString<kCrashKeyBufferSize> crash_key_9(
      "sf-crash-has-crash-data");

  auto itr = rfh_crash_data_.find(rfh);
  if (itr == rfh_crash_data_.end()) {
    keys.push_back(std::make_unique<crash_reporter::ScopedCrashKeyString>(
        &crash_key_9, "NO"));
    return;
  }

  CrashKeyData& data = itr->second;

  keys.push_back(std::make_unique<crash_reporter::ScopedCrashKeyString>(
      &crash_key_1, base::StringPrintf("%d", data.num_commits)));
  keys.push_back(std::make_unique<crash_reporter::ScopedCrashKeyString>(
      &crash_key_2, base::StringPrintf("%d", data.last_nav_had_manager)));
  keys.push_back(std::make_unique<crash_reporter::ScopedCrashKeyString>(
      &crash_key_3,
      base::StringPrintf("%d", data.last_nav_manager_moved_to_page)));
  keys.push_back(std::make_unique<crash_reporter::ScopedCrashKeyString>(
      &crash_key_4, base::StringPrintf("%d", data.entered_bf_cache)));
  keys.push_back(std::make_unique<crash_reporter::ScopedCrashKeyString>(
      &crash_key_5, base::StringPrintf("%d", data.is_bfcache_restore)));
  keys.push_back(std::make_unique<crash_reporter::ScopedCrashKeyString>(
      &crash_key_6,
      base::StringPrintf("%p", data.page_at_last_nav_finish.get())));
  keys.push_back(std::make_unique<crash_reporter::ScopedCrashKeyString>(
      &crash_key_7,
      base::StringPrintf("%p", data.page_at_bf_cache_entry.get())));
  keys.push_back(std::make_unique<crash_reporter::ScopedCrashKeyString>(
      &crash_key_8, base::StringPrintf("%p", data.page_at_restore.get())));
  keys.push_back(std::make_unique<crash_reporter::ScopedCrashKeyString>(
      &crash_key_9, "YES"));
}

// End temporary debugging for crbug.com/1264667.
// =============================================================================

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
      debug_crash_observer_(web_contents),
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
  DCHECK(!handle.IsPageActivation());

  if (WillCreateNewThrottleManager(handle)) {
    auto* container =
        ThrottleManagerInUserDataContainer::GetForNavigationHandle(handle);
    if (!container)
      return nullptr;

    ContentSubresourceFilterThrottleManager* throttle_manager =
        container->Get();
    DCHECK(throttle_manager);
    return throttle_manager;
  }

  // For navigations that are not page activations (this method cannot be
  // called for page activations) nor same-document, a throttle manager will be
  // created iff they occur in a non-fenced-frame main frame. Since we didn't
  // create a throttle manager here, for the non-same-document case use the
  // frame's parent/outer-document RFH since subframe navigations are not
  // associated with a RFH until commit.
  DCHECK(handle.IsSameDocument() || !IsInSubresourceFilterRoot(&handle));
  content::RenderFrameHost* rfh = handle.IsSameDocument()
                                      ? handle.GetRenderFrameHost()
                                      : handle.GetParentFrameOrOuterDocument();
  DCHECK(rfh);
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
  DCHECK(was_erased);
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
    int frame_tree_node_id) {
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
      DCHECK(navigation_handle->IsServedFromBackForwardCache());
      return;
    }

    DCHECK(navigation_handle->HasCommitted());
    DCHECK(navigation_handle->GetRenderFrameHost());

    debug_crash_observer_.DidFinishPageActivatingNavigation(navigation_handle);

    std::vector<std::unique_ptr<crash_reporter::ScopedCrashKeyString>>
        scoped_crash_keys;
    debug_crash_observer_.GetScopedCrashKeyStrings(
        navigation_handle->GetRenderFrameHost(), scoped_crash_keys);

    ContentSubresourceFilterThrottleManager* throttle_manager =
        GetThrottleManager(navigation_handle->GetRenderFrameHost()->GetPage());
    // This should be non-null since every fully loaded page should have a
    // throttle manager and only fully loaded pages can be in the BFCache.
    // TODO(bokan): Temporary to help debug crash in crbug.com/1264667.
    CHECK(throttle_manager);
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

    // It is theoretically possible to start a navigation in an unattached
    // WebContents (so the WebContents doesn't yet have any WebContentsHelpers
    // such as this class) but attach it before a navigation completes. If that
    // happened we won't have a throttle manager for the navigation. Not sure
    // this would ever happen in real usage but it does happen in some tests.
    if (!container) {
      debug_crash_observer_.DidFinishNonActivatingNavigation(navigation_handle,
                                                             nullptr);
      return;
    }

    DCHECK(throttle_manager);

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
        // TODO(bokan): Ideally this should only happen on the first navigation
        // in a frame, however, in some cases we actually attach this TabHelper
        // after a navigation has occurred (possibly before it has finished).
        // See
        // https://groups.google.com/a/chromium.org/g/navigation-dev/c/cY5V-w-xPRM/m/uC1Nsg_KAwAJ.
        // DCHECK(rfh->GetLastCommittedURL().is_empty() ||
        //        rfh->GetLastCommittedURL().IsAboutBlank());
        DCHECK(!GetThrottleManager(rfh->GetPage()));
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

    debug_crash_observer_.DidFinishNonActivatingNavigation(navigation_handle,
                                                           page);
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
  DCHECK(scoped_observation_.IsObserving());
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
  DCHECK(!IsInSubresourceFilterRoot(navigation_handle));
  if (ContentSubresourceFilterThrottleManager* throttle_manager =
          GetThrottleManager(*navigation_handle)) {
    throttle_manager->OnChildFrameNavigationEvaluated(navigation_handle,
                                                      load_policy);
  }
}

bool IsInSubresourceFilterRoot(content::NavigationHandle* navigation_handle) {
  // TODO(bokan): This should eventually consider Portals. crbug.com/1267506.
  switch (navigation_handle->GetNavigatingFrameType()) {
    case content::FrameType::kPrimaryMainFrame:
    case content::FrameType::kPrerenderMainFrame:
      return true;
    case content::FrameType::kSubframe:
    case content::FrameType::kFencedFrameRoot:
      return false;
  }
}

bool IsSubresourceFilterRoot(content::RenderFrameHost* rfh) {
  return !rfh->GetParent() && !rfh->IsFencedFrameRoot();
}

content::Page& GetSubresourceFilterRootPage(content::RenderFrameHost* rfh) {
  // This only "breaks out" from fenced frames since the desired behavior in
  // other nested frame trees (e.g. portals) isn't clear. Otherwise we could
  // just use GetOutermostMainFrame.
  while (rfh->IsNestedWithinFencedFrame()) {
    rfh = rfh->GetMainFrame()->GetParentOrOuterDocument();
    DCHECK(rfh);
  }

  return rfh->GetPage();
}

}  // namespace subresource_filter
