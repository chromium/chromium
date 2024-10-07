// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_observer.h"
#include "components/fingerprinting_protection_filter/browser/throttle_manager.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/subresource_filter/content/shared/browser/utils.h"
#include "components/subresource_filter/core/browser/verified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace subresource_filter {
enum class ActivationDecision;
enum class LoadPolicy;
}  // namespace subresource_filter

namespace fingerprinting_protection_filter {
namespace {

using ::subresource_filter::GetSubresourceFilterRootPage;
using ::subresource_filter::IsInSubresourceFilterRoot;
using ::subresource_filter::VerifiedRulesetDealer;

bool WillCreateNewThrottleManager(content::NavigationHandle& handle) {
  return IsInSubresourceFilterRoot(&handle) && !handle.IsSameDocument() &&
         !handle.IsPageActivation();
}

// A small container for holding a
// fingerprinting_protection_filter::ThrottleManager while it's owned by a
// NavigationHandle. We need this container since base::SupportsUserData cannot
// relinquish ownership and we need to transfer the throttle manager to Page.
// When that happens, we remove the inner pointer from this class and transfer
// that to Page, leaving this empty container to be destroyed with
// NavigationHandle.
// TODO(bokan): Ideally this would be provided by a //content API and this
// class will eventually be removed. See the TODO in the class comment in the
// header file.
class ThrottleManagerInUserDataContainer
    : public content::NavigationHandleUserData<
          ThrottleManagerInUserDataContainer> {
 public:
  explicit ThrottleManagerInUserDataContainer(
      content::NavigationHandle&,
      std::unique_ptr<ThrottleManager> throttle_manager)
      : throttle_manager_(std::move(throttle_manager)) {}
  ~ThrottleManagerInUserDataContainer() override = default;

  std::unique_ptr<ThrottleManager> Take() {
    return std::move(throttle_manager_);
  }

  ThrottleManager* Get() { return throttle_manager_.get(); }

 private:
  friend NavigationHandleUserData;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();

  std::unique_ptr<ThrottleManager> throttle_manager_;
};

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(ThrottleManagerInUserDataContainer);

}  // namespace

// static
void FingerprintingProtectionWebContentsHelper::CreateForWebContents(
    content::WebContents* web_contents,
    PrefService* pref_service,
    privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
    VerifiedRulesetDealer::Handle* dealer_handle,
    bool is_incognito) {
  if (!features::IsFingerprintingProtectionEnabledForIncognitoState(is_incognito)) {
    return;
  }

  // Do nothing if a FingerprintingProtectionWebContentsHelper already exists
  // for the current WebContents.
  if (FromWebContents(web_contents)) {
    return;
  }

  content::WebContentsUserData<FingerprintingProtectionWebContentsHelper>::
      CreateForWebContents(web_contents, pref_service,
                           tracking_protection_settings, dealer_handle,
                           is_incognito);
}

// private
FingerprintingProtectionWebContentsHelper::
    FingerprintingProtectionWebContentsHelper(
        content::WebContents* web_contents,
        PrefService* pref_service,
        privacy_sandbox::TrackingProtectionSettings*
            tracking_protection_settings,
        VerifiedRulesetDealer::Handle* dealer_handle,
        bool is_incognito)
    : content::WebContentsUserData<FingerprintingProtectionWebContentsHelper>(
          *web_contents),
      content::WebContentsObserver(web_contents),
      pref_service_(pref_service),
      tracking_protection_settings_(tracking_protection_settings),
      dealer_handle_(dealer_handle),
      is_incognito_(is_incognito) {}

FingerprintingProtectionWebContentsHelper::
    ~FingerprintingProtectionWebContentsHelper() = default;

// static
ThrottleManager* FingerprintingProtectionWebContentsHelper::GetThrottleManager(
    content::Page& page) {
  content::Page& filter_root_page =
      GetSubresourceFilterRootPage(&page.GetMainDocument());
  auto* throttle_manager = static_cast<ThrottleManager*>(
      filter_root_page.GetUserData(&ThrottleManager::kUserDataKey));
  return throttle_manager;
}

// static
ThrottleManager* FingerprintingProtectionWebContentsHelper::GetThrottleManager(
    content::NavigationHandle& handle) {
  // We should never be requesting the throttle manager for a navigation that
  // moves a page into the primary frame tree (e.g. prerender activation,
  // BFCache restoration).
  CHECK(!handle.IsPageActivation());

  // TODO(https://crbug.com/40280666): Consider storing pointers to existing
  // throttle managers to enable short-circuiting this function in most cases.

  if (WillCreateNewThrottleManager(handle)) {
    auto* container =
        ThrottleManagerInUserDataContainer::GetForNavigationHandle(handle);
    if (!container) {
      return nullptr;
    }

    ThrottleManager* throttle_manager = container->Get();
    CHECK(throttle_manager);
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
  DCHECK(handle.IsSameDocument() || !IsInSubresourceFilterRoot(&handle));
  content::RenderFrameHost* rfh = IsInSubresourceFilterRoot(&handle)
                                      ? handle.GetRenderFrameHost()
                                      : handle.GetParentFrameOrOuterDocument();
  CHECK(rfh);
  return GetThrottleManager(GetSubresourceFilterRootPage(rfh));
}

void FingerprintingProtectionWebContentsHelper::WillDestroyThrottleManager(
    ThrottleManager* throttle_manager) {
  bool was_erased = throttle_managers_.erase(throttle_manager);
  CHECK(was_erased);
}

void FingerprintingProtectionWebContentsHelper::NotifyPageActivationComputed(
    content::NavigationHandle* navigation_handle,
    const subresource_filter::mojom::ActivationState& activation_state,
    const subresource_filter::ActivationDecision& activation_decision) {
  if (ThrottleManager* throttle_manager =
          GetThrottleManager(*navigation_handle)) {
    throttle_manager->OnPageActivationComputed(
        navigation_handle, activation_state, activation_decision);
  }
}

void FingerprintingProtectionWebContentsHelper::
    NotifyChildFrameNavigationEvaluated(
        content::NavigationHandle* navigation_handle,
        subresource_filter::LoadPolicy load_policy) {
  // TODO(https://crbug.com/40280666): Notify throttle manager after blink
  // communication is implemented.
  if (load_policy == subresource_filter::LoadPolicy::WOULD_DISALLOW ||
      load_policy == subresource_filter::LoadPolicy::DISALLOW) {
    if (ThrottleManager* throttle_manager =
            GetThrottleManager(*navigation_handle)) {
      throttle_manager->NotifyDisallowLoadPolicy(navigation_handle);
    }
  }
}

void FingerprintingProtectionWebContentsHelper::FrameDeleted(
    content::FrameTreeNodeId frame_tree_node_id) {
  navigated_frames_.erase(frame_tree_node_id);
}

void FingerprintingProtectionWebContentsHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!WillCreateNewThrottleManager(*navigation_handle)) {
    return;
  }

