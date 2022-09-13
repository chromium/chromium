// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/android/android_history_types.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace history {

TEST(AndroidHistoryTypesTest, TestGetBookmarkColumnID) {
  EXPECT_EQ(HistoryAndBookmarkRow::ID,
            HistoryAndBookmarkRow::GetColumnID("_id"));
  EXPECT_EQ(HistoryAndBookmarkRow::URL,
            HistoryAndBookmarkRow::GetColumnID("url"));
  EXPECT_EQ(HistoryAndBookmarkRow::TITLE,
            HistoryAndBookmarkRow::GetColumnID("title"));
  EXPECT_EQ(HistoryAndBookmarkRow::CREATED,
            HistoryAndBookmarkRow::GetColumnID("created"));
  EXPECT_EQ(HistoryAndBookmarkRow::LAST_VISIT_TIME,
            HistoryAndBookmarkRow::GetColumnID("date"));
  EXPECT_EQ(HistoryAndBookmarkRow::VISIT_COUNT,
            HistoryAndBookmarkRow::GetColumnID("visits"));
  EXPECT_EQ(HistoryAndBookmarkRow::FAVICON,
            HistoryAndBookmarkRow::GetColumnID("favicon"));
  EXPECT_EQ(HistoryAndBookmarkRow::BOOKMARK,
            HistoryAndBookmarkRow::GetColumnID("bookmark"));
  EXPECT_EQ(HistoryAndBookmarkRow::RAW_URL,
            HistoryAndBookmarkRow::GetColumnID("raw_url"));
}

TEST(AndroidHistoryTypesTest, TestGetSearchColumnID) {
  EXPECT_EQ(SearchRow::ID, SearchRow::GetColumnID("_id"));
  EXPECT_EQ(SearchRow::SEARCH_TERM, SearchRow::GetColumnID("search"));
  EXPECT_EQ(SearchRow::SEARCH_TIME, SearchRow::GetColumnID("date"));
}

}  // namespace history
