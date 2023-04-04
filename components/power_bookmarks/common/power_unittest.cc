// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/common/power.h"

#include "base/time/time.h"
#include "base/uuid.h"
#include "components/sync/protocol/power_bookmark_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace power_bookmarks {

namespace {
sync_pb::PowerBookmarkSpecifics CreatePowerBookmarkSpecifics() {
  sync_pb::PowerBookmarkSpecifics specifics;
  specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  specifics.set_url("http://google.com/");
  specifics.set_power_type(sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK);
  specifics.set_creation_time_usec(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  specifics.set_update_time_usec(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  return specifics;
}
}  // namespace

TEST(PowerTest, CreateFromSpecifics) {
  sync_pb::PowerBookmarkSpecifics specifics = CreatePowerBookmarkSpecifics();
  Power power(specifics);

  EXPECT_EQ(power.guid(), base::Uuid::ParseLowercase(specifics.guid()));
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
  sync_pb::PowerBookmarkSpecifics specifics = CreatePowerBookmarkSpecifics();
  Power power(specifics);
  sync_pb::PowerBookmarkSpecifics new_specifics;
  power.ToPowerBookmarkSpecifics(&new_specifics);

  EXPECT_EQ(specifics.guid(), new_specifics.guid());
  EXPECT_EQ(specifics.url(), new_specifics.url());
  EXPECT_EQ(specifics.power_type(), new_specifics.power_type());
  EXPECT_EQ(specifics.creation_time_usec(), new_specifics.creation_time_usec());
  EXPECT_EQ(specifics.update_time_usec(), new_specifics.update_time_usec());
}

TEST(PowerTest, ClonePower) {
  sync_pb::PowerBookmarkSpecifics specifics = CreatePowerBookmarkSpecifics();
  Power power(specifics);
  std::unique_ptr<Power> clone = power.Clone();
  EXPECT_EQ(power.guid(), clone->guid());
  EXPECT_EQ(power.url(), clone->url());
  EXPECT_EQ(power.time_added(), clone->time_added());
  EXPECT_EQ(power.time_modified(), clone->time_modified());
  EXPECT_EQ(power.power_entity()->SerializeAsString(),
            clone->power_entity()->SerializeAsString());
}

TEST(PowerTest, MergePower) {
  sync_pb::PowerBookmarkSpecifics specifics = CreatePowerBookmarkSpecifics();
  Power power(specifics);
  Power other(specifics);
  base::Time now = base::Time::Now();
  power.set_time_added(now);
  power.set_time_modified(now);
  other.set_time_added(now + base::Seconds(1));
  other.set_time_modified(now + base::Seconds(1));
  power.Merge(other);
  EXPECT_EQ(power.time_added(), now);
  EXPECT_EQ(power.time_modified(), other.time_modified());
}

}  // namespace power_bookmarks
