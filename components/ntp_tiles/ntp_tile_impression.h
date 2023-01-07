// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_NTP_TILE_IMPRESSION_H_
#define COMPONENTS_NTP_TILES_NTP_TILE_IMPRESSION_H_

#include "base/time/time.h"
#include "components/favicon_base/favicon_types.h"
#include "components/ntp_tiles/tile_source.h"
#include "components/ntp_tiles/tile_title_source.h"
#include "components/ntp_tiles/tile_visual_type.h"
#include "url/gurl.h"

namespace ntp_tiles {

struct NTPTileImpression {
  // Default constructor needed for Mojo.
  NTPTileImpression();
  NTPTileImpression(int index,
                    TileSource source,
                    TileTitleSource title_source,
                    TileVisualType visual_type,
                    favicon_base::IconType icon_type,
                    const GURL& url_for_rappor);
  ~NTPTileImpression();

  // Zero-based index representing the position.
  int index;
  TileSource source;
  TileTitleSource title_source;
  TileVisualType visual_type;
  favicon_base::IconType icon_type;
  // URL the tile points to, formerly used to report Rappor metrics. Currently
  // completely ignored but this code remains to leave the ability to port to
  // UKM in the future.
  GURL url_for_rappor;
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_NTP_TILE_IMPRESSION_H_
