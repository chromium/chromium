// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/tile.h"

#include <utility>

#include "base/test/task_environment.h"
#include "components/query_tiles/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace query_tiles {
namespace {

TEST(TileTest, CompareOperators) {
  Tile lhs, rhs;
  test::ResetTestEntry(&lhs);
  test::ResetTestEntry(&rhs);
  EXPECT_EQ(lhs, rhs);
  EXPECT_FALSE(lhs != rhs);

  rhs.id = "changed";
  EXPECT_NE(lhs, rhs);
  test::ResetTestEntry(&rhs);

  rhs.query_text = "changed";
  EXPECT_NE(lhs, rhs);
  test::ResetTestEntry(&rhs);

  rhs.display_text = "changed";
  EXPECT_NE(lhs, rhs);
  test::ResetTestEntry(&rhs);

  rhs.accessibility_text = "changed";
  EXPECT_NE(lhs, rhs);
  test::ResetTestEntry(&rhs);

  rhs.search_params = {"xyz=1"};
  EXPECT_NE(lhs, rhs);
  test::ResetTestEntry(&rhs);
}

TEST(TileTest, DeepComparison) {
  Tile lhs, rhs;
  test::ResetTestEntry(&lhs);
  test::ResetTestEntry(&rhs);
  EXPECT_TRUE(test::AreTilesIdentical(lhs, rhs));

  // Test image metadatas changed.
  rhs.image_metadatas.front().url = GURL("http://www.url-changed.com");
  EXPECT_FALSE(test::AreTilesIdentical(lhs, rhs));
  test::ResetTestEntry(&rhs);

  rhs.image_metadatas.pop_back();
  EXPECT_FALSE(test::AreTilesIdentical(lhs, rhs));
  test::ResetTestEntry(&rhs);

  rhs.image_metadatas.emplace_back(ImageMetadata());
  EXPECT_FALSE(test::AreTilesIdentical(lhs, rhs));
  test::ResetTestEntry(&rhs);

  std::reverse(rhs.image_metadatas.begin(), rhs.image_metadatas.end());
  EXPECT_TRUE(test::AreTilesIdentical(lhs, rhs));
  test::ResetTestEntry(&rhs);

  // Test children changed.
  rhs.sub_tiles.front()->id = "changed";
  EXPECT_FALSE(test::AreTilesIdentical(lhs, rhs));
  test::ResetTestEntry(&rhs);

  rhs.sub_tiles.pop_back();
  EXPECT_FALSE(test::AreTilesIdentical(lhs, rhs));
  test::ResetTestEntry(&rhs);

  rhs.sub_tiles.emplace_back(std::make_unique<Tile>());
  EXPECT_FALSE(test::AreTilesIdentical(lhs, rhs));
  test::ResetTestEntry(&rhs);

  std::reverse(rhs.sub_tiles.begin(), rhs.sub_tiles.end());
  EXPECT_TRUE(test::AreTilesIdentical(lhs, rhs));
}

TEST(TileTest, CopyOperator) {
  Tile lhs;
  test::ResetTestEntry(&lhs);
  Tile rhs(lhs);
  EXPECT_TRUE(test::AreTilesIdentical(lhs, rhs));
}

TEST(TileTest, AssignOperator) {
  Tile lhs;
  test::ResetTestEntry(&lhs);
  Tile rhs = lhs;
  EXPECT_TRUE(test::AreTilesIdentical(lhs, rhs));
}

TEST(TileTest, MoveOperator) {
  Tile lhs;
  test::ResetTestEntry(&lhs);
  Tile rhs = std::move(lhs);
  Tile expected;
  test::ResetTestEntry(&expected);
  EXPECT_TRUE(test::AreTilesIdentical(expected, rhs));
}

}  // namespace

}  // namespace query_tiles
