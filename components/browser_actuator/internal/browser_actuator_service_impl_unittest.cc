// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/browser_actuator_service_impl.h"

#include <memory>

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {

class BrowserActuatorServiceImplTest : public testing::Test {
 protected:
  BrowserActuatorServiceImplTest()
      : service_(std::make_unique<BrowserActuatorServiceImpl>()) {}
  ~BrowserActuatorServiceImplTest() override = default;

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<BrowserActuatorServiceImpl> service_;
};

TEST_F(BrowserActuatorServiceImplTest, IsInitialized) {
  EXPECT_TRUE(service_->IsInitialized());
}

}  // namespace browser_actuator
