// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/test/test_utils.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace query_tiles {
namespace test {

namespace {

const char kTimeStr[] = "03/18/20 01:00:00 AM";

}  // namespace

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
  group->id = "group_guid";
  group->locale = "en-US";
  bool success = base::Time::FromString(kTimeStr, &group->last_updated_ts);
  CHECK(success);
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

bool AreTileGroupsIdentical(const TileGroup& lhs, const TileGroup& rhs) {
  if (lhs != rhs)
    return false;

  for (const auto& it : lhs.tiles) {
    auto* target = it.get();
    auto found = std::find_if(rhs.tiles.begin(), rhs.tiles.end(),
                              [&target](const std::unique_ptr<Tile>& entry) {
                                return entry->id == target->id;
                              });
    if (found == rhs.tiles.end() || *target != *found->get())
      return false;
  }

  return lhs.tile_stats == rhs.tile_stats;
}

bool AreTilesIdentical(const Tile& lhs, const Tile& rhs) {
  if (lhs != rhs)
    return false;

  for (const auto& it : lhs.image_metadatas) {
    auto found =
        std::find_if(rhs.image_metadatas.begin(), rhs.image_metadatas.end(),
                     [it](const ImageMetadata& image) { return image == it; });
    if (found == rhs.image_metadatas.end())
      return false;
  }

  for (const auto& it : lhs.sub_tiles) {
    auto* target = it.get();
    auto found = std::find_if(rhs.sub_tiles.begin(), rhs.sub_tiles.end(),
                              [&target](const std::unique_ptr<Tile>& entry) {
                                return entry->id == target->id;
                              });
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
