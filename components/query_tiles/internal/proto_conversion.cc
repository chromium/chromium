// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/proto_conversion.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"

using TileStatsProto = query_tiles::proto::TileStats;

namespace query_tiles {
namespace {

// Helper method to convert base::Time to integer for serialization. Loses
// precision beyond milliseconds.
int64_t TimeToMilliseconds(const base::Time& time) {
  return time.ToDeltaSinceWindowsEpoch().InMilliseconds();
}

// Helper method to convert serialized time as integer to base::Time for
// deserialization. Loses precision beyond milliseconds.
base::Time MillisecondsToTime(int64_t serialized_time_ms) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Milliseconds(serialized_time_ms));
}

// Use to convert TileInfo in response proto to the local Tile structure.
void ResponseToTile(
    const ResponseTileProto& response,
    Tile* tile,
    std::map<std::string, ResponseTileProto> sub_tiles_from_response) {
  tile->id = response.tile_id();
  tile->display_text = response.display_text();
  tile->accessibility_text = response.accessibility_text();
  tile->query_text = response.query_string();

  for (const auto& image : response.tile_images())
    tile->image_metadatas.emplace_back(GURL(image.url()));

  for (const auto& search_param : response.search_params())
    tile->search_params.emplace_back(search_param);

  for (const auto& id : response.sub_tile_ids()) {
    if (sub_tiles_from_response.count(id)) {
      auto sub_tile_from_response = sub_tiles_from_response.at(id);
      auto new_sub_tile = std::make_unique<Tile>();
      ResponseToTile(sub_tile_from_response, new_sub_tile.get(),
                     sub_tiles_from_response);
      tile->sub_tiles.emplace_back(std::move(new_sub_tile));
    }
  }
}

}  // namespace

void TileToProto(Tile* entry, TileProto* proto) {
  DCHECK(entry);
  DCHECK(proto);
  proto->set_id(entry->id);
  proto->set_query_text(entry->query_text);
  proto->set_display_text(entry->display_text);
  proto->set_accessibility_text(entry->accessibility_text);

  // Set ImageMetadatas.
  for (auto& image : entry->image_metadatas) {
    auto* data = proto->add_image_metadatas();
    data->set_url(image.url.spec());
  }

  for (auto& search_param : entry->search_params) {
    auto* param = proto->add_search_params();
    *param = search_param;
  }

  // Set children.
  for (auto& subtile : entry->sub_tiles) {
    TileToProto(subtile.get(), proto->add_sub_tiles());
  }
}

void TileFromProto(TileProto* proto, Tile* entry) {
  DCHECK(entry);
  DCHECK(proto);
  entry->id = proto->id();
  entry->query_text = proto->query_text();
  entry->display_text = proto->display_text();
  entry->accessibility_text = proto->accessibility_text();

  for (const auto& image_md : proto->image_metadatas())
    entry->image_metadatas.emplace_back(GURL(image_md.url()));

  for (const auto& search_param : proto->search_params())
    entry->search_params.emplace_back(search_param);

  for (int i = 0; i < proto->sub_tiles_size(); i++) {
    auto sub_tile_proto = proto->sub_tiles(i);
    auto child = std::make_unique<Tile>();
    TileFromProto(&sub_tile_proto, child.get());
    entry->sub_tiles.emplace_back(std::move(child));
  }
}

void TileGroupToProto(TileGroup* group, TileGroupProto* proto) {
  proto->set_id(group->id);
  proto->set_locale(group->locale);
  proto->set_last_updated_time_ms(TimeToMilliseconds(group->last_updated_ts));
  for (auto& tile : group->tiles) {
    TileToProto(tile.get(), proto->add_tiles());
  }
  auto& map = *(proto->mutable_tile_stats());
  for (auto& entry : group->tile_stats) {
    TileStatsProto stats;
    stats.set_score(entry.second.score);
    stats.set_last_clicked_time_ms(
        TimeToMilliseconds(entry.second.last_clicked_time));
    map[entry.first] = stats;
  }
}

void TileGroupFromProto(TileGroupProto* proto, TileGroup* group) {
  group->id = proto->id();
  group->locale = proto->locale();
  group->last_updated_ts = MillisecondsToTime(proto->last_updated_time_ms());
  for (int i = 0; i < proto->tiles().size(); i++) {
    auto entry_proto = proto->tiles(i);
    auto child = std::make_unique<Tile>();
    TileFromProto(&entry_proto, child.get());
    group->tiles.emplace_back(std::move(child));
  }
  for (auto& entry : proto->tile_stats()) {
    group->tile_stats[entry.first] =
        TileStats(MillisecondsToTime(entry.second.last_clicked_time_ms()),
                  entry.second.score());
  }
}

void TileGroupFromResponse(const ResponseGroupProto& response,
                           TileGroup* tile_group) {
  if (!response.has_tile_group())
    return;
  std::vector<ResponseTileProto> top_level_tiles;
  std::map<std::string, ResponseTileProto> sub_tiles;
  tile_group->locale = response.tile_group().locale();
  for (const auto& tile : response.tile_group().tiles()) {
    if (tile.is_top_level()) {
      top_level_tiles.emplace_back(tile);
    } else {
      sub_tiles[tile.tile_id()] = tile;
    }
  }

  for (const auto& top_level_tile : top_level_tiles) {
    auto new_tile = std::make_unique<Tile>();
    ResponseToTile(top_level_tile, new_tile.get(), sub_tiles);
    tile_group->tiles.emplace_back(std::move(new_tile));
  }
}
}  // namespace query_tiles
