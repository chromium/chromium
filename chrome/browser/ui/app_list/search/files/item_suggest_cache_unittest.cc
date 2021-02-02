// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/item_suggest_cache.h"

#include <vector>

#include "base/json/json_reader.h"
#include "chrome/browser/profiles/profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

// TODO(crbug.com/1034842): Add test checking we make no requests when disabled
// by experiment or policy.

class ItemSuggestCacheTest : public testing::Test {
 public:
  ItemSuggestCacheTest() = default;
  ~ItemSuggestCacheTest() override = default;

  base::Value Parse(const std::string& json) {
    return base::JSONReader::Read(json).value();
  }

  void ResultMatches(const ItemSuggestCache::Result& actual,
                     const std::string& id,
                     const std::string& title) {
    EXPECT_EQ(actual.id, id);
    EXPECT_EQ(actual.title, title);
  }

  void ResultsMatch(
      const base::Optional<ItemSuggestCache::Results>& actual,
      const std::string& suggestion_id,
      const std::vector<std::pair<std::string, std::string>>& results) {
    EXPECT_TRUE(actual.has_value());

    EXPECT_EQ(actual->suggestion_id, suggestion_id);
    ASSERT_EQ(actual->results.size(), results.size());
    for (int i = 0; i < results.size(); ++i) {
      ResultMatches(actual->results[i], results[i].first, results[i].second);
    }
  }
};

TEST_F(ItemSuggestCacheTest, ConvertJsonSuccess) {
  const base::Value full = Parse(R"(
    {
      "item": [
        {
          "itemId": "item id 1",
          "displayText": "display text 1"
        },
        {
          "itemId": "item id 2",
          "displayText": "display text 2",
          "predictionReason": "unused field"
        },
        {
          "itemId": "item id 3",
          "displayText": "display text 3"
        }
      ],
      "suggestionSessionId": "the suggestion id"
    })");
  ResultsMatch(ItemSuggestCache::ConvertJsonForTest(&full), "the suggestion id",
               {{"item id 1", "display text 1"},
                {"item id 2", "display text 2"},
                {"item id 3", "display text 3"}});

  const base::Value empty_items = Parse(R"(
    {
      "item": [],
      "suggestionSessionId": "the suggestion id"
    })");
  ResultsMatch(ItemSuggestCache::ConvertJsonForTest(&empty_items),
               "the suggestion id", {});
}

TEST_F(ItemSuggestCacheTest, ConvertJsonFailure) {
  const base::Value no_display_text = Parse(R"(
    {
      "item": [
        {
          "itemId": "item id 1"
        }
      ],
      "suggestionSessionId": "the suggestion id"
    })");
  EXPECT_FALSE(
      ItemSuggestCache::ConvertJsonForTest(&no_display_text).has_value());

  const base::Value no_item_id = Parse(R"(
    {
      "item": [
        {
          "displayText": "display text 1"
        }
      ],
      "suggestionSessionId": "the suggestion id"
    })");
  EXPECT_FALSE(ItemSuggestCache::ConvertJsonForTest(&no_item_id).has_value());

  const base::Value no_session_id = Parse(R"(
    {
      "item": [
        {
          "itemId": "item id 1",
          "displayText": "display text 2"
        }
      ]
    })");
  EXPECT_FALSE(
      ItemSuggestCache::ConvertJsonForTest(&no_session_id).has_value());

  const base::Value no_items = Parse(R"(
    {
      "suggestionSessionId": "the suggestion id"
    })");
  EXPECT_FALSE(ItemSuggestCache::ConvertJsonForTest(&no_items).has_value());
}

}  // namespace app_list
