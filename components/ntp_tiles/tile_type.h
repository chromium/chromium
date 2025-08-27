// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_TILE_TYPE_H_
#define COMPONENTS_NTP_TILES_TILE_TYPE_H_

namespace ntp_tiles {

// The enum demonstrates the type of a tile.
//
// It is used to categorize NTP tiles based on their origin for the following
// cases:
// * To track tiles that have been deleted post default app migration.
// * To track which tile type to show in the New Tab page.
//
// These values must stay in sync with the TypeOfDeletedMostVisitedApp enum in
// enums.xml and `kNtpShortcutsType` pref.

enum class TileType {
  // Tile with top sites type.
  kTopSites = 0,

  // Tile with custom links type.
  kCustomLinks = 1,

  kMaxValue = kCustomLinks
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_TILE_TYPE_H_
