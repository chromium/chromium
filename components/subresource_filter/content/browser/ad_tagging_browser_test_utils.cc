// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/ad_tagging_browser_test_utils.h"

#include <string>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
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

}  // namespace subresource_filter
