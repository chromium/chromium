// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/navigation_capturing_information_forwarder.h"

#include "chrome/browser/web_applications/navigation_capturing_navigation_handle_user_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/window_open_disposition.h"

namespace web_app {

NavigationCapturingInformationForwarder::
    ~NavigationCapturingInformationForwarder() = default;

NavigationCapturingInformationForwarder::
    NavigationCapturingInformationForwarder(content::WebContents* contents,
                                            WindowOpenDisposition disposition)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<NavigationCapturingInformationForwarder>(
          *contents),
      disposition_(disposition) {}

void NavigationCapturingInformationForwarder::SelfDestruct() {
  GetWebContents().RemoveUserData(UserDataKey());
}

void NavigationCapturingInformationForwarder::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  CHECK(disposition_ != WindowOpenDisposition::UNKNOWN);
  web_app::NavigationCapturingNavigationHandleUserData::
      CreateForNavigationHandle(*navigation_handle, disposition_);
  SelfDestruct();
}

void NavigationCapturingInformationForwarder::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  SelfDestruct();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(NavigationCapturingInformationForwarder);

}  // namespace web_app
