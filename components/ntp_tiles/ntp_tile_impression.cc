// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/ntp_tile_impression.h"

namespace ntp_tiles {

NTPTileImpression::NTPTileImpression()
    : NTPTileImpression(/*index=*/0,
                        /*source=*/TileSource::TOP_SITES,
                        /*title_source=*/TileTitleSource::UNKNOWN,
                        /*visual_type=*/TileVisualType::UNKNOWN_TILE_TYPE,
                        /*icon_type=*/favicon_base::IconType::kInvalid,
                        /*url_for_rappor=*/GURL()) {}

NTPTileImpression::NTPTileImpression(int index,
                                     TileSource source,
                                     TileTitleSource title_source,
                                     TileVisualType visual_type,
                                     favicon_base::IconType icon_type,
                                     const GURL& url_for_rappor)
    : index(index),
      source(source),
      title_source(title_source),
      visual_type(visual_type),
      icon_type(icon_type),
      url_for_rappor(url_for_rappor) {}

NTPTileImpression::~NTPTileImpression() = default;

}  // namespace ntp_tiles
