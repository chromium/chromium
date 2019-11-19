// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_TILE_TITLE_SOURCE_H_
#define COMPONENTS_NTP_TILES_TILE_TITLE_SOURCE_H_

namespace ntp_tiles {

// The source where the displayed title of an NTP tile originates from.
//
// These values must stay in sync with the NTPTileTitleSource enums in
// enums.xml AND in chrome/browser/resources/local_ntp/most_visited_single.js.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.suggestions.tile
enum class TileTitleSource {
  // The title might be invalid, aggregated, user-set, extracted from history,
  // not loaded or simply not known.
  UNKNOWN = 0,

  // The site's manifest contained a usable "(short_)name" attribute.
  MANIFEST = 1,

  // The site provided a meta tag (e.g. OpenGraph's site_name).
  META_TAG = 2,

  // The site's title is used as tile title, extracted from the title tag.
  TITLE_TAG = 3,

  // The title was inferred from multiple signals (e.g. meta tags, url, title).
  INFERRED = 4,

  // The maximum tile title source value that gets recorded in UMA.
  LAST = INFERRED
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_TILE_TITLE_SOURCE_H_
