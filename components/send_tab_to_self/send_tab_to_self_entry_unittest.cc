// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_entry.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/send_tab_to_self/proto/send_tab_to_self.pb.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace send_tab_to_self {

namespace {

bool IsEqualForTesting(const SendTabToSelfEntry& a,
                       const SendTabToSelfEntry& b) {
  return a.GetGUID() == b.GetGUID() && a.GetURL() == b.GetURL() &&
         a.GetTitle() == b.GetTitle() &&
         a.GetDeviceName() == b.GetDeviceName() &&
         a.GetTargetDeviceSyncCacheGuid() == b.GetTargetDeviceSyncCacheGuid() &&
         a.GetSharedTime() == b.GetSharedTime() &&
         a.GetOriginalNavigationTime() == b.GetOriginalNavigationTime();
}

bool IsEqualForTesting(const SendTabToSelfEntry& entry,
                       const sync_pb::SendTabToSelfSpecifics& specifics) {
  return (
      entry.GetGUID() == specifics.guid() &&
      entry.GetURL() == specifics.url() &&
      entry.GetTitle() == specifics.title() &&
      entry.GetDeviceName() == specifics.device_name() &&
      entry.GetTargetDeviceSyncCacheGuid() ==
          specifics.target_device_sync_cache_guid() &&
      specifics.shared_time_usec() ==
          entry.GetSharedTime().ToDeltaSinceWindowsEpoch().InMicroseconds() &&
      specifics.navigation_time_usec() == entry.GetOriginalNavigationTime()
                                              .ToDeltaSinceWindowsEpoch()
                                              .InMicroseconds());
}

TEST(SendTabToSelfEntry, CompareEntries) {
  const SendTabToSelfEntry e1("1", GURL("http://example.com"), "bar",
                              base::Time::FromTimeT(10),
                              base::Time::FromTimeT(10), "device1", "device2");
  const SendTabToSelfEntry e2("1", GURL("http://example.com"), "bar",
                              base::Time::FromTimeT(10),
                              base::Time::FromTimeT(10), "device1", "device2");

  EXPECT_TRUE(IsEqualForTesting(e1, e2));
  const SendTabToSelfEntry e3("2", GURL("http://example.org"), "bar",
                              base::Time::FromTimeT(10),
                              base::Time::FromTimeT(10), "device1", "device2");

  EXPECT_FALSE(IsEqualForTesting(e1, e3));
}

TEST(SendTabToSelfEntry, SharedTime) {
  SendTabToSelfEntry e("1", GURL("http://example.com"), "bar",
                       base::Time::FromTimeT(10), base::Time::FromTimeT(10),
                       "device", "device2");
  EXPECT_EQ("bar", e.GetTitle());
  // Getters return Base::Time values.
  EXPECT_EQ(e.GetSharedTime(), base::Time::FromTimeT(10));
}

// Tests that the send tab to self entry is correctly encoded to
// sync_pb::SendTabToSelfSpecifics.
TEST(SendTabToSelfEntry, AsProto) {
  SendTabToSelfEntry entry("1", GURL("http://example.com"), "bar",
                           base::Time::FromTimeT(10), base::Time::FromTimeT(10),
                           "device", "device2");
  SendTabToSelfLocal pb_entry(entry.AsLocalProto());
  EXPECT_TRUE(IsEqualForTesting(entry, pb_entry.specifics()));
}

// Tests that the send tab to self entry is correctly created from the required
// fields
TEST(SendTabToSelfEntry, FromRequiredFields) {
  SendTabToSelfEntry expected("1", GURL("http://example.com"), "", base::Time(),
                              base::Time(), "", "target_device");
  std::unique_ptr<SendTabToSelfEntry> actual =
      SendTabToSelfEntry::FromRequiredFields("1", GURL("http://example.com"),
                                             "target_device");
  EXPECT_TRUE(IsEqualForTesting(expected, *actual));
}

// Tests that the send tab to self entry is correctly parsed from
// sync_pb::SendTabToSelfSpecifics.
TEST(SendTabToSelfEntry, FromProto) {
  std::unique_ptr<sync_pb::SendTabToSelfSpecifics> pb_entry =
      std::make_unique<sync_pb::SendTabToSelfSpecifics>();
  pb_entry->set_guid("1");
  pb_entry->set_url("http://example.com/");
  pb_entry->set_title("title");
  pb_entry->set_device_name("device");
  pb_entry->set_target_device_sync_cache_guid("device");
  pb_entry->set_shared_time_usec(1);
  pb_entry->set_navigation_time_usec(1);

  std::unique_ptr<SendTabToSelfEntry> entry(
      SendTabToSelfEntry::FromProto(*pb_entry, base::Time::FromTimeT(10)));

  EXPECT_TRUE(IsEqualForTesting(*entry, *pb_entry));
}

// Tests that the send tab to self entry expiry works as expected
TEST(SendTabToSelfEntry, IsExpired) {
  SendTabToSelfEntry entry("1", GURL("http://example.com"), "bar",
                           base::Time::FromTimeT(10), base::Time::FromTimeT(10),
                           "device1", "device1");

  EXPECT_TRUE(entry.IsExpired(base::Time::FromTimeT(11) +
                              base::TimeDelta::FromDays(10)));
  EXPECT_FALSE(entry.IsExpired(base::Time::FromTimeT(11)));
}

// Tests that the send tab to self entry rejects strings that are not utf8.
TEST(SendTabToSelfEntry, InvalidStrings) {
  const base::char16 term[1] = {0xFDD1u};
  std::string invalid_utf8;
  base::UTF16ToUTF8(&term[0], 1, &invalid_utf8);

  EXPECT_DCHECK_DEATH(SendTabToSelfEntry(
      "1", GURL("http://example.com"), invalid_utf8, base::Time::FromTimeT(10),
      base::Time::FromTimeT(10), "device", "device"));

  EXPECT_DCHECK_DEATH(
      SendTabToSelfEntry(invalid_utf8, GURL("http://example.com"), "title",
                         base::Time::FromTimeT(10), base::Time::FromTimeT(10),
                         "device", "device"));

  EXPECT_DCHECK_DEATH(SendTabToSelfEntry(
      "1", GURL("http://example.com"), "title", base::Time::FromTimeT(10),
      base::Time::FromTimeT(10), invalid_utf8, "device"));

  EXPECT_DCHECK_DEATH(SendTabToSelfEntry(
      "1", GURL("http://example.com"), "title", base::Time::FromTimeT(10),
      base::Time::FromTimeT(10), "device", invalid_utf8));

  std::unique_ptr<sync_pb::SendTabToSelfSpecifics> pb_entry =
      std::make_unique<sync_pb::SendTabToSelfSpecifics>();
  pb_entry->set_guid(invalid_utf8);
  pb_entry->set_url("http://example.com/");
  pb_entry->set_title(invalid_utf8);
  pb_entry->set_device_name(invalid_utf8);
  pb_entry->set_target_device_sync_cache_guid("device");
  ;
  pb_entry->set_shared_time_usec(1);
  pb_entry->set_navigation_time_usec(1);

  EXPECT_DCHECK_DEATH(
      SendTabToSelfEntry::FromProto(*pb_entry, base::Time::FromTimeT(10)));
}

// Tests that the send tab to self entry is correctly encoded to
// sync_pb::SendTabToSelfSpecifics.
TEST(SendTabToSelfEntry, MarkAsOpened) {
  SendTabToSelfEntry entry("1", GURL("http://example.com"), "bar",
                           base::Time::FromTimeT(10), base::Time::FromTimeT(10),
                           "device", "device2");
  EXPECT_FALSE(entry.IsOpened());
  entry.MarkOpened();
  EXPECT_TRUE(entry.IsOpened());

  std::unique_ptr<sync_pb::SendTabToSelfSpecifics> pb_entry =
      std::make_unique<sync_pb::SendTabToSelfSpecifics>();
  pb_entry->set_guid("1");
  pb_entry->set_url("http://example.com/");
  pb_entry->set_title("title");
  pb_entry->set_device_name("device");
  pb_entry->set_target_device_sync_cache_guid("device");
  pb_entry->set_shared_time_usec(1);
  pb_entry->set_navigation_time_usec(1);
  pb_entry->set_opened(true);

  std::unique_ptr<SendTabToSelfEntry> entry2(
      SendTabToSelfEntry::FromProto(*pb_entry, base::Time::FromTimeT(10)));

  EXPECT_TRUE(entry2->IsOpened());
}

}  // namespace

}  // namespace send_tab_to_self
