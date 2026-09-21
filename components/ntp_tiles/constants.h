// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_CONSTANTS_H_
#define COMPONENTS_NTP_TILES_CONSTANTS_H_

#include <stddef.h>

namespace ntp_tiles {

// URL for AI Mode on the search results page.
inline constexpr char kAiModeTileUrl[] = "http://google.com/aimode";

// Maximum number of custom links that can be set by the user. Used on desktop.
extern const size_t kMaxNumCustomLinks;

// Maximum number of Most Visited sites that will be generated. Used on desktop.
extern const size_t kMaxNumMostVisited;

// Maximum number of tiles that can be shown on the NTP.
const int kMaxNumTiles = 10;

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_CONSTANTS_H_
