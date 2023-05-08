// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/cellular_utils.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/hermes/hermes_clients.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const char kTestEuiccPath[] = "/org/chromium/Hermes/Euicc/0";
const char kTestEuiccPath2[] = "/org/chromium/Hermes/Euicc/0";
const char kTestEid[] = "12345678901234567890123456789012";
const char kTestEid2[] = "12345678901234567890123456789000";

}  // namespace

class CellularUtilsTest : public testing::Test {
 public:
  CellularUtilsTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  CellularUtilsTest(const CellularUtilsTest&) = delete;
  CellularUtilsTest& operator=(const CellularUtilsTest&) = delete;

  ~CellularUtilsTest() override = default;

  // testing::Test
  void SetUp() override { hermes_clients::InitializeFakes(); }

  void TearDown() override { hermes_clients::Shutdown(); }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(CellularUtilsTest, GetCurrentEuiccPath) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kCellularUseSecondEuicc);
  HermesManagerClient::Get()->GetTestInterface()->ClearEuiccs();
  EXPECT_FALSE(cellular_utils::GetCurrentEuiccPath());
  // Verify that use-second-euicc flag should be ignored when Hermes only
  // exposes only one Euicc.
  HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
      dbus::ObjectPath(kTestEuiccPath), kTestEid, /*is_active=*/true,
      /*physical_slot=*/0);
  EXPECT_EQ(kTestEuiccPath, cellular_utils::GetCurrentEuiccPath()->value());
  // Verify that use-second-euicc flag should take effect when Hermes exposes
  // two Euicc(s).
  HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
      dbus::ObjectPath(kTestEuiccPath2), kTestEid2, /*is_active=*/false,
      /*physical_slot=*/1);
  EXPECT_EQ(kTestEuiccPath2, cellular_utils::GetCurrentEuiccPath()->value());
}

}  // namespace ash
