// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_COMMON_SUBRESOURCE_FILTER_UTILS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_COMMON_SUBRESOURCE_FILTER_UTILS_H_

#include <optional>

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"

class GURL;

namespace subresource_filter {

// Child frame navigations and initial root frame navigations matching these
// URLs/ schemes will not trigger ReadyToCommitNavigation in the browser
// process, so they must be treated specially to maintain activation. Each
// should inherit the activation of its parent in the case of a child frame and
// its opener in the case of a root frame. This also accounts for the ability of
// the parent/opener to affect the frame's content more directly, e.g. through
// document.write(), even though these URLs won't match a filter list rule by
// themselves.
bool ShouldInheritActivation(const GURL& url);

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

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_COMMON_SUBRESOURCE_FILTER_UTILS_H_
