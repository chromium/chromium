// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_bypass/user_bypass_web_contents_observer.h"

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "url/gurl.h"

namespace user_bypass {

UserBypassWebContentsObserver::UserBypassWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<UserBypassWebContentsObserver>(
          *web_contents) {
  Profile* profile = Profile::FromBrowserContext(
      content::WebContentsObserver::web_contents()->GetBrowserContext());
  cookie_settings_ = CookieSettingsFactory::GetForProfile(profile);
}

UserBypassWebContentsObserver::~UserBypassWebContentsObserver() = default;

void UserBypassWebContentsObserver::LoadUserBypass(
    content::NavigationHandle* navigation_handle) {
  // Only set the blink runtime feature on top-level frames.
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  blink::RuntimeFeatureStateContext& context =
      navigation_handle->GetMutableRuntimeFeatureStateContext();

  // Enable blink runtime feature when User bypass is enabled.
  context.SetDisableThirdPartyStoragePartitioning2Enabled(
      cookie_settings_->IsStoragePartitioningBypassEnabled(
          navigation_handle->GetURL()));
}

void UserBypassWebContentsObserver::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  UserBypassWebContentsObserver::LoadUserBypass(navigation_handle);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(UserBypassWebContentsObserver);

}  // namespace user_bypass
