// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_iterator.h"

#include <string>
#include <vector>

#include "components/query_tiles/internal/tile_group.h"
#include "components/query_tiles/test/test_utils.h"
#include "components/query_tiles/tile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace query_tiles {

TEST(TileIteratorTest, EmtpyTileIterator) {
  TileIterator it(std::vector<const Tile*>(), TileIterator::kAllTiles);
  EXPECT_FALSE(it.HasNext());
  EXPECT_FALSE(it.Next());
}

TEST(TileIteratorTest, EmtpyTileGroup) {
  TileGroup group;
  TileIterator it(group, TileIterator::kAllTiles);
  EXPECT_FALSE(it.HasNext());
  EXPECT_FALSE(it.Next());
}

TEST(TileIteratorTest, TileIterateAllNodes) {
  Tile tile;
  test::ResetTestEntry(&tile);
  TileIterator it({&tile}, TileIterator::kAllTiles);

  // Root level.
  EXPECT_TRUE(it.HasNext());
  EXPECT_EQ(it.Next()->id, "guid-1-1");

  // Level 1 tiles.
  EXPECT_TRUE(it.HasNext());
  EXPECT_EQ(it.Next()->id, "guid-2-1");
  EXPECT_TRUE(it.HasNext());
  EXPECT_EQ(it.Next()->id, "guid-2-2");

  // Level 2 tiles.
  EXPECT_TRUE(it.HasNext());
  EXPECT_EQ(it.Next()->id, "guid-3-1");

  EXPECT_FALSE(it.HasNext());
  EXPECT_FALSE(it.Next());
}

TEST(TileIteratorTest, TileIterateOnlyRoot) {
  Tile tile;
  test::ResetTestEntry(&tile);

  TileIterator it({&tile}, 0);
  EXPECT_TRUE(it.HasNext());
  EXPECT_EQ(it.Next()->id, "guid-1-1");
  EXPECT_FALSE(it.HasNext());
  EXPECT_FALSE(it.Next());
}

TEST(TileIteratorTest, TileIterateWithLevel) {
  Tile tile;
  test::ResetTestEntry(&tile);
  TileIterator it({&tile}, 1);

  // Root level.
  EXPECT_TRUE(it.HasNext());
  EXPECT_EQ(it.Next()->id, "guid-1-1");

  // Level 1 tiles.
  EXPECT_TRUE(it.HasNext());
  EXPECT_EQ(it.Next()->id, "guid-2-1");
  EXPECT_TRUE(it.HasNext());
  EXPECT_EQ(it.Next()->id, "guid-2-2");

  EXPECT_FALSE(it.HasNext());
  EXPECT_FALSE(it.Next());
}

TEST(TileIteratorTest, TileGroupIterate) {
  TileGroup group;
  test::ResetTestGroup(&group);
  TileIterator it(group, 1);

  // Root level tiles.
  EXPECT_TRUE(it.HasNext());
  EXPECT_EQ(it.Next()->id, "guid-1-1");
  EXPECT_TRUE(it.HasNext());
  EXPECT_EQ(it.Next()->id, "guid-1-2");
  EXPECT_TRUE(it.HasNext());
  EXPECT_EQ(it.Next()->id, "guid-1-3");

  // Level 1 tiles.
  EXPECT_TRUE(it.HasNext());
  EXPECT_EQ(it.Next()->id, "guid-2-1");
  EXPECT_TRUE(it.HasNext());
  EXPECT_EQ(it.Next()->id, "guid-2-2");
  EXPECT_TRUE(it.HasNext());
  EXPECT_EQ(it.Next()->id, "guid-1-4");

  EXPECT_FALSE(it.HasNext());
  EXPECT_FALSE(it.Next());
}

}  // namespace query_tiles