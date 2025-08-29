// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/ntp_tile.h"

namespace ntp_tiles {

NTPTile::NTPTile()
    : title_source(TileTitleSource::UNKNOWN), source(TileSource::TOP_SITES) {}

NTPTile::NTPTile(const NTPTile&) = default;

NTPTile::~NTPTile() = default;

bool operator==(const NTPTile& a, const NTPTile& b) {
  bool are_equal =
      (a.title == b.title) && (a.url == b.url) && (a.source == b.source) &&
      (a.title_source == b.title_source) && (a.favicon_url == b.favicon_url) &&
      (a.from_most_visited == b.from_most_visited);
#if !BUILDFLAG(IS_ANDROID)
  are_equal = are_equal && (a.allow_user_edit == b.allow_user_edit) &&
              (a.allow_user_delete == b.allow_user_delete);
#endif
  return are_equal;
}

}  // namespace ntp_tiles
