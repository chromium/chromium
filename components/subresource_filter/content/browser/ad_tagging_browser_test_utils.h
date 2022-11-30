// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_AD_TAGGING_BROWSER_TEST_UTILS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_AD_TAGGING_BROWSER_TEST_UTILS_H_

#include <string>

#include "third_party/blink/public/common/frame/frame_ad_evidence.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
class ToRenderFrameHost;
}  // namespace content

namespace testing {
class AssertionResult;
}  // namespace testing

namespace subresource_filter {

// Used for giving identifiers to frames that can easily be searched for
// with content::FrameMatchingPredicate.
std::string GetUniqueFrameName();

// Create a frame that navigates via the src attribute. It's created by ad
// script. Returns after navigation has completed.
content::RenderFrameHost* CreateSrcFrameFromAdScript(
    const content::ToRenderFrameHost& adapter,
    const GURL& url);

// Create a fencedframe that navigates via the src attribute. It's created by ad
// script. Returns after navigation has completed.
content::RenderFrameHost* CreateFencedFrameFromAdScript(
    const content::ToRenderFrameHost& adapter,
    const GURL& url);

// Create a frame that navigates via the src attribute. Returns after
// navigation has completed.
content::RenderFrameHost* CreateSrcFrame(
    const content::ToRenderFrameHost& adapter,
    const GURL& url);

// Create a fencedframe that navigates via the src attribute. Returns after
// navigation has completed.
content::RenderFrameHost* CreateFencedFrame(
    const content::ToRenderFrameHost& adapter,
    const GURL& url);

// Returns whether the ad evidence associated with the frame is composed of the
// provided values. The first signature assumes that the most restrictive and
// latest filter list results are the same.
::testing::AssertionResult EvidenceForFrameComprises(
    content::RenderFrameHost* frame_host,
    bool parent_is_ad,
    blink::mojom::FilterListResult filter_list_result,
    blink::mojom::FrameCreationStackEvidence created_by_ad_script);
::testing::AssertionResult EvidenceForFrameComprises(
    content::RenderFrameHost* frame_host,
    bool parent_is_ad,
    blink::mojom::FilterListResult latest_filter_list_result,
    blink::mojom::FilterListResult most_restrictive_filter_list_result,
    blink::mojom::FrameCreationStackEvidence created_by_ad_script);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_AD_TAGGING_BROWSER_TEST_UTILS_H_
