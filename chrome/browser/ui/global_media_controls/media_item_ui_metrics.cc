// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_item_ui_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/media_router/browser/media_router_metrics.h"

using media_router::MediaRouterMetrics;

namespace {

GlobalMediaControlsCastMode GetGlobalMediaControlsCastMode(
    media_router::MediaCastMode cast_mode) {
  switch (cast_mode) {
    case media_router::MediaCastMode::PRESENTATION:
      return GlobalMediaControlsCastMode::kPresentation;
    case media_router::MediaCastMode::REMOTE_PLAYBACK:
      return GlobalMediaControlsCastMode::kRemotePlayback;
    case media_router::MediaCastMode::TAB_MIRROR:
      return GlobalMediaControlsCastMode::kTabMirror;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return GlobalMediaControlsCastMode::kPresentation;
}

}  // namespace

// static
void MediaItemUIMetrics::RecordStartCastingMetrics(
    media_router::SinkIconType sink_icon_type,
    media_router::MediaCastMode cast_mode) {
  MediaRouterMetrics::RecordMediaSinkTypeForGlobalMediaControls(sink_icon_type);
  base::UmaHistogramEnumeration(kStartCastingModeHistogramName,
                                GetGlobalMediaControlsCastMode(cast_mode));
}

// static
void MediaItemUIMetrics::RecordStopCastingMetrics(
    media_router::MediaCastMode cast_mode) {
  base::UmaHistogramEnumeration(kStopCastingModeHistogramName,
                                GetGlobalMediaControlsCastMode(cast_mode));
}
