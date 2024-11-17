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
  return (a.title == b.title) && (a.url == b.url) && (a.source == b.source) &&
         (a.title_source == b.title_source) &&
         (a.favicon_url == b.favicon_url) &&
         (a.from_most_visited == b.from_most_visited);
}

bool operator!=(const NTPTile& a, const NTPTile& b) {
  return !(a == b);
}

}  // namespace ntp_tiles
