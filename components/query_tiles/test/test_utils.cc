// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/test/test_utils.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"

namespace query_tiles {
namespace test {

void ResetTestEntry(Tile* entry) {
  entry->id = "guid-1-1";
  entry->query_text = "test query str";
  entry->display_text = "test display text";
  entry->accessibility_text = "read this test display text";
  entry->image_metadatas.clear();
  entry->image_metadatas.emplace_back(GURL("http://www.example.com"));
  entry->image_metadatas.emplace_back(GURL("http://www.fakeurl.com"));

  auto entry1 = std::make_unique<Tile>();
  entry1->id = "guid-2-1";
  auto entry2 = std::make_unique<Tile>();
  entry2->id = "guid-2-2";
  auto entry3 = std::make_unique<Tile>();
  entry3->id = "guid-3-1";
  entry1->sub_tiles.emplace_back(std::move(entry3));
  entry->sub_tiles.clear();
  entry->sub_tiles.emplace_back(std::move(entry1));
  entry->sub_tiles.emplace_back(std::move(entry2));
}

void ResetTestGroup(TileGroup* group) {
  ResetTestGroup(group, base::Time::Now() - base::Days(7));
}

void ResetTestGroup(TileGroup* group, base::Time last_updated_ts) {
  group->id = "group_guid";
  group->locale = "en-US";
  // Convert time due to precision we used in proto message conversion.
  int64_t milliseconds =
      last_updated_ts.ToDeltaSinceWindowsEpoch().InMilliseconds();
  group->last_updated_ts =
      base::Time::FromDeltaSinceWindowsEpoch(base::Milliseconds(milliseconds));
  group->tiles.clear();
  auto test_entry_1 = std::make_unique<Tile>();
  ResetTestEntry(test_entry_1.get());
  auto test_entry_2 = std::make_unique<Tile>();
  test_entry_2->id = "guid-1-2";
  auto test_entry_3 = std::make_unique<Tile>();
  test_entry_3->id = "guid-1-3";
  auto test_entry_4 = std::make_unique<Tile>();
  test_entry_4->id = "guid-1-4";
  test_entry_3->sub_tiles.emplace_back(std::move(test_entry_4));
  group->tiles.emplace_back(std::move(test_entry_1));
  group->tiles.emplace_back(std::move(test_entry_2));
  group->tiles.emplace_back(std::move(test_entry_3));
  group->tile_stats["guid-1-1"] = TileStats(group->last_updated_ts, 0.5);
  group->tile_stats["guid-1-2"] = TileStats(group->last_updated_ts, 0.2);
  group->tile_stats["guid-1-3"] = TileStats(group->last_updated_ts, 0.7);
  group->tile_stats["guid-1-4"] = TileStats(group->last_updated_ts, 0.4);
  group->tile_stats["guid-2-1"] = TileStats(group->last_updated_ts, 0.3);
  group->tile_stats["guid-2-2"] = TileStats(group->last_updated_ts, 0.6);
  group->tile_stats["guid-3-1"] = TileStats(group->last_updated_ts, 0.5);
}

std::vector<std::unique_ptr<Tile>> GetTestTrendingTileList() {
  auto trending_tile1 = std::make_unique<Tile>();
  trending_tile1->id = "trending_1";

  auto trending_tile2 = std::make_unique<Tile>();
  trending_tile2->id = "trending_2";

  auto trending_tile3 = std::make_unique<Tile>();
  trending_tile3->id = "trending_3";

  std::vector<std::unique_ptr<Tile>> trending_tile_list;
  trending_tile_list.emplace_back(std::move(trending_tile1));
  trending_tile_list.emplace_back(std::move(trending_tile2));
  trending_tile_list.emplace_back(std::move(trending_tile3));
  return trending_tile_list;
}

bool AreTileGroupsIdentical(const TileGroup& lhs, const TileGroup& rhs) {
  if (lhs != rhs)
    return false;

  for (const auto& it : lhs.tiles) {
    auto* target = it.get();
    auto found = base::ranges::find(rhs.tiles, target->id, &Tile::id);
    if (found == rhs.tiles.end() || *target != *found->get())
      return false;
  }

  return lhs.tile_stats == rhs.tile_stats;
}

bool AreTilesIdentical(const Tile& lhs, const Tile& rhs) {
  if (lhs != rhs)
    return false;

  for (const auto& it : lhs.image_metadatas) {
    if (!base::Contains(rhs.image_metadatas, it))
      return false;
  }

  for (const auto& it : lhs.sub_tiles) {
    auto* target = it.get();
    auto found = base::ranges::find(rhs.sub_tiles, target->id, &Tile::id);
    if (found == rhs.sub_tiles.end() ||
        !AreTilesIdentical(*target, *found->get()))
      return false;
  }
  return true;
}

bool AreTilesIdentical(std::vector<Tile*> lhs, std::vector<Tile*> rhs) {
  std::vector<Tile> lhs_copy, rhs_copy;
  for (auto* tile : lhs)
    lhs_copy.emplace_back(*tile);
  for (auto* tile : rhs)
    rhs_copy.emplace_back(*tile);
  return AreTilesIdentical(std::move(lhs_copy), std::move(rhs_copy));
}

bool AreTilesIdentical(std::vector<Tile> lhs, std::vector<Tile> rhs) {
  if (lhs.size() != rhs.size())
    return false;

  auto entry_comparator = [](const Tile& a, const Tile& b) {
    return a.id < b.id;
  };

  std::sort(lhs.begin(), lhs.end(), entry_comparator);
  std::sort(rhs.begin(), rhs.end(), entry_comparator);

  for (size_t i = 0; i < lhs.size(); i++) {
    if (!AreTilesIdentical(lhs[i], rhs[i]))
      return false;
  }

  return true;
}

}  // namespace test

}  // namespace query_tiles
