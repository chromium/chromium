// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/test/fake_server_response.h"

#include "base/strings/string_number_conversions.h"
#include "components/query_tiles/internal/tile_fetcher.h"
#include "components/query_tiles/proto/tile_response.pb.h"

namespace query_tiles {
namespace {

constexpr char kTestLocale[] = "en";

std::string BuildPrefix(const std::string& prefix, size_t pos) {
  return prefix + "_" + base::NumberToString(pos);
}

void CreateTiles(proto::TileInfoGroup* info_group,
                 proto::TileInfo* parent,
                 const std::string& prefix,
                 int levels,
                 size_t tiles_per_level) {
  if (levels <= 0)
    return;

  // Add sub-tiles.
  for (size_t j = 0; j < tiles_per_level; j++) {
    std::string subprefix = BuildPrefix(prefix, j);
    parent->add_sub_tile_ids(subprefix + "_id");
    auto* new_tile = info_group->add_tiles();
    new_tile->set_tile_id(subprefix + "_id");
    new_tile->set_display_text(subprefix + "_display_text");
    new_tile->set_accessibility_text(subprefix + "_accessibility_text");
    new_tile->set_query_string(subprefix + "_query_string");
    new_tile->set_is_top_level(false);

    // Add sub-tiles.
    CreateTiles(info_group, new_tile, subprefix, levels - 1, tiles_per_level);
  }
}

// Build a fake two level response proto.
void InitResponseProto(proto::ServerResponse* response,
                       int levels,
                       size_t tiles_per_level) {
  proto::TileInfoGroup* info_group = response->mutable_tile_group();
  info_group->set_locale(kTestLocale);
  // Add top level tiles.
  for (size_t i = 0; i < tiles_per_level; i++) {
    auto* new_top_level_tile = info_group->add_tiles();
    std::string prefix = BuildPrefix("Tile", i);
    new_top_level_tile->set_tile_id(prefix + "_id");
    new_top_level_tile->set_display_text(prefix + "_display_text");
    new_top_level_tile->set_accessibility_text(prefix + "_accessibility_text");
    new_top_level_tile->set_query_string(prefix + "_query_string");
    new_top_level_tile->set_is_top_level(true);

    // Add sub-tiles.
    CreateTiles(info_group, new_top_level_tile, prefix, levels - 1,
                tiles_per_level);
  }
}

}  // namespace

// static
void FakeServerResponse::SetTileFetcherServerURL(const GURL& url) {
  TileFetcher::SetOverrideURLForTesting(url);
}

// static
std::string FakeServerResponse::CreateServerResponseProto(int levels,
                                                          int tiles_per_level) {
  proto::ServerResponse server_response;
  InitResponseProto(&server_response, levels, tiles_per_level);

  std::string response_str;
  CHECK(server_response.SerializeToString(&response_str));
  return response_str;
}

}  // namespace query_tiles
