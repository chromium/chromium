// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/media_sink_with_cast_modes.h"

namespace media_router {

MediaSinkWithCastModes::MediaSinkWithCastModes(const MediaSink& sink)
    : sink(sink) {}

MediaSinkWithCastModes::MediaSinkWithCastModes(
    const MediaSink& sink,
    std::initializer_list<MediaCastMode> cast_modes)
    : sink(sink), cast_modes(cast_modes) {}

MediaSinkWithCastModes::MediaSinkWithCastModes(
    const MediaSinkWithCastModes& other) = default;

MediaSinkWithCastModes::~MediaSinkWithCastModes() {}

bool MediaSinkWithCastModes::operator==(
    const MediaSinkWithCastModes& other) const {
  return sink.id() == other.sink.id() && cast_modes == other.cast_modes;
}

}  // namespace media_router
