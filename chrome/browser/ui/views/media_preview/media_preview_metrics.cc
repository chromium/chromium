// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"

#include <optional>
#include <string>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"

namespace {

base::HistogramBase* GetMediaPreviewDurationHistogram(std::string name) {
  // Duration buckets as powers of 2
  const std::vector<int> custom_ranges{1, 2, 4, 8, 16, 32, 64, 128, 256, 512};
  return base::CustomHistogram::FactoryGet(
      name, custom_ranges, base::HistogramBase::kUmaTargetedHistogramFlag);
}

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

void UmaHistogramLinearCounts(const std::string& name,
                              int sample,
                              int minimum,
                              int maximum,
                              size_t bucket_count) {
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      name, minimum, maximum, bucket_count,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

}  // anonymous namespace

namespace media_preview_metrics {

Context::Context(UiLocation ui_location) : ui_location(ui_location) {}
Context::Context(UiLocation ui_location, PreviewType preview_type)
    : ui_location(ui_location), preview_type(preview_type) {}
Context::~Context() = default;

void RecordPageInfoCameraNumInUseDevices(int devices) {
  base::UmaHistogramExactLinear(
      "MediaPreviews.UI.PageInfo.Camera.NumInUseDevices", devices, 5);
}

void RecordPageInfoMicNumInUseDevices(int devices) {
  base::UmaHistogramExactLinear("MediaPreviews.UI.PageInfo.Mic.NumInUseDevices",
                                devices, 5);
}

void RecordDeviceSelectionTotalDevices(Context context, int devices) {
  std::optional<std::string> context_metric_id = MapContextToString(context);
  if (!context_metric_id) {
    return;
  }
  std::string metric_name =
      "MediaPreviews.UI.DeviceSelection." + *context_metric_id + ".NumDevices";
  base::UmaHistogramExactLinear(metric_name, devices, 5);
}

void RecordPreviewCameraPixelHeight(Context context, int pixel_height) {
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
  // This really has 8 buckets for 1-1080, but we have to add 2 for underflow
  // and overflow.
  UmaHistogramLinearCounts(context_metric_id, pixel_height, 1, 1080, 10);
}

void RecordPreviewVideoExpectedFPS(Context context, int expected_fps) {
  std::string context_metric_id;
  switch (context.ui_location) {
    case media_preview_metrics::UiLocation::kPermissionPrompt:
      context_metric_id =
          "MediaPreviews.UI.Preview.Permissions.Video.ExpectedFPS";
      break;
    case media_preview_metrics::UiLocation::kPageInfo:
      context_metric_id = "MediaPreviews.UI.Preview.PageInfo.Video.ExpectedFPS";
      break;
    default:
#if DCHECK_IS_ON()
      NOTREACHED_NORETURN() << "Context ui_location is unknown";
#else
      LOG(ERROR) << "Context ui_location is unknown";
      return;
#endif
  }
  base::UmaHistogramExactLinear(context_metric_id, expected_fps,
                                /*exclusive_max=*/61);
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

void RecordPreviewVideoActualFPS(Context context, int actual_fps) {
  std::string context_metric_id;
  switch (context.ui_location) {
    case media_preview_metrics::UiLocation::kPermissionPrompt:
      context_metric_id =
          "MediaPreviews.UI.Preview.Permissions.Video.ActualFPS";
      break;
    case media_preview_metrics::UiLocation::kPageInfo:
      context_metric_id = "MediaPreviews.UI.Preview.PageInfo.Video.ActualFPS";
      break;
    default:
#if DCHECK_IS_ON()
      NOTREACHED_NORETURN() << "Context ui_location is unknown";
#else
      LOG(ERROR) << "Context ui_location is unknown";
      return;
#endif
  }
  base::UmaHistogramExactLinear(context_metric_id, actual_fps,
                                /*exclusive_max=*/61);
}

void RecordMediaPreviewDuration(Context context, const base::TimeDelta& delta) {
  std::string metric_name;
  if (context.preview_type == PreviewType::kCameraAndMic) {
    if (context.ui_location == UiLocation::kPageInfo) {
      return;
    }
    metric_name = "MediaPreviews.UI.Permissions.CameraAndMic.Duration";
  } else {
    std::optional<std::string> context_metric_id = MapContextToString(context);
    if (!context_metric_id) {
      return;
    }
    metric_name = "MediaPreviews.UI." + context_metric_id.value() + ".Duration";
  }

  GetMediaPreviewDurationHistogram(metric_name)->Add(delta.InSeconds());
}

void RecordPreviewVideoFramesRenderedPercent(Context context, float percent) {
  std::string context_metric_id;
  switch (context.ui_location) {
    case media_preview_metrics::UiLocation::kPermissionPrompt:
      context_metric_id =
          "MediaPreviews.UI.Preview.Permissions.Video.RenderedPercent";
      break;
    case media_preview_metrics::UiLocation::kPageInfo:
      context_metric_id =
          "MediaPreviews.UI.Preview.PageInfo.Video.RenderedPercent";
      break;
    default:
#if DCHECK_IS_ON()
      NOTREACHED_NORETURN() << "Context ui_location is unknown";
#else
      LOG(ERROR) << "Context ui_location is unknown";
      return;
#endif
  }

  // Convert percentage to 0-100 integer.
  int integer_percent = std::clamp(percent, /*__lo=*/0.0f, /*__hi=*/1.0f) * 100;
  base::UmaHistogramPercentage(context_metric_id, integer_percent);
}

}  // namespace media_preview_metrics
