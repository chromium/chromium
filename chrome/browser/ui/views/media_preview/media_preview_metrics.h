// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_PREVIEW_METRICS_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_PREVIEW_METRICS_H_

#include "base/time/time.h"

namespace media_preview_metrics {

enum class UiLocation { kPermissionPrompt, kPageInfo };
enum class PreviewType { kUnknown, kCamera, kMic, kCameraAndMic };

struct Context {
  explicit Context(UiLocation ui_location);
  Context(UiLocation ui_location, PreviewType preview_type);
  ~Context();

  const UiLocation ui_location;
  PreviewType preview_type = PreviewType::kUnknown;
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MediaPreviewDeviceSelectionUserAction {
  kNoAction = 0,
  kOpened = 1,
  kSelection = 2,
  // Add new types only immediately above this line. Remember to also update
  // tools/metrics/histograms/metadata/media/enums.xml.
  kMaxValue = kSelection,
};

void RecordPageInfoCameraNumInUseDevices(int devices);
void RecordPageInfoMicNumInUseDevices(int devices);
void RecordDeviceSelectionTotalDevices(Context context, int devices);
void RecordDeviceSelectionAction(
    Context context,
    MediaPreviewDeviceSelectionUserAction user_action);
void RecordPreviewCameraPixelHeight(Context context, int pixel_height);
void RecordPreviewVideoExpectedFPS(Context context, int expected_fps);
void RecordPreviewVideoActualFPS(Context context, int actual_fps);
void RecordMediaPreviewDuration(Context context, const base::TimeDelta& delta);
void RecordPreviewVideoFramesRenderedPercent(Context context, float percent);

}  // namespace media_preview_metrics

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_PREVIEW_METRICS_H_
