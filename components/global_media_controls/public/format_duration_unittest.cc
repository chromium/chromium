// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/format_duration.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace global_media_controls {

using FormatDurationTest = testing::Test;

TEST_F(FormatDurationTest, FormatDurationWithHours) {
  auto long_duration = base::Seconds(5698721);
  EXPECT_EQ(u"1,582:58:41", GetFormattedDuration(long_duration));

  auto with_zero_duration = base::Seconds(3600);
  EXPECT_EQ(u"1:00:00", GetFormattedDuration(with_zero_duration));
}

TEST_F(FormatDurationTest, FormatDurationWithoutHours) {
  auto long_duration = base::Seconds(3538);
  EXPECT_EQ(u"58:58", GetFormattedDuration(long_duration));

  auto with_zero_duration = base::Seconds(61);
  EXPECT_EQ(u"1:01", GetFormattedDuration(with_zero_duration));

  auto no_minute_duration = base::Seconds(3);
  EXPECT_EQ(u"0:03", GetFormattedDuration(no_minute_duration));

  auto zero_duration = base::Seconds(0);
  EXPECT_EQ(u"0:00", GetFormattedDuration(zero_duration));
}

}  // namespace global_media_controls
