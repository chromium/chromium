// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/resource_usage_tab_helper.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "url/gurl.h"

namespace {
constexpr char kTestDomain[] = "http://foo.bar";
constexpr uint64_t kTestMemoryUsageBytes = 100000;
}  // namespace

class ResourceUsageTabHelperTest : public ChromeRenderViewHostTestHarness {
 protected:
  ResourceUsageTabHelper* InitializeTabHelper() {
    ResourceUsageTabHelper::CreateForWebContents(web_contents());
    return ResourceUsageTabHelper::FromWebContents(web_contents());
  }
};

// Return memory usage that was set on the tab helper.
TEST_F(ResourceUsageTabHelperTest, ReturnsMemoryUsageInBytes) {
  auto* tab_helper = InitializeTabHelper();
  tab_helper->SetMemoryUsageInBytes(kTestMemoryUsageBytes);
  EXPECT_EQ(tab_helper->GetMemoryUsageInBytes(), kTestMemoryUsageBytes);
}

// Clears memory usage on navigate.
TEST_F(ResourceUsageTabHelperTest, ClearsMemoryUsageOnNavigate) {
  auto* tab_helper = InitializeTabHelper();
  tab_helper->SetMemoryUsageInBytes(kTestMemoryUsageBytes);
  NavigateAndCommit(GURL(kTestDomain));
  EXPECT_EQ(tab_helper->GetMemoryUsageInBytes(), 0u);
}