  std::unique_ptr<ThrottleManager> new_manager =
      ThrottleManager::CreateForNewPage(dealer_handle_.get(), *this,
                                        *navigation_handle, is_incognito_);

  throttle_managers_.insert(new_manager.get());

  ThrottleManagerInUserDataContainer::CreateForNavigationHandle(
      *navigation_handle, std::move(new_manager));
}

void FingerprintingProtectionWebContentsHelper::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsPrerenderedPageActivation() ||
      navigation_handle->IsServedFromBackForwardCache()) {
    return;
  }

  if (ThrottleManager* throttle_manager =
          GetThrottleManager(*navigation_handle)) {
    throttle_manager->ReadyToCommitInFrameNavigation(navigation_handle);
  }
}

void FingerprintingProtectionWebContentsHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->GetReloadType() != content::ReloadType::NONE) {
    refresh_count_++;
  }
  if (navigation_handle->IsPrerenderedPageActivation() ||
      navigation_handle->IsServedFromBackForwardCache()) {
    if (!navigation_handle->HasCommitted()) {
      CHECK(navigation_handle->IsServedFromBackForwardCache());
      return;
    }

    CHECK(navigation_handle->HasCommitted());
    CHECK(navigation_handle->GetRenderFrameHost());

    ThrottleManager* throttle_manager =
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

  ThrottleManager* throttle_manager = GetThrottleManager(*navigation_handle);

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

    CHECK(throttle_manager);

    // If the navigation was successful it will have created a new page;
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
        page = &rfh->GetPage();
      }
    }

    if (page) {
      std::unique_ptr<ThrottleManager> throttle_manager_user_data =
          container->Take();
      page->SetUserData(&ThrottleManager::kUserDataKey,
                        std::move(throttle_manager_user_data));
      throttle_manager->OnPageCreated(*page);
    }
  }

  // Call DidFinishInFrameNavigation on the throttle manager after performing
  // the transfer as that method assumes a Page already owns the throttle
  // manager (see the `opener_rfh` case in FilterForFinishedNavigation).
  if (throttle_manager) {
    throttle_manager->DidFinishInFrameNavigation(navigation_handle,
                                                 is_initial_navigation);
  }
}

void FingerprintingProtectionWebContentsHelper::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (ThrottleManager* throttle_manager =
          GetThrottleManager(render_frame_host->GetPage())) {
    throttle_manager->DidFinishLoad(render_frame_host, validated_url);
  }
}

void FingerprintingProtectionWebContentsHelper::NotifyOnBlockedResources() {
  is_subresource_blocked_ = true;
  for (auto& observer : observer_list_) {
    observer.OnSubresourceBlocked();
  }
}

void FingerprintingProtectionWebContentsHelper::WebContentsDestroyed() {
  // The user has closed the tab or otherwise destroyed the web contents. Flush
  // metrics.
  Detach();
}

void FingerprintingProtectionWebContentsHelper::Detach() {
  base::UmaHistogramCounts100(
      "FingerprintingProtection.WebContentsObserver.RefreshCount",
      refresh_count_);
  ukm::SourceId source_id =
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  ukm::builders::FingerprintingProtectionUsage(source_id)
      .SetRefreshCount(refresh_count_)
      .Record(ukm::UkmRecorder::Get());
}

void FingerprintingProtectionWebContentsHelper::AddObserver(
    FingerprintingProtectionObserver* observer) {
  observer_list_.AddObserver(observer);
}

void FingerprintingProtectionWebContentsHelper::RemoveObserver(
    FingerprintingProtectionObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FingerprintingProtectionWebContentsHelper);

}  // namespace fingerprinting_protection_filter
