// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_schedule.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace download {
namespace {

TEST(DownloadScheduleTest, CtorAndCopy) {
  DownloadSchedule download_schedule(false, absl::nullopt);
  EXPECT_FALSE(download_schedule.only_on_wifi());
  EXPECT_EQ(download_schedule.start_time(), absl::nullopt);

  download_schedule = DownloadSchedule(true, absl::nullopt);
  EXPECT_TRUE(download_schedule.only_on_wifi());
  EXPECT_EQ(download_schedule.start_time(), absl::nullopt);

  auto time = absl::make_optional(
      base::Time::FromDeltaSinceWindowsEpoch(base::Days(1)));
  download_schedule = DownloadSchedule(false, time);
  EXPECT_FALSE(download_schedule.only_on_wifi());
  EXPECT_EQ(download_schedule.start_time(), time);
}

}  // namespace
}  // namespace download
