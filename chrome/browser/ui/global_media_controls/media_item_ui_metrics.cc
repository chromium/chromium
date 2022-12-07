// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_item_ui_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/media_router/browser/media_router_metrics.h"

using global_media_controls::GlobalMediaControlsCastActionAndEntryPoint;
using global_media_controls::GlobalMediaControlsEntryPoint;
using media_router::MediaRouterMetrics;
using media_router::mojom::MediaRouteProviderId;

namespace {

GlobalMediaControlsCastMode GetGlobalMediaControlsCastMode(
    media_router::MediaCastMode cast_mode) {
  switch (cast_mode) {
    case media_router::MediaCastMode::PRESENTATION:
      return GlobalMediaControlsCastMode::kPresentation;
    case media_router::MediaCastMode::REMOTE_PLAYBACK:
      return GlobalMediaControlsCastMode::kRemotePlayback;
    default:
      NOTREACHED();
  }
  return GlobalMediaControlsCastMode::kPresentation;
}

}  // namespace

void MediaItemUIMetrics::RecordStartCastingMetrics(
    media_router::SinkIconType sink_icon_type,
    media_router::MediaCastMode cast_mode,
    GlobalMediaControlsEntryPoint entry_point) {
  MediaRouterMetrics::RecordMediaSinkTypeForGlobalMediaControls(sink_icon_type);
  base::UmaHistogramEnumeration(kStartCastingModeHistogramName,
                                GetGlobalMediaControlsCastMode(cast_mode));

  GlobalMediaControlsCastActionAndEntryPoint action;
  switch (entry_point) {
    case GlobalMediaControlsEntryPoint::kToolbarIcon:
      action = GlobalMediaControlsCastActionAndEntryPoint::kStartViaToolbarIcon;
      break;
    case GlobalMediaControlsEntryPoint::kPresentation:
      action =
          GlobalMediaControlsCastActionAndEntryPoint::kStartViaPresentation;
      break;
    case GlobalMediaControlsEntryPoint::kSystemTray:
      action = GlobalMediaControlsCastActionAndEntryPoint::kStartViaSystemTray;
      break;
  }
  base::UmaHistogramEnumeration(kCastStartStopHistogramName, action);
}

void MediaItemUIMetrics::RecordStopCastingMetrics(
    media_router::MediaCastMode cast_mode,
    GlobalMediaControlsEntryPoint entry_point) {
  base::UmaHistogramEnumeration(kStopCastingModeHistogramName,
                                GetGlobalMediaControlsCastMode(cast_mode));

  GlobalMediaControlsCastActionAndEntryPoint action;
  switch (entry_point) {
    case GlobalMediaControlsEntryPoint::kToolbarIcon:
      action = GlobalMediaControlsCastActionAndEntryPoint::kStopViaToolbarIcon;
      break;
    case GlobalMediaControlsEntryPoint::kPresentation:
      action = GlobalMediaControlsCastActionAndEntryPoint::kStopViaPresentation;
      break;
    case GlobalMediaControlsEntryPoint::kSystemTray:
      action = GlobalMediaControlsCastActionAndEntryPoint::kStopViaSystemTray;
      break;
  }
  base::UmaHistogramEnumeration(kCastStartStopHistogramName, action);
}
