// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/navigation_capturing_information_forwarder.h"

#include "chrome/browser/ui/web_applications/navigation_capturing_navigation_handle_user_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/window_open_disposition.h"

namespace web_app {

NavigationCapturingInformationForwarder::
    ~NavigationCapturingInformationForwarder() = default;

NavigationCapturingInformationForwarder::
    NavigationCapturingInformationForwarder(
        content::WebContents* contents,
        NavigationCapturingRedirectionInfo redirection_info,
        std::optional<webapps::AppId> launched_app_id,
        bool force_iph_off)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<NavigationCapturingInformationForwarder>(
          *contents),
      redirection_info_(std::move(redirection_info)),
      launched_app_id_(std::move(launched_app_id)),
      force_iph_off_(force_iph_off) {}

void NavigationCapturingInformationForwarder::SelfDestruct() {
  GetWebContents().RemoveUserData(UserDataKey());
}

void NavigationCapturingInformationForwarder::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  web_app::NavigationCapturingNavigationHandleUserData::
      CreateForNavigationHandle(*navigation_handle,
                                std::move(redirection_info_),
                                std::move(launched_app_id_), force_iph_off_);
  SelfDestruct();
}

void NavigationCapturingInformationForwarder::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  SelfDestruct();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(NavigationCapturingInformationForwarder);

}  // namespace web_app
