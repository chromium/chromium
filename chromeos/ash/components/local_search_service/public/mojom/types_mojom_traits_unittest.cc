// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/public/mojom/types_mojom_traits.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/local_search_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::local_search_service {

TEST(LocalSearchMojomTraitsTest, ContentTraits) {
  Content input("id", u"content", 0.3);
  Content output;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::Content>(input, output));
  EXPECT_EQ(input.id, output.id);
  EXPECT_EQ(input.content, output.content);
  EXPECT_EQ(input.weight, output.weight);
}

TEST(LocalSearchMojomTraitsTest, DataTraits) {
  std::vector<Content> contents{Content("id1", u"contents1", 0.1),
                                Content("id2", u"contents2", 0.2)};

  {
    // Empty locale.
    Data input("id", contents);
    Data output;

    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::Data>(input, output));
    EXPECT_EQ(input.id, output.id);
    EXPECT_EQ(input.contents[0].id, output.contents[0].id);
    EXPECT_EQ(input.contents[0].content, output.contents[0].content);
    EXPECT_EQ(input.contents[0].weight, output.contents[0].weight);
    EXPECT_EQ(input.contents[1].id, output.contents[1].id);
    EXPECT_EQ(input.contents[1].content, output.contents[1].content);
    EXPECT_EQ(input.contents[1].weight, output.contents[1].weight);
    EXPECT_EQ(input.locale, output.locale);
  }

  {
    // Non-empty locale.
    Data input("id", contents, "en");
    Data output;

    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::Data>(input, output));
    EXPECT_EQ(input.id, output.id);
    EXPECT_EQ(input.contents[0].id, output.contents[0].id);
    EXPECT_EQ(input.contents[0].content, output.contents[0].content);
    EXPECT_EQ(input.contents[0].weight, output.contents[0].weight);
    EXPECT_EQ(input.contents[1].id, output.contents[1].id);
    EXPECT_EQ(input.contents[1].content, output.contents[1].content);
    EXPECT_EQ(input.contents[1].weight, output.contents[1].weight);
    EXPECT_EQ(input.locale, output.locale);
  }
}

TEST(LocalSearchMojomTraitsTest, SearchParamsTraits) {
  SearchParams input{0.1, 0.2, 0.3};
  SearchParams output;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::SearchParams>(input, output));
  EXPECT_EQ(input.relevance_threshold, output.relevance_threshold);
  EXPECT_EQ(input.prefix_threshold, output.prefix_threshold);
  EXPECT_EQ(input.fuzzy_threshold, output.fuzzy_threshold);
}

TEST(LocalSearchMojomTraitsTest, PositionTraits) {
  Position input{"content_id", 12, 7};
  Position output;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::Position>(input, output));
  EXPECT_EQ(input.content_id, output.content_id);
  EXPECT_EQ(input.start, output.start);
  EXPECT_EQ(input.length, output.length);
}

TEST(LocalSearchMojomTraitsTest, ResultTraits) {
  Result input;
  input.id = "id";
  input.score = 1.3;
  input.positions =
      std::vector<Position>{Position{"id1", 1, 1}, Position{"id2", 2, 2}};
  Result output;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::Result>(input, output));
  EXPECT_EQ(input.id, output.id);
  EXPECT_EQ(input.score, output.score);
  EXPECT_EQ(input.positions[0].content_id, output.positions[0].content_id);
  EXPECT_EQ(input.positions[0].start, output.positions[0].start);
  EXPECT_EQ(input.positions[0].length, output.positions[0].length);
  EXPECT_EQ(input.positions[1].content_id, output.positions[1].content_id);
  EXPECT_EQ(input.positions[1].start, output.positions[1].start);
  EXPECT_EQ(input.positions[1].length, output.positions[1].length);
}

}  // namespace ash::local_search_service
