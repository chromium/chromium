// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/ad_tagging_browser_test_utils.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
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
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  const bool is_prerender =
      rfh->GetLifecycleState() ==
      content::RenderFrameHost::LifecycleState::kPrerendering;
  std::string name = GetUniqueFrameName();

  if (is_prerender) {
    // TODO(bokan): We must avoid using a TestNavigationObserver if executing
    // this script on a prerendering RFH because this observer relies on
    // DidStartLoading, which PrerenderHost::PageHolder doesn't yet implement.
    // Instead we use a promise based version of the script and wait on that.
    // Once load events in prerender are clarified this can be resolved.
    // https://crbug.com/1199682.
    std::string script = base::StringPrintf(
        R"JS(
        (async () => {
            await %s('%s', '%s');
        })()
        )JS",
        ad_script ? "createAdFramePromise" : "createFramePromise",
        url.spec().c_str(), name.c_str());
    EXPECT_TRUE(content::ExecJs(rfh, script));
  } else {
    std::string script = base::StringPrintf(
        "%s('%s','%s');", ad_script ? "createAdFrame" : "createFrame",
        url.spec().c_str(), name.c_str());
    content::TestNavigationObserver navigation_observer(web_contents, 1);
    EXPECT_TRUE(content::ExecJs(rfh, script));
    navigation_observer.Wait();
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded())
        << navigation_observer.last_net_error_code();
  }

  return content::FrameMatchingPredicate(
      rfh->GetPage(), base::BindRepeating(&content::FrameMatchesName, name));
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

testing::AssertionResult EvidenceForFrameComprises(
    content::RenderFrameHost* frame_host,
    bool parent_is_ad,
    blink::mojom::FilterListResult filter_list_result,
    blink::mojom::FrameCreationStackEvidence created_by_ad_script) {
  return EvidenceForFrameComprises(frame_host, parent_is_ad, filter_list_result,
                                   filter_list_result, created_by_ad_script);
}

testing::AssertionResult EvidenceForFrameComprises(
    content::RenderFrameHost* frame_host,
    bool parent_is_ad,
    blink::mojom::FilterListResult latest_filter_list_result,
    blink::mojom::FilterListResult most_restrictive_filter_list_result,
    blink::mojom::FrameCreationStackEvidence created_by_ad_script) {
  auto* throttle_manager =
      ContentSubresourceFilterThrottleManager::FromPage(frame_host->GetPage());
  absl::optional<blink::FrameAdEvidence> ad_evidence =
      throttle_manager->GetAdEvidenceForFrame(frame_host);

  if (!ad_evidence.has_value())
    return testing::AssertionFailure() << "Expected ad evidence to exist.";
  if (!ad_evidence->is_complete())
    return testing::AssertionFailure() << "Expect ad evidence to be complete.";
  if (ad_evidence->parent_is_ad() != parent_is_ad) {
    return testing::AssertionFailure()
           << "Expected: " << parent_is_ad
           << " for parent_is_ad, actual: " << ad_evidence->parent_is_ad();
  }
  if (ad_evidence->latest_filter_list_result() != latest_filter_list_result) {
    return testing::AssertionFailure()
           << "Expected: " << latest_filter_list_result
           << " for latest_filter_list_result, actual: "
           << ad_evidence->latest_filter_list_result();
  }
  if (ad_evidence->most_restrictive_filter_list_result() !=
      most_restrictive_filter_list_result) {
    return testing::AssertionFailure()
           << "Expected: " << most_restrictive_filter_list_result
           << " for most_restrictive_filter_list_result, actual: "
           << ad_evidence->most_restrictive_filter_list_result();
  }
  if (ad_evidence->created_by_ad_script() != created_by_ad_script) {
    return testing::AssertionFailure() << "Expected: " << created_by_ad_script
                                       << " for created_by_ad_script, actual: "
                                       << ad_evidence->created_by_ad_script();
  }

  return testing::AssertionSuccess();
}

}  // namespace subresource_filter
