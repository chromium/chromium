// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_LOG_SINK_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_LOG_SINK_H_

#include "components/query_tiles/internal/tile_group.h"
#include "components/query_tiles/internal/tile_types.h"

namespace query_tiles {

// A destination for all interesting events from internal components.
class LogSink {
 public:
  virtual ~LogSink() = default;

  // To be called when status of fetcher or manager changes.
  virtual void OnServiceStatusChanged() = 0;

  // To be called when the tile gorup raw data is available.
  virtual void OnTileDataAvailable() = 0;
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_LOG_SINK_H_
