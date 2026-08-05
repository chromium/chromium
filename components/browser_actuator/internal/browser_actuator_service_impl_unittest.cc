// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/browser_actuator_service_impl.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/browser_actuator/internal/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {

class BrowserActuatorServiceImplTest : public testing::Test {
 protected:
  BrowserActuatorServiceImplTest() = default;
  ~BrowserActuatorServiceImplTest() override = default;

  base::test::TaskEnvironment task_environment_;
};

TEST_F(BrowserActuatorServiceImplTest, IsInitialized) {
  BrowserActuatorServiceImpl service;
  EXPECT_TRUE(service.IsInitialized());
}

TEST_F(BrowserActuatorServiceImplTest, GetChannelReturnsValidChannel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowserActuatorChannelEnabled);
  BrowserActuatorServiceImpl service;
  EXPECT_NE(service.GetChannel(), nullptr);
}

TEST_F(BrowserActuatorServiceImplTest, GetChannelReturnsNullIfChannelDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kBrowserActuatorChannelEnabled);
  BrowserActuatorServiceImpl disabled_service;
  EXPECT_EQ(disabled_service.GetChannel(), nullptr);
}

}  // namespace browser_actuator
