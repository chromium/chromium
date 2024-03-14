// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"

namespace media_preview_metrics {

Context::Context(UiLocation ui_location) : ui_location(ui_location) {}
Context::~Context() = default;

void RecordPageInfoCameraNumInUseDevices(int devices) {
  base::UmaHistogramCustomCounts(
      "MediaPreviews.UI.PageInfo.Camera.NumInUseDevices", devices, 0, 5, 5);
}

void RecordPageInfoMicNumInUseDevices(int devices) {
  base::UmaHistogramCustomCounts(
      "MediaPreviews.UI.PageInfo.Mic.NumInUseDevices", devices, 0, 5, 5);
}

}  // namespace media_preview_metrics
