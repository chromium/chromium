// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_item.h"

#include <memory>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using OptOutBlocklistItemTest = testing::Test;

}  // namespace

namespace blocklist {

TEST_F(OptOutBlocklistItemTest, BlockListState) {
  const int history = 4;
  const int threshold = 2;
  const base::TimeDelta max_blocklist_duration = base::Seconds(30);
  const base::Time now = base::Time::UnixEpoch();
  const base::TimeDelta delay_between_entries = base::Seconds(1);
  const base::Time later =
      now + max_blocklist_duration + (delay_between_entries * 3);

  OptOutBlocklistItem block_list_item(history, threshold,
                                      max_blocklist_duration);

  // Empty block list item should report that the host is allowed.
  EXPECT_FALSE(block_list_item.IsBlockListed(now));
  EXPECT_FALSE(block_list_item.IsBlockListed(later));

  EXPECT_FALSE(block_list_item.most_recent_opt_out_time());
  block_list_item.AddEntry(false, now);
  EXPECT_FALSE(block_list_item.most_recent_opt_out_time());

  block_list_item.AddEntry(true, now);
  EXPECT_TRUE(block_list_item.most_recent_opt_out_time());
  EXPECT_EQ(now, block_list_item.most_recent_opt_out_time().value());
  // Block list item of size less that |threshold| should report that the host
  // is allowed.
  EXPECT_FALSE(block_list_item.IsBlockListed(now));
  EXPECT_FALSE(block_list_item.IsBlockListed(later));

  block_list_item.AddEntry(true, now + delay_between_entries);
  // Block list item with |threshold| fresh entries should report the host as
  // disallowed.
  EXPECT_TRUE(block_list_item.IsBlockListed(now));
  // Block list item with only entries from longer than |duration| ago should
  // report the host is allowed.
  EXPECT_FALSE(block_list_item.IsBlockListed(later));
  block_list_item.AddEntry(true, later - (delay_between_entries * 2));
  // Block list item with a fresh opt out and total number of opt outs larger
  // than |threshold| should report the host is disallowed.
  EXPECT_TRUE(block_list_item.IsBlockListed(later));

  // The block list item should maintain entries based on time, so adding
  // |history| entries should not push out newer entries.
  block_list_item.AddEntry(true, later - delay_between_entries * 2);
  block_list_item.AddEntry(false, later - delay_between_entries * 3);
  block_list_item.AddEntry(false, later - delay_between_entries * 3);
  block_list_item.AddEntry(false, later - delay_between_entries * 3);
  block_list_item.AddEntry(false, later - delay_between_entries * 3);
  EXPECT_TRUE(block_list_item.IsBlockListed(later));

  // The block list item should maintain entries based on time, so adding
  // |history| newer entries should push out older entries.
  block_list_item.AddEntry(false, later - delay_between_entries * 1);
  block_list_item.AddEntry(false, later - delay_between_entries * 1);
  block_list_item.AddEntry(false, later - delay_between_entries * 1);
  block_list_item.AddEntry(false, later - delay_between_entries * 1);
  EXPECT_FALSE(block_list_item.IsBlockListed(later));
}

}  // namespace blocklist
