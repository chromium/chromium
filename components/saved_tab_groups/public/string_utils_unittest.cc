// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/string_utils.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {

// Returns a date back in the past by the given time delta.
std::u16string TimeAgo(base::TimeDelta delta) {
  return LocalizedElapsedTimeSinceCreation(delta);
}

using SavedTabGroupsStringUtilsTest = testing::Test;

TEST_F(SavedTabGroupsStringUtilsTest, LocalizedElapsedTimeSinceCreation) {
  // Testing English strings.
  EXPECT_EQ(u"Created just now", TimeAgo(base::Seconds(-123456789)));
  EXPECT_EQ(u"Created just now", TimeAgo(base::Seconds(0)));
  EXPECT_EQ(u"Created just now", TimeAgo(base::Seconds(1)));
  EXPECT_EQ(u"Created just now", TimeAgo(base::Seconds(59)));
  EXPECT_EQ(u"Created 1 minute ago", TimeAgo(base::Seconds(60)));
  EXPECT_EQ(u"Created 1 minute ago", TimeAgo(base::Minutes(1)));
  EXPECT_EQ(u"Created 2 minutes ago", TimeAgo(base::Minutes(2)));
  EXPECT_EQ(u"Created 1 hour ago", TimeAgo(base::Hours(1)));
  EXPECT_EQ(u"Created 2 hours ago", TimeAgo(base::Hours(2)));
  EXPECT_EQ(u"Created 1 day ago", TimeAgo(base::Days(1)));
  EXPECT_EQ(u"Created 2 days ago", TimeAgo(base::Days(2)));
  EXPECT_EQ(u"Created 1 month ago", TimeAgo(base::Days(45)));
  EXPECT_EQ(u"Created 2 months ago", TimeAgo(base::Days(90)));
  EXPECT_EQ(u"Created 1 year ago", TimeAgo(base::Days(400)));
  EXPECT_EQ(u"Created 2 years ago", TimeAgo(base::Days(800)));
  EXPECT_EQ(u"Created 10 years ago", TimeAgo(base::Days(4000)));
}

}  // namespace tab_groups
