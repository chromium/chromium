// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_group.h"

#include <iostream>
#include <type_traits>
#include <utility>

#include "base/test/task_environment.h"
#include "components/query_tiles/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace query_tiles {
namespace {

TEST(TileGroupTest, CompareOperators) {
  base::Time last_updated_ts = base::Time::Now() - base::Days(7);
  TileGroup lhs, rhs;
  test::ResetTestGroup(&lhs, last_updated_ts);
  test::ResetTestGroup(&rhs, last_updated_ts);
  EXPECT_EQ(lhs, rhs);

  rhs.id = "changed";
  EXPECT_NE(lhs, rhs);
  test::ResetTestGroup(&rhs);

  rhs.locale = "changed";
  EXPECT_NE(lhs, rhs);
  test::ResetTestGroup(&rhs);

  rhs.last_updated_ts += base::Days(1);
  EXPECT_NE(lhs, rhs);
  test::ResetTestGroup(&rhs);

  rhs.tiles.clear();
  EXPECT_NE(lhs, rhs);
}

TEST(TileGroupTest, DeepCompareOperators) {
  base::Time last_updated_ts = base::Time::Now() - base::Days(7);
  TileGroup lhs, rhs;
  test::ResetTestGroup(&lhs, last_updated_ts);
  test::ResetTestGroup(&rhs, last_updated_ts);
  EXPECT_TRUE(test::AreTileGroupsIdentical(lhs, rhs));

  // Verify the order of tiles does not matter.
  std::reverse(rhs.tiles.begin(), rhs.tiles.end());
  EXPECT_TRUE(test::AreTileGroupsIdentical(lhs, rhs));
  test::ResetTestGroup(&rhs);

  // Verify change on children tiles will make them different.
  rhs.tiles.front()->id = "changed";
  EXPECT_FALSE(test::AreTileGroupsIdentical(lhs, rhs));
}

TEST(TileGroupTest, CopyOperator) {
  TileGroup lhs;
  test::ResetTestGroup(&lhs);
  TileGroup rhs = lhs;
  EXPECT_TRUE(test::AreTileGroupsIdentical(lhs, rhs));
}

TEST(TileGroupTest, MoveOperator) {
  TileGroup lhs;
  base::Time last_updated_ts = base::Time::Now() - base::Days(7);
  test::ResetTestGroup(&lhs, last_updated_ts);
  TileGroup rhs = std::move(lhs);
  TileGroup expected;
  test::ResetTestGroup(&expected, last_updated_ts);
  EXPECT_TRUE(test::AreTileGroupsIdentical(expected, rhs));
}

TEST(TileGroupTest, OnTileClicked) {
  base::Time now_time = base::Time::Now();
  TileGroup group;
  group.tile_stats["guid-1-1"] = TileStats(now_time, 0);
  group.tile_stats["guid-1-2"] = TileStats(now_time + base::Hours(1), 0.5);
  group.OnTileClicked("guid-1-1");
  EXPECT_EQ(group.tile_stats["guid-1-1"].score, 1);
  group.OnTileClicked("guid-1-2");
  EXPECT_EQ(group.tile_stats["guid-1-2"].score, 1.5);
}

TEST(TileGroupTest, RemoveTiles) {
  TileGroup group;
  test::ResetTestGroup(&group);
  EXPECT_EQ(group.tiles.size(), 3u);
  EXPECT_EQ(group.tiles[2]->id, "guid-1-3");
  EXPECT_FALSE(group.tiles[2]->sub_tiles.empty());

  std::vector<std::string> tiles_to_remove;
  tiles_to_remove.emplace_back("guid-1-1");
  group.RemoveTiles(tiles_to_remove);
  EXPECT_EQ(group.tiles.size(), 2u);
  EXPECT_EQ(group.tiles[0]->id, "guid-1-2");
  EXPECT_EQ(group.tiles[1]->id, "guid-1-3");

  test::ResetTestGroup(&group);
  tiles_to_remove.clear();
  tiles_to_remove.emplace_back("guid-1-4");
  group.RemoveTiles(tiles_to_remove);
  EXPECT_EQ(group.tiles.size(), 3u);
  EXPECT_EQ(group.tiles[2]->id, "guid-1-3");
  EXPECT_TRUE(group.tiles[2]->sub_tiles.empty());

  // Remove 2 tiles.
  test::ResetTestGroup(&group);
  tiles_to_remove.emplace_back("guid-1-1");
  group.RemoveTiles(tiles_to_remove);
  EXPECT_EQ(group.tiles.size(), 2u);
  EXPECT_EQ(group.tiles[0]->id, "guid-1-2");
  EXPECT_EQ(group.tiles[1]->id, "guid-1-3");
  EXPECT_TRUE(group.tiles[1]->sub_tiles.empty());
}

}  // namespace

}  // namespace query_tiles
