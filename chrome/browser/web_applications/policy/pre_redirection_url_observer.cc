// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/policy/pre_redirection_url_observer.h"

#include "content/public/browser/navigation_handle.h"
#include "ui/base/page_transition_types.h"

namespace webapps {

PreRedirectionURLObserver::PreRedirectionURLObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PreRedirectionURLObserver>(*web_contents) {}

void PreRedirectionURLObserver::DidFinishNavigation(
    content::NavigationHandle* handle) {
  DCHECK(handle);
  // Don't save subframe navigations.
  if (!handle->IsInPrimaryMainFrame())
    return;
  // Don't save same-document navigations, e.g. to #anchor links.
  if (handle->IsSameDocument())
    return;
  // Don't save navigations that were initiated by the document (e.g. javascript
  // redirections).
  if (handle->GetInitiatorFrameToken())
    return;
  // Server-side redirects are stored in the RedirectChain, get the initial URL:
  last_url_ = handle->GetRedirectChain()[0];
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PreRedirectionURLObserver);

}  // namespace webapps
