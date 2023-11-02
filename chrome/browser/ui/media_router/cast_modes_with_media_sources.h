// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_CAST_MODES_WITH_MEDIA_SOURCES_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_CAST_MODES_WITH_MEDIA_SOURCES_H_

#include <map>
#include <unordered_set>

#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"

namespace media_router {

// Contains information on cast modes and the sources associated with them.
// Each cast mode contained has at least one source.
// TODO(imcheng): Move this into QueryResultManager as this class is only used
// there.
class CastModesWithMediaSources {
 public:
  explicit CastModesWithMediaSources(const MediaSink& sink);
  CastModesWithMediaSources(CastModesWithMediaSources&& other);

  CastModesWithMediaSources(const CastModesWithMediaSources&) = delete;
  CastModesWithMediaSources& operator=(const CastModesWithMediaSources&) =
      delete;

  ~CastModesWithMediaSources();

  const MediaSink& sink() const { return sink_; }
  void set_sink(const MediaSink& sink) { sink_ = sink; }

  // Adds a source for the cast mode.
  void AddSource(MediaCastMode cast_mode, const MediaSource& source);

  // Removes a source from the cast mode. The cast mode will also get removed if
  // it has no other sources. This is a no-op if the source is not found.
  void RemoveSource(MediaCastMode cast_mode, const MediaSource& source);

  // Returns true if the source for the cast mode is contained.
  bool HasSource(MediaCastMode cast_mode, const MediaSource& source) const;

  // Returns a set of all the cast modes contained.
  CastModeSet GetCastModes() const;

  // Returns true if there are no cast modes contained.
  bool IsEmpty() const;

 private:
  MediaSink sink_;
  std::map<MediaCastMode, std::set<MediaSource, MediaSource::Cmp>> cast_modes_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_CAST_MODES_WITH_MEDIA_SOURCES_H_
