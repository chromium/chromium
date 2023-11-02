// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_DELETED_TILE_TYPE_H_
#define COMPONENTS_NTP_TILES_DELETED_TILE_TYPE_H_

namespace ntp_tiles {

// The enum demonstrates the type of tile that has been deleted post default
// app migration as part of the bug fix.
//
// These values must stay in sync with the TypeOfDeletedMostVisitedApp enum in
// enums.xml.

enum class DeletedTileType {
  // Default app from most visited sites.
  kMostVisitedSite = 0,

  // Default app from custom links.
  kCustomLink = 1,

  kMaxValue = kCustomLink
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_DELETED_TILE_TYPE_H_
