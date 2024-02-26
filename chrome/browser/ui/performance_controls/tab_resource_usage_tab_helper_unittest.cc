// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "url/gurl.h"

namespace {
constexpr char kTestDomain[] = "http://foo.bar";
constexpr uint64_t kTestMemoryUsageBytes = 100000;
}  // namespace

class TabResourceUsageTabHelperTest : public ChromeRenderViewHostTestHarness {
 protected:
  TabResourceUsageTabHelper* InitializeTabHelper() {
    TabResourceUsageTabHelper::CreateForWebContents(
        web_contents());
    return TabResourceUsageTabHelper::
        FromWebContents(web_contents());
  }
};

// Return memory usage that was set on the tab helper.
TEST_F(TabResourceUsageTabHelperTest, ReturnsMemoryUsageInBytes) {
  auto* const tab_helper = InitializeTabHelper();
  tab_helper->SetMemoryUsageInBytes(kTestMemoryUsageBytes);
  EXPECT_EQ(tab_helper->GetMemoryUsageInBytes(), kTestMemoryUsageBytes);
}

// Clears memory usage on navigate.
TEST_F(TabResourceUsageTabHelperTest, ClearsMemoryUsageOnNavigate) {
  auto* const tab_helper = InitializeTabHelper();
  tab_helper->SetMemoryUsageInBytes(kTestMemoryUsageBytes);
  NavigateAndCommit(GURL(kTestDomain));
  EXPECT_EQ(tab_helper->GetMemoryUsageInBytes(), 0u);
}

// Correctly reports whether memory usage is high after memory usage is set.
TEST_F(TabResourceUsageTabHelperTest, HighMemoryUsage) {
  auto* const tab_helper = InitializeTabHelper();
  uint64_t const high_memory_usage_threshold =
      TabResourceUsage::kHighMemoryUsageThresholdBytes;
  tab_helper->SetMemoryUsageInBytes(high_memory_usage_threshold);
  EXPECT_FALSE(tab_helper->resource_usage()->is_high_memory_usage());
  tab_helper->SetMemoryUsageInBytes(high_memory_usage_threshold + 1);
  EXPECT_TRUE(tab_helper->resource_usage()->is_high_memory_usage());
}
