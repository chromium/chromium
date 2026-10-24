// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/content_extraction_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/local_search_service/shared_structs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::local_search_service {

namespace {

constexpr double kDefaultWeight = 1.0;

}  // namespace

TEST(ContentExtractionUtilsTest, ConsolidateTokenTest) {
  {
    const std::u16string text(
        u"Check duplicate. Duplicate is ##$%^&##$%##$^##$ bad");
    const auto tokens = ConsolidateToken(
        ExtractContent("3rd test", text, kDefaultWeight, "en"));
    EXPECT_EQ(tokens.size(), 3u);

    bool found = false;
    for (const auto& token : tokens) {
      if (token.content == u"duplicate") {
        found = true;
        EXPECT_EQ(token.positions[0].weight, kDefaultWeight);
        EXPECT_EQ(token.positions[0].position.content_id, "3rd test");
        EXPECT_EQ(token.positions[0].position.start, 6u);
        EXPECT_EQ(token.positions[0].position.length, 9u);
        EXPECT_EQ(token.positions[1].weight, kDefaultWeight);
        EXPECT_EQ(token.positions[1].position.start, 17u);
        EXPECT_EQ(token.positions[1].position.length, 9u);
      }
    }
    EXPECT_TRUE(found);
  }
  {
    std::vector<Token> sources = {
        Token(u"A",
              {WeightedPosition(kDefaultWeight, Position("ID1", 1u, 1u)),
               WeightedPosition(kDefaultWeight, Position("ID1", 3u, 1u))}),
        Token(u"B", {WeightedPosition(kDefaultWeight, Position("ID1", 5, 1))}),
        // A different weight for content "ID2".
        Token(u"A",
              {WeightedPosition(kDefaultWeight / 2, Position("ID2", 10, 1))})};
    const auto tokens = ConsolidateToken(sources);
    EXPECT_EQ(tokens.size(), 2u);

    bool found = false;
    for (const auto& token : tokens) {
      if (token.content == u"A") {
        found = true;
        EXPECT_EQ(token.positions[0].weight, kDefaultWeight);
        EXPECT_EQ(token.positions[0].position.content_id, "ID1");
        EXPECT_EQ(token.positions[0].position.start, 1u);
        EXPECT_EQ(token.positions[0].position.length, 1u);
        EXPECT_EQ(token.positions[1].weight, kDefaultWeight);
        EXPECT_EQ(token.positions[1].position.content_id, "ID1");
        EXPECT_EQ(token.positions[1].position.start, 3u);
        EXPECT_EQ(token.positions[1].position.length, 1u);
        EXPECT_EQ(token.positions[2].weight, kDefaultWeight / 2);
        EXPECT_EQ(token.positions[2].position.content_id, "ID2");
        EXPECT_EQ(token.positions[2].position.start, 10u);
        EXPECT_EQ(token.positions[2].position.length, 1u);
      }
    }
    EXPECT_TRUE(found);
  }
}

TEST(ContentExtractionUtilsTest, ExtractContentTest) {
  {
    const std::u16string text(
        u"Normal... English!!! paragraph: email@gmail.com. Here is a link: "
        u"https://google.com, ip=8.8.8.8");
    const auto tokens =
        ExtractContent("first test", text, kDefaultWeight / 2, "en");
    EXPECT_EQ(tokens.size(), 7u);

    EXPECT_EQ(tokens[1].content, u"english");
    EXPECT_EQ(tokens[1].positions[0].weight, kDefaultWeight / 2);
    EXPECT_EQ(tokens[1].positions[0].position.content_id, "first test");
    EXPECT_EQ(tokens[1].positions[0].position.start, 10u);
    EXPECT_EQ(tokens[1].positions[0].position.length, 7u);
  }
  {
    const std::u16string text(u"##$%#^你好!!!");
    const auto tokens = ExtractContent("2nd test", text, kDefaultWeight, "zh");
    EXPECT_EQ(tokens.size(), 1u);

    EXPECT_EQ(tokens[0].content, u"你好");
    EXPECT_EQ(tokens[0].positions[0].weight, kDefaultWeight);
    EXPECT_EQ(tokens[0].positions[0].position.content_id, "2nd test");
    EXPECT_EQ(tokens[0].positions[0].position.start, 6u);
    EXPECT_EQ(tokens[0].positions[0].position.length, 2u);
  }
}

TEST(ContentExtractionUtilsTest, StopwordTest) {
  // Non English.
  EXPECT_FALSE(IsStopword(u"was", "vn"));

  // English.
  EXPECT_TRUE(IsStopword(u"i", "en-US"));
  EXPECT_TRUE(IsStopword(u"my", "en"));
  EXPECT_FALSE(IsStopword(u"stopword", "en"));
}

TEST(ContentExtractionUtilsTest, NormalizerTest) {
  // Test diacritic removed.
  EXPECT_EQ(Normalizer(u"các dấu câu đã được loại bỏ thành công"),
            u"cac dau cau da duoc loai bo thanh cong");

  // Test hyphens removed.
  EXPECT_EQ(Normalizer(u"wi\u2015fi----", true), u"wifi");

  // Keep hyphen.
  EXPECT_EQ(Normalizer(u"wi-fi", false), u"wi-fi");

  // Case folding test.
  EXPECT_EQ(Normalizer(u"This Is sOmE WEIRD LooKing text"),
            u"this is some weird looking text");

  // Combine test.
  EXPECT_EQ(
      Normalizer(u"Đây là MỘT trình duyệt tuyệt vời và mượt\u2014\u058Amà",
                 true),
      u"day la mot trinh duyet tuyet voi va muotma");
}

}  // namespace ash::local_search_service
