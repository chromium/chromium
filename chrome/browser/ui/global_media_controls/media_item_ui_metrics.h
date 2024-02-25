// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_METRICS_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_METRICS_H_

#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "components/media_router/common/media_sink.h"

namespace {

const char kStartCastingModeHistogramName[] =
    "Media.GlobalMediaControls.MediaCastMode.Start";
const char kStopCastingModeHistogramName[] =
    "Media.GlobalMediaControls.MediaCastMode.Stop";
const char kCastStartStopHistogramName[] = "Media.Notification.Cast.StartStop";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum GlobalMediaControlsCastMode {
  kPresentation = 0,
  kRemotePlayback = 1,
  kTabMirror = 2,

  // NOTE: Do not reorder existing entries, and add entries only immediately
  // above this line.
  kMaxValue = kTabMirror,
};

}  // namespace

class MediaItemUIMetrics {
 public:
  static void RecordStartCastingMetrics(
      media_router::SinkIconType sink_icon_type,
      media_router::MediaCastMode cast_mode);
  static void RecordStopCastingMetrics(media_router::MediaCastMode cast_mode);
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_ITEM_UI_METRICS_H_
