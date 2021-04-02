// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/ad_tagging_browser_test_utils.h"

#include <string>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

content::RenderFrameHost* CreateFrameImpl(
    const content::ToRenderFrameHost& adapter,
    const GURL& url,
    bool ad_script) {
  content::RenderFrameHost* rfh = adapter.render_frame_host();
  std::string name = GetUniqueFrameName();
  std::string script = base::StringPrintf(
      "%s('%s','%s');", ad_script ? "createAdFrame" : "createFrame",
      url.spec().c_str(), name.c_str());
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  content::TestNavigationObserver navigation_observer(web_contents, 1);
  EXPECT_TRUE(content::ExecuteScript(rfh, script));
  navigation_observer.Wait();
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded())
      << navigation_observer.last_net_error_code();
  return content::FrameMatchingPredicate(
      web_contents, base::BindRepeating(&content::FrameMatchesName, name));
}

}  // namespace

std::string GetUniqueFrameName() {
  static uint32_t frame_count = 0;
  return base::StringPrintf("frame_%d", frame_count++);
}

content::RenderFrameHost* CreateSrcFrameFromAdScript(
    const content::ToRenderFrameHost& adapter,
    const GURL& url) {
  return CreateFrameImpl(adapter, url, true /* ad_script */);
}

content::RenderFrameHost* CreateSrcFrame(
    const content::ToRenderFrameHost& adapter,
    const GURL& url) {
  return CreateFrameImpl(adapter, url, false /* ad_script */);
}

void ExpectFrameAdEvidence(
    content::RenderFrameHost* frame_host,
    bool parent_is_ad,
    blink::mojom::FilterListResult filter_list_result,
    blink::mojom::FrameCreationStackEvidence created_by_ad_script) {
  ExpectFrameAdEvidence(frame_host, parent_is_ad, filter_list_result,
                        filter_list_result, created_by_ad_script);
}

void ExpectFrameAdEvidence(
    content::RenderFrameHost* frame_host,
    bool parent_is_ad,
    blink::mojom::FilterListResult latest_filter_list_result,
    blink::mojom::FilterListResult most_restrictive_filter_list_result,
    blink::mojom::FrameCreationStackEvidence created_by_ad_script) {
  auto* throttle_manager =
      ContentSubresourceFilterThrottleManager::FromWebContents(
          content::WebContents::FromRenderFrameHost(frame_host));
  base::Optional<blink::FrameAdEvidence> ad_evidence =
      throttle_manager->GetAdEvidenceForFrame(frame_host);
  ASSERT_TRUE(ad_evidence.has_value());
  EXPECT_TRUE(ad_evidence->is_complete());
  EXPECT_EQ(ad_evidence->parent_is_ad(), parent_is_ad);
  EXPECT_EQ(ad_evidence->latest_filter_list_result(),
            latest_filter_list_result);
  EXPECT_EQ(ad_evidence->most_restrictive_filter_list_result(),
            most_restrictive_filter_list_result);
  EXPECT_EQ(ad_evidence->created_by_ad_script(), created_by_ad_script);
}

}  // namespace subresource_filter
