// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_UNSAFE_RESOURCE_UTIL_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_UNSAFE_RESOURCE_UTIL_H_

#include "base/callback.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/global_routing_id.h"

namespace content {
class NavigationEntry;
class WebContents;
}  // namespace content

namespace security_interstitials {

// Returns the NavigationEntry for |resource| (for a main frame hit) or
// for the page which contains this resource (for a subresource hit).
// This method must only be called while the UnsafeResource is still
// "valid".
// I.e,
//   For MainPageLoadBlocked resources, it must not be called if the load
//   was aborted (going back or replaced with a different navigation),
//   or resumed (proceeded through warning or matched whitelist).
//   For non-MainPageLoadBlocked resources, it must not be called if any
//   other navigation has committed (whether by going back or unrelated
//   navigations), though a pending navigation is okay.
content::NavigationEntry* GetNavigationEntryForResource(
    const UnsafeResource& resource);

// Builds a getter for WebContents* from the given render frame id.
base::RepeatingCallback<content::WebContents*(void)> GetWebContentsGetter(
    int render_process_host_id,
    int render_frame_id);
base::RepeatingCallback<content::WebContents*(void)> GetWebContentsGetter(
    content::GlobalRenderFrameHostId render_frame_host_id);

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_UNSAFE_RESOURCE_UTIL_H_
