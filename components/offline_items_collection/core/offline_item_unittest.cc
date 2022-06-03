// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/offline_item.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace offline_items_collection {
namespace {

TEST(OfflineItemTest, OfflineItemSchedule) {
  OfflineItemSchedule schedule(true, absl::nullopt);
  EXPECT_TRUE(schedule.only_on_wifi);
  EXPECT_FALSE(schedule.start_time.has_value());

  base::Time time = base::Time::Now();
  schedule = OfflineItemSchedule(false, time);
  EXPECT_FALSE(schedule.only_on_wifi);
  EXPECT_EQ(schedule.start_time, time);
}

}  // namespace
}  // namespace offline_items_collection
