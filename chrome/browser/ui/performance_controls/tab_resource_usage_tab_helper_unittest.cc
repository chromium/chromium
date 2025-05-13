// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "url/gurl.h"

namespace {
constexpr uint64_t kTestMemoryUsageBytes = 100000;
}  // namespace

class TabResourceUsageTabHelperUiTest : public testing::Test {
 public:
  TabResourceUsageTabHelperUiTest() = default;
  ~TabResourceUsageTabHelperUiTest() override = default;

  void SetUp() override {
    auto web_contents =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    tab_strip_model_.AppendWebContents(std::move(web_contents), true);
    const int index = tab_strip_model_.count() - 1;
    auto* tab = tab_strip_model_.GetTabAtIndex(index);
    helper_ = tab->GetTabFeatures()->SetResourceUsageHelperForTesting(
        std::make_unique<TabResourceUsageTabHelper>(*tab));
  }

  void TearDown() override { helper_ = nullptr; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestTabStripModelDelegate delegate_;
  TestingProfile profile_;
  TabStripModel tab_strip_model_{&delegate_, &profile_};
  tabs::PreventTabFeatureInitialization prevent_;
  raw_ptr<TabResourceUsageTabHelper> helper_;
};

// Return memory usage that was set on the tab helper.
TEST_F(TabResourceUsageTabHelperUiTest, ReturnsMemoryUsageInBytes) {
  helper_->SetMemoryUsageInBytes(kTestMemoryUsageBytes);
  EXPECT_EQ(helper_->GetMemoryUsageInBytes(), kTestMemoryUsageBytes);
}

// Correctly reports whether memory usage is high after memory usage is set.
TEST_F(TabResourceUsageTabHelperUiTest, HighMemoryUsage) {
  uint64_t const high_memory_usage_threshold =
      TabResourceUsage::kHighMemoryUsageThresholdBytes;
  helper_->SetMemoryUsageInBytes(high_memory_usage_threshold);
  EXPECT_FALSE(helper_->resource_usage()->is_high_memory_usage());
  helper_->SetMemoryUsageInBytes(high_memory_usage_threshold + 1);
  EXPECT_TRUE(helper_->resource_usage()->is_high_memory_usage());
}
