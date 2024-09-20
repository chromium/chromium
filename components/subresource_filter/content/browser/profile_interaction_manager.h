// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_PROFILE_INTERACTION_MANAGER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_PROFILE_INTERACTION_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/subresource_filter/content/browser/safe_browsing_page_activation_throttle.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/subresource_filter/content/browser/ads_blocked_message_delegate.h"
#endif

namespace content {
class Page;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace content_settings {
class CookieSettings;
}  // namespace content_settings

namespace subresource_filter {

class SubresourceFilterProfileContext;

// Class that manages interaction between the per-navigation/per-page
// subresource filter objects (i.e., the throttles and throttle manager) and
// the per-profile objects (e.g., content settings).
class ProfileInteractionManager
    : public SafeBrowsingPageActivationThrottle::Delegate {
 public:
  explicit ProfileInteractionManager(
      SubresourceFilterProfileContext* profile_context);
  ~ProfileInteractionManager() override;

  ProfileInteractionManager(const ProfileInteractionManager&) = delete;
  ProfileInteractionManager& operator=(const ProfileInteractionManager&) =
      delete;

  base::WeakPtr<ProfileInteractionManager> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void DidCreatePage(content::Page& page);

  // Invoked when the user has requested a reload of a page with blocked ads
  // (e.g., via an infobar).
  void OnReloadRequested();

  // Invoked when an ads violation is triggered.
  void OnAdsViolationTriggered(content::RenderFrameHost* rfh,
                               mojom::AdsViolation triggered_violation);

  // Invoked when a notification should potentially be shown to the user that
  // ads are being blocked on this page. Will make the final determination as to
  // whether the notification should be shown. On Android this will show an
  // infobar if appropriate and if an infobar::ContentInfoBarManager instance
  // has been installed in web_contents() by the embedder.
  void MaybeShowNotification();

  // SafeBrowsingPageActivationThrottle::Delegate:
  mojom::ActivationLevel OnPageActivationComputed(
      content::NavigationHandle* navigation_handle,
      mojom::ActivationLevel initial_activation_level,
      ActivationDecision* decision) override;

#if BUILDFLAG(IS_ANDROID)
  AdsBlockedMessageDelegate* ads_blocked_message_delegate_for_testing() {
    return ads_blocked_message_delegate_;
  }
#endif

  content_settings::CookieSettings* GetCookieSettings();

 private:
  content::WebContents* GetWebContents();

  // Tracks the current page in the frame tree the owning
  // ContentSubresourceFilterThrottleManager is associated with. This will be
  // nullptr initially until the main frame navigation commits and a Page is
  // created, at which point the throttle manager will set this member.
  raw_ptr<content::Page> page_ = nullptr;

  // Unowned and must outlive this object.
  raw_ptr<SubresourceFilterProfileContext, DanglingUntriaged> profile_context_ =
      nullptr;

  bool ads_violation_triggered_for_last_committed_navigation_ = false;

#if BUILDFLAG(IS_ANDROID)
  raw_ptr<AdsBlockedMessageDelegate> ads_blocked_message_delegate_;
#endif

  base::WeakPtrFactory<ProfileInteractionManager> weak_ptr_factory_{this};
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_PROFILE_INTERACTION_MANAGER_H_
