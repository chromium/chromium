// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"

#include <optional>
#include <string>
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"

namespace {

std::optional<std::string> MapContextToString(
    media_preview_metrics::Context context) {
  std::string ui_location;
  std::string preview_type;

  switch (context.ui_location) {
    case media_preview_metrics::UiLocation::kPermissionPrompt:
      ui_location = "Permissions";
      break;
    case media_preview_metrics::UiLocation::kPageInfo:
      ui_location = "PageInfo";
      break;
    default:
#if DCHECK_IS_ON()
      DLOG(FATAL) << "Context ui_location is unknown";
#else
      LOG(ERROR) << "Context ui_location is unknown";
#endif
      return std::nullopt;
  }

  switch (context.preview_type) {
    case media_preview_metrics::PreviewType::kCamera:
      preview_type = "Camera";
      break;
    case media_preview_metrics::PreviewType::kMic:
      preview_type = "Mic";
      break;
    default:
#if DCHECK_IS_ON()
      DLOG(FATAL) << "Context preview_type is unknown";
#else
      LOG(ERROR) << "Context preview_type is unknown";
#endif
      return std::nullopt;
  }
  return ui_location + "." + preview_type;
}

}  // anonymous namespace

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

void RecordDeviceSelectionTotalDevices(Context context, int devices) {
  std::optional<std::string> context_metric_id = MapContextToString(context);
  if (!context_metric_id) {
    return;
  }
  std::string metric_name =
      "MediaPreviews.UI.DeviceSelection." + *context_metric_id + ".NumDevices";
  base::UmaHistogramCustomCounts(metric_name, devices, 0, 5, 5);
}

void RecordPreviewCameraPixelHeight(Context context, int pixelHeight) {
  std::string context_metric_id;
  switch (context.ui_location) {
    case media_preview_metrics::UiLocation::kPermissionPrompt:
      context_metric_id = "MediaPreviews.UI.Permissions.Camera.PixelHeight";
      break;
    case media_preview_metrics::UiLocation::kPageInfo:
      context_metric_id = "MediaPreviews.UI.PageInfo.Camera.PixelHeight";
      break;
    default:
#if DCHECK_IS_ON()
      NOTREACHED_NORETURN() << "Context ui_location is unknown";
#else
      LOG(ERROR) << "Context ui_location is unknown";
      return;
#endif
  }
  base::UmaHistogramCustomCounts(context_metric_id, pixelHeight, 0, 1080, 8);
}

void RecordDeviceSelectionAction(
    Context context,
    MediaPreviewDeviceSelectionUserAction user_action) {
  std::optional<std::string> context_metric_id = MapContextToString(context);
  if (!context_metric_id) {
    return;
  }
  std::string metric_name =
      "MediaPreviews.UI.DeviceSelection." + *context_metric_id + ".Action";
  base::UmaHistogramEnumeration(metric_name, user_action);
}

}  // namespace media_preview_metrics
