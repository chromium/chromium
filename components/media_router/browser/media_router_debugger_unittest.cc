// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/media_router_debugger.h"

#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class MediaRouterDebuggerTest : public ::testing::Test {
 public:
  MediaRouterDebuggerTest()
      : debugger_(std::make_unique<MediaRouterDebugger>()) {}
  MediaRouterDebuggerTest(const MediaRouterDebuggerTest&) = delete;
  ~MediaRouterDebuggerTest() override = default;
  MediaRouterDebuggerTest& operator=(const MediaRouterDebuggerTest&) = delete;

 protected:
  std::unique_ptr<MediaRouterDebugger> debugger_;
  content::BrowserTaskEnvironment test_environment_;
};

TEST_F(MediaRouterDebuggerTest, TestIsRtcpReportsEnabled) {
  // Tests default condition.
  EXPECT_FALSE(debugger_->IsRtcpReportsEnabled());

  // Reports should still be disabled since we the feature flag has not been
  // set.
  debugger_->EnableRtcpReports();
  EXPECT_FALSE(debugger_->IsRtcpReportsEnabled());

  // All conditions should be met and the function should return true now.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({media::kEnableRtcpReporting}, {});

  EXPECT_TRUE(debugger_->IsRtcpReportsEnabled());
}

}  // namespace media_router
