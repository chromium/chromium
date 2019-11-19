// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/cast_modes_with_media_sources.h"
#include "base/stl_util.h"

namespace media_router {

CastModesWithMediaSources::CastModesWithMediaSources(const MediaSink& sink)
    : sink_(sink) {}
CastModesWithMediaSources::CastModesWithMediaSources(
    CastModesWithMediaSources&& other) = default;
CastModesWithMediaSources::~CastModesWithMediaSources() = default;

void CastModesWithMediaSources::AddSource(MediaCastMode cast_mode,
                                          const MediaSource& source) {
  cast_modes_[cast_mode].insert(source);
}

void CastModesWithMediaSources::RemoveSource(MediaCastMode cast_mode,
                                             const MediaSource& source) {
  const auto& cast_mode_it = cast_modes_.find(cast_mode);
  if (cast_mode_it != cast_modes_.end()) {
    auto& sources_for_cast_mode = cast_mode_it->second;
    sources_for_cast_mode.erase(source);
    if (sources_for_cast_mode.empty())
      cast_modes_.erase(cast_mode);
  }
}

bool CastModesWithMediaSources::HasSource(MediaCastMode cast_mode,
                                          const MediaSource& source) const {
  return base::Contains(cast_modes_, cast_mode)
             ? base::Contains(cast_modes_.at(cast_mode), source)
             : false;
}

CastModeSet CastModesWithMediaSources::GetCastModes() const {
  CastModeSet cast_mode_set;
  for (const auto& cast_mode_pair : cast_modes_)
    cast_mode_set.insert(cast_mode_pair.first);
  return cast_mode_set;
}

bool CastModesWithMediaSources::IsEmpty() const {
  return cast_modes_.empty();
}

}  // namespace media_router
