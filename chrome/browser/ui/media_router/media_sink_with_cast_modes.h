// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_SINK_WITH_CAST_MODES_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_SINK_WITH_CAST_MODES_H_

#include <set>

#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "components/media_router/common/media_sink.h"

namespace media_router {

// Contains information on a MediaSink and the set of cast modes it is
// compatible with. This should be interpreted under the context of a
// QueryResultManager which contains a mapping from MediaCastMode to
// MediaSource.
struct MediaSinkWithCastModes {
  explicit MediaSinkWithCastModes(const MediaSink& sink);
  MediaSinkWithCastModes(const MediaSink& sink,
                         std::initializer_list<MediaCastMode> cast_modes);
  MediaSinkWithCastModes(const MediaSinkWithCastModes& other);
  ~MediaSinkWithCastModes();

  bool operator==(const MediaSinkWithCastModes& other) const;

  MediaSink sink;
  CastModeSet cast_modes;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_SINK_WITH_CAST_MODES_H_
