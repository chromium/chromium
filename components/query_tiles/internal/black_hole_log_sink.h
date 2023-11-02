// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_BLACK_HOLE_LOG_SINK_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_BLACK_HOLE_LOG_SINK_H_

#include "components/query_tiles/internal/log_sink.h"

namespace query_tiles {
namespace test {

// A LogSink that does nothing with the calls to the interface.
class BlackHoleLogSink : public LogSink {
 public:
  BlackHoleLogSink() = default;
  ~BlackHoleLogSink() override = default;

  BlackHoleLogSink(const BlackHoleLogSink& other) = delete;
  BlackHoleLogSink& operator=(const BlackHoleLogSink& other) = delete;

  // LogSink implementation.
  void OnServiceStatusChanged() override;
  void OnTileDataAvailable() override;
};

}  // namespace test
}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_BLACK_HOLE_LOG_SINK_H_
