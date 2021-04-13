// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/network_health_provider.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace diagnostics {

class NetworkHealthProviderTest : public testing::Test {
 public:
  NetworkHealthProviderTest() {
    // Wait for CrosNetworkConfig service to be created and initialized.
    task_environment_.RunUntilIdle();
  }

  ~NetworkHealthProviderTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  NetworkHealthProvider network_health_provider_;
};

TEST_F(NetworkHealthProviderTest, DummyTest) {
  EXPECT_TRUE(true);
}

}  // namespace diagnostics
}  // namespace chromeos
