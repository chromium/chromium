// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/core/powers/power.h"

#include "base/guid.h"
#include "base/time/time.h"
#include "components/power_bookmarks/core/proto/power_bookmark_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_bookmarks {

namespace {
PowerBookmarkSpecifics CreatePowerBookmarkSpecifics() {
  PowerBookmarkSpecifics specifics;
  specifics.set_guid(base::GUID::GenerateRandomV4().AsLowercaseString());
  specifics.set_url("http://google.com/");
  specifics.set_power_type(PowerType::POWER_TYPE_MOCK);
  specifics.set_creation_time_usec(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  specifics.set_update_time_usec(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  return specifics;
}
}  // namespace

TEST(PowerTest, CreateFromSpecifics) {
  PowerBookmarkSpecifics specifics = CreatePowerBookmarkSpecifics();
  Power power(specifics);

  EXPECT_EQ(power.guid(), base::GUID::ParseLowercase(specifics.guid()));
  EXPECT_EQ(power.url().spec(), specifics.url());
  EXPECT_EQ(power.power_type(), specifics.power_type());
  EXPECT_EQ(power.time_added(),
            base::Time::FromDeltaSinceWindowsEpoch(
                base::Microseconds(specifics.creation_time_usec())));
  EXPECT_EQ(power.time_modified(),
            base::Time::FromDeltaSinceWindowsEpoch(
                base::Microseconds(specifics.update_time_usec())));
}

TEST(PowerTest, ToAndFromSpecifics) {
  PowerBookmarkSpecifics specifics = CreatePowerBookmarkSpecifics();
  Power power(specifics);
  PowerBookmarkSpecifics new_specifics;
  power.ToPowerBookmarkSpecifics(&new_specifics);

  EXPECT_EQ(specifics.guid(), new_specifics.guid());
  EXPECT_EQ(specifics.url(), new_specifics.url());
  EXPECT_EQ(specifics.power_type(), new_specifics.power_type());
  EXPECT_EQ(specifics.creation_time_usec(), new_specifics.creation_time_usec());
  EXPECT_EQ(specifics.update_time_usec(), new_specifics.update_time_usec());
}

}  // namespace power_bookmarks
