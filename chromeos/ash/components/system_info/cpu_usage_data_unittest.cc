// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system_info/cpu_usage_data.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace system_info {

class CpuUsageDataTest : public testing::Test {
 public:
  CpuUsageDataTest() = default;
  ~CpuUsageDataTest() override = default;
};

TEST_F(CpuUsageDataTest, Initialized) {
  CpuUsageData data;

  EXPECT_FALSE(data.IsInitialized());

  data = CpuUsageData(1, 2, 3);

  EXPECT_TRUE(data.IsInitialized());
}

TEST_F(CpuUsageDataTest, Total) {
  CpuUsageData data(1, 2, 3);

  EXPECT_EQ(6u, data.GetTotalTime());
}

TEST_F(CpuUsageDataTest, Addition) {
  CpuUsageData data_1(1, 2, 3);
  CpuUsageData data_2(4, 5, 6);

  CpuUsageData data_3 = data_1 + data_2;

  EXPECT_EQ(5u, data_3.GetUserTime());
  EXPECT_EQ(7u, data_3.GetSystemTime());
  EXPECT_EQ(9u, data_3.GetIdleTime());
}

TEST_F(CpuUsageDataTest, CompoundAddition) {
  CpuUsageData data_1(1, 2, 3);
  CpuUsageData data_2(4, 5, 6);

  data_1 += data_2;

  EXPECT_EQ(5u, data_1.GetUserTime());
  EXPECT_EQ(7u, data_1.GetSystemTime());
  EXPECT_EQ(9u, data_1.GetIdleTime());
}

TEST_F(CpuUsageDataTest, Subtraction) {
  CpuUsageData data_1(1, 2, 3);
  CpuUsageData data_2(4, 5, 6);

  CpuUsageData data_3 = data_2 - data_1;

  EXPECT_EQ(3u, data_3.GetUserTime());
  EXPECT_EQ(3u, data_3.GetSystemTime());
  EXPECT_EQ(3u, data_3.GetIdleTime());
}

TEST_F(CpuUsageDataTest, CompoundSubtraction) {
  CpuUsageData data_1(1, 2, 3);
  CpuUsageData data_2(4, 5, 6);

  data_2 -= data_1;

  EXPECT_EQ(3u, data_2.GetUserTime());
  EXPECT_EQ(3u, data_2.GetSystemTime());
  EXPECT_EQ(3u, data_2.GetIdleTime());
}

}  // namespace system_info
