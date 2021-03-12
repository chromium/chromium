// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_AD_TAGGING_BROWSER_TEST_UTILS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_AD_TAGGING_BROWSER_TEST_UTILS_H_

#include <string>

#include "url/gurl.h"

namespace content {
class RenderFrameHost;
class ToRenderFrameHost;
}  // namespace content

namespace subresource_filter {

// Used for giving identifiers to frames that can easily be searched for
// with content::FrameMatchingPredicate.
std::string GetUniqueFrameName();

// Create a frame that navigates via the src attribute. It's created by ad
// script. Returns after navigation has completed.
content::RenderFrameHost* CreateSrcFrameFromAdScript(
    const content::ToRenderFrameHost& adapter,
    const GURL& url);

// Create a frame that navigates via the src attribute. Returns after
// navigation has completed.
content::RenderFrameHost* CreateSrcFrame(
    const content::ToRenderFrameHost& adapter,
    const GURL& url);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_AD_TAGGING_BROWSER_TEST_UTILS_H_
