// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"

#include "base/byte_count.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace {
constexpr base::ByteCount kTestMemoryUsage = base::ByteCount(100000);
}  // namespace

class TabResourceUsageTabHelperUiTest : public testing::Test {
 public:
  TabResourceUsageTabHelperUiTest() = default;
  ~TabResourceUsageTabHelperUiTest() override = default;

  void SetUp() override {
    auto web_contents =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    tab_strip_model_.AppendWebContents(std::move(web_contents), true);

    EXPECT_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(user_data_host_));

    helper_.emplace(mock_tab_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestTabStripModelDelegate delegate_;
  TestingProfile profile_;
  TabStripModel tab_strip_model_{&delegate_, &profile_};
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  ui::UnownedUserDataHost user_data_host_;
  tabs::MockTabInterface mock_tab_;
  std::optional<TabResourceUsageTabHelper> helper_;
};

// Return memory usage that was set on the tab helper.
TEST_F(TabResourceUsageTabHelperUiTest, ReturnsMemoryUsageInBytes) {
  helper_->SetMemoryUsage(kTestMemoryUsage);
  EXPECT_EQ(helper_->GetMemoryUsage(), kTestMemoryUsage);
}

// Correctly reports whether memory usage is high after memory usage is set.
TEST_F(TabResourceUsageTabHelperUiTest, HighMemoryUsage) {
  helper_->SetMemoryUsage(TabResourceUsage::kHighMemoryUsageThreshold);
  EXPECT_FALSE(helper_->resource_usage()->is_high_memory_usage());
  helper_->SetMemoryUsage(TabResourceUsage::kHighMemoryUsageThreshold +
                          base::ByteCount(1));
  EXPECT_TRUE(helper_->resource_usage()->is_high_memory_usage());
}

// Verifies that the callback is being invoked correctly.
TEST_F(TabResourceUsageTabHelperUiTest, InvokesCallback) {
  scoped_refptr<const TabResourceUsage> received_usage;
  bool callback_invoked = false;

  base::RepeatingCallback<void(scoped_refptr<const TabResourceUsage>)>
      mock_callback = base::BindLambdaForTesting(
          [&](scoped_refptr<const TabResourceUsage> usage) {
            received_usage = usage;
            callback_invoked = true;
          });

  auto subscription = helper_->AddResourceUsageChangeCallback(mock_callback);

  EXPECT_FALSE(callback_invoked);

  helper_->SetMemoryUsage(kTestMemoryUsage);

  EXPECT_TRUE(callback_invoked);
  EXPECT_EQ(received_usage->memory_usage(), kTestMemoryUsage);
}
