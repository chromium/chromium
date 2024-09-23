// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/answers_cache.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(AnswersCacheTest, CacheStartsEmpty) {
  AnswersCache cache(1);
  EXPECT_TRUE(cache.empty());
}

TEST(AnswersCacheTest, UpdatePopulatesCache) {
  AnswersCache cache(1);
  cache.UpdateRecentAnswers(u"weather los angeles",
                            omnibox::ANSWER_TYPE_WEATHER);
  EXPECT_FALSE(cache.empty());
}

TEST(AnswersCacheTest, GetWillRetrieveMatchingInfo) {
  AnswersCache cache(1);

  std::u16string full_query_text = u"weather los angeles";
  omnibox::AnswerType query_type = omnibox::ANSWER_TYPE_WEATHER;
  cache.UpdateRecentAnswers(full_query_text, query_type);

  // Partial prefixes retrieve info.
  AnswersQueryData data = cache.GetTopAnswerEntry(full_query_text.substr(0, 8));
  EXPECT_EQ(full_query_text, data.full_query_text);
  EXPECT_EQ(query_type, data.query_type);

  // Mismatched prefix retrieves empty data.
  data = cache.GetTopAnswerEntry(full_query_text.substr(1, 8));
  EXPECT_TRUE(data.full_query_text.empty());
  EXPECT_EQ(omnibox::ANSWER_TYPE_UNSPECIFIED, data.query_type);

  // Full match retrieves info.
  data = cache.GetTopAnswerEntry(full_query_text);
  EXPECT_EQ(full_query_text, data.full_query_text);
  EXPECT_EQ(query_type, data.query_type);
}

TEST(AnswersCacheTest, MatchMostRecent) {
  AnswersCache cache(2);

  std::u16string query_weather_la = u"weather los angeles";
  std::u16string query_weather_lv = u"weather las vegas";
  omnibox::AnswerType query_type = omnibox::ANSWER_TYPE_WEATHER;

  cache.UpdateRecentAnswers(query_weather_lv, query_type);
  cache.UpdateRecentAnswers(query_weather_la, query_type);

  // "weather los angeles" is most recent match to "weather l".
  AnswersQueryData data = cache.GetTopAnswerEntry(u"weather l");
  EXPECT_EQ(query_weather_la, data.full_query_text);

  // Update recency for "weather las vegas".
  cache.GetTopAnswerEntry(query_weather_lv);

  // "weather las vegas" should now be the most recent match to "weather l".
  data = cache.GetTopAnswerEntry(u"weather l");
  EXPECT_EQ(query_weather_lv, data.full_query_text);
}

TEST(AnswersCacheTest, LeastRecentItemIsEvicted) {
  AnswersCache cache(2);

  std::u16string query_weather_la = u"weather los angeles";
  std::u16string query_weather_lv = u"weather las vegas";
  std::u16string query_weather_lb = u"weather long beach";
  omnibox::AnswerType query_type = omnibox::ANSWER_TYPE_WEATHER;

  cache.UpdateRecentAnswers(query_weather_lb, query_type);
  cache.UpdateRecentAnswers(query_weather_lv, query_type);
  EXPECT_FALSE(
      cache.GetTopAnswerEntry(query_weather_lb).full_query_text.empty());
  EXPECT_FALSE(
      cache.GetTopAnswerEntry(query_weather_lv).full_query_text.empty());
  EXPECT_TRUE(
      cache.GetTopAnswerEntry(query_weather_la).full_query_text.empty());

  cache.UpdateRecentAnswers(query_weather_la, query_type);
  EXPECT_TRUE(
      cache.GetTopAnswerEntry(query_weather_lb).full_query_text.empty());
  EXPECT_FALSE(
      cache.GetTopAnswerEntry(query_weather_lv).full_query_text.empty());
  EXPECT_FALSE(
      cache.GetTopAnswerEntry(query_weather_la).full_query_text.empty());
}

TEST(AnswersCacheTest, DuplicateEntries) {
  AnswersCache cache(2);

  std::u16string query_weather_lv = u"weather las vegas";
  std::u16string query_weather_lb = u"weather long beach";
  std::u16string query_weather_l = u"weather l";
  omnibox::AnswerType query_type = omnibox::ANSWER_TYPE_WEATHER;

  cache.UpdateRecentAnswers(query_weather_lb, query_type);
  cache.UpdateRecentAnswers(query_weather_lv, query_type);

  // Test that duplicate entries update recency.
  cache.UpdateRecentAnswers(query_weather_lb, query_type);
  EXPECT_EQ(query_weather_lb,
            cache.GetTopAnswerEntry(query_weather_l).full_query_text);

  // Test duplicate do not evict other entries. LV should still be in cache.
  cache.UpdateRecentAnswers(query_weather_lb, query_type);
  EXPECT_FALSE(
      cache.GetTopAnswerEntry(query_weather_lv).full_query_text.empty());
}
