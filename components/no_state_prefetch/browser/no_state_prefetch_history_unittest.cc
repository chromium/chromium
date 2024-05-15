// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/browser/no_state_prefetch_history.h"

#include <stddef.h>

#include <memory>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace prerender {

namespace {

bool ListEntryMatches(const base::Value::List& list,
                      size_t index,
                      const char* expected_url,
                      FinalStatus expected_final_status,
                      Origin expected_origin,
                      const std::string& expected_end_time) {
  if (index >= list.size()) {
    return false;
  }
  const base::Value& value = list[index];
  if (!value.is_dict()) {
    return false;
  }
  const base::Value::Dict& dict = value.GetDict();
  if (dict.size() != 4u) {
    return false;
  }
  const std::string* url = dict.FindString("url");
  if (!url) {
    return false;
  }
  if (*url != expected_url) {
    return false;
  }
  const std::string* final_status = dict.FindString("final_status");
  if (!final_status) {
    return false;
  }
  if (*final_status != NameFromFinalStatus(expected_final_status)) {
    return false;
  }
  const std::string* origin = dict.FindString("origin");
  if (!origin) {
    return false;
  }
  if (*origin != NameFromOrigin(expected_origin)) {
    return false;
  }
  const std::string* end_time = dict.FindString("end_time");
  if (!end_time) {
    return false;
  }
  if (*end_time != expected_end_time) {
    return false;
  }
  return true;
}

TEST(NoStatePrefetchHistoryTest, GetAsValue) {
  // Create a history with only 2 values.
  NoStatePrefetchHistory history(2);

  // Make sure an empty list exists when retrieving as value.
  base::Value::List entry_value = history.CopyEntriesAsValue();
  EXPECT_TRUE(entry_value.empty());

  // Base time used for all events.  Each event is given a time 1 millisecond
  // after that of the previous one.
  base::Time epoch_start = base::Time::UnixEpoch();

  // Add a single entry and make sure that it matches up.
  const char* const kFirstUrl = "http://www.alpha.com/";
  const FinalStatus kFirstFinalStatus = FINAL_STATUS_USED;
  const Origin kFirstOrigin = ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN;
  NoStatePrefetchHistory::Entry entry_first(GURL(kFirstUrl), kFirstFinalStatus,
                                            kFirstOrigin, epoch_start);
  history.AddEntry(entry_first);
  entry_value = history.CopyEntriesAsValue();
  EXPECT_EQ(1u, entry_value.size());
  EXPECT_TRUE(ListEntryMatches(entry_value, 0u, kFirstUrl, kFirstFinalStatus,
                               kFirstOrigin, "0"));

  // Add a second entry and make sure both first and second appear.
  const char* const kSecondUrl = "http://www.beta.com/";
  const FinalStatus kSecondFinalStatus = FINAL_STATUS_DUPLICATE;
  const Origin kSecondOrigin = ORIGIN_SAME_ORIGIN_SPECULATION;
  NoStatePrefetchHistory::Entry entry_second(
      GURL(kSecondUrl), kSecondFinalStatus, kSecondOrigin,
      epoch_start + base::Milliseconds(1));
  history.AddEntry(entry_second);
  entry_value = history.CopyEntriesAsValue();
  EXPECT_EQ(2u, entry_value.size());
  EXPECT_TRUE(ListEntryMatches(entry_value, 0u, kSecondUrl, kSecondFinalStatus,
                               kSecondOrigin, "1"));
  EXPECT_TRUE(ListEntryMatches(entry_value, 1u, kFirstUrl, kFirstFinalStatus,
                               kFirstOrigin, "0"));

  // Add a third entry and make sure that the first one drops off.
  const char* const kThirdUrl = "http://www.gamma.com/";
  const FinalStatus kThirdFinalStatus = FINAL_STATUS_AUTH_NEEDED;
  const Origin kThirdOrigin = ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN;
  NoStatePrefetchHistory::Entry entry_third(
      GURL(kThirdUrl), kThirdFinalStatus, kThirdOrigin,
      epoch_start + base::Milliseconds(2));
  history.AddEntry(entry_third);
  entry_value = history.CopyEntriesAsValue();
  EXPECT_EQ(2u, entry_value.size());
  EXPECT_TRUE(ListEntryMatches(entry_value, 0u, kThirdUrl, kThirdFinalStatus,
                               kThirdOrigin, "2"));
  EXPECT_TRUE(ListEntryMatches(entry_value, 1u, kSecondUrl, kSecondFinalStatus,
                               kSecondOrigin, "1"));

  // Make sure clearing history acts as expected.
  history.Clear();
  entry_value = history.CopyEntriesAsValue();
  EXPECT_TRUE(entry_value.empty());
}

}  // namespace

}  // namespace prerender
