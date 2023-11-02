// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/jumplist_update_util.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Helper function to create a ShellLinkItem whose url is specified by |url|.
scoped_refptr<ShellLinkItem> CreateShellLinkWithURL(const std::string& url) {
  auto item = base::MakeRefCounted<ShellLinkItem>();
  item->set_url(url);
  return item;
}

}  // namespace

TEST(JumpListUpdateUtilTest, MostVisitedItemsUnchanged) {
  // Test data.
  static constexpr struct {
    const char* url;
    const char16_t* title;
  } kTestData[] = {{"https://www.google.com/", u"Google"},
                   {"https://www.youtube.com/", u"Youtube"},
                   {"https://www.gmail.com/", u"Gmail"}};

  ShellLinkItemList jumplist_items;
  history::MostVisitedURLList history_items;

  for (const auto& test_data : kTestData) {
    jumplist_items.push_back(CreateShellLinkWithURL(test_data.url));
    history_items.emplace_back(GURL(test_data.url), test_data.title);
  }

  // Both jumplist_items and history_items have 3 urls: Google, Youtube, Gmail.
  // Also, their urls have the same order.
  EXPECT_TRUE(MostVisitedItemsUnchanged(jumplist_items, history_items, 3));
  EXPECT_FALSE(MostVisitedItemsUnchanged(jumplist_items, history_items, 2));
  EXPECT_FALSE(MostVisitedItemsUnchanged(jumplist_items, history_items, 1));

  // Reverse history_items, so the 3 urls in history_items are in reverse order:
  // Gmail, Youtube, Google.
  // The 3 urls in jumplist_items remain the same: Google, Youtube, Gmail.
  std::reverse(history_items.begin(), history_items.end());
  EXPECT_FALSE(MostVisitedItemsUnchanged(jumplist_items, history_items, 3));

  // Reverse history_items back.
  std::reverse(history_items.begin(), history_items.end());
  EXPECT_TRUE(MostVisitedItemsUnchanged(jumplist_items, history_items, 3));

  // Pop out the last url ("Gmail") from jumplist_items.
  // Now jumplist_items has 2 urls: Google, Youtube,
  // and history_items has 3 urls: Google, Youtube, Gmail.
  jumplist_items.pop_back();
  EXPECT_FALSE(MostVisitedItemsUnchanged(jumplist_items, history_items, 3));
  EXPECT_TRUE(MostVisitedItemsUnchanged(jumplist_items, history_items, 2));
  EXPECT_FALSE(MostVisitedItemsUnchanged(jumplist_items, history_items, 1));

  // Pop out the last two urls ("Youtube", "Gmail") from history_items.
  // Now jumplist_items has 2 urls: Google, Youtube,
  // and history_items has 1 url: Google.
  history_items.pop_back();
  history_items.pop_back();
  EXPECT_FALSE(MostVisitedItemsUnchanged(jumplist_items, history_items, 3));
  EXPECT_FALSE(MostVisitedItemsUnchanged(jumplist_items, history_items, 2));
  EXPECT_FALSE(MostVisitedItemsUnchanged(jumplist_items, history_items, 1));
}
