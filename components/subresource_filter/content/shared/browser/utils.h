// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_UTILS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_UTILS_H_

namespace content {
class NavigationHandle;
class RenderFrameHost;
class Page;
}  // namespace content

// See also //components/subresource_filter/content/shared/common/utils.h for
// related utils.

namespace subresource_filter {

// If |navigation_handle| is for a special url that did not go through the
// network stack or if the initial (attempted) load wasn't committed, the
// frame's activation will not have been set. It should instead be inherited
// from its same-origin opener (if any).
bool ShouldInheritOpenerActivation(content::NavigationHandle* navigation_handle,
                                   content::RenderFrameHost* frame_host);

// Checks that the navigation has a valid, committed parent navigation and is
// handled by the network stack.
bool ShouldInheritParentActivation(
    content::NavigationHandle* navigation_handle);

// Returns true if the navigation is happening in the main frame of a page
// considered a subresource filter root (i.e. one that may create a new
// ThrottleManager). These navigations are not themselves able to be filtered
// by the subresource filter.
bool IsInSubresourceFilterRoot(content::NavigationHandle* navigation_handle);

// Same as above but for RenderFrameHosts, returns true if the given
// RenderFrameHost is a subresource filter root.
bool IsSubresourceFilterRoot(content::RenderFrameHost* rfh);

// Gets the closest ancestor Page which is a subresource filter root, i.e. one
// for which we have created a throttle manager. Note: This crosses the fenced
// frame boundary (as they are considered a subresource filter child).
content::Page& GetSubresourceFilterRootPage(content::RenderFrameHost* rfh);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_UTILS_H_
