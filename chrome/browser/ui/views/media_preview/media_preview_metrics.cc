// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"

#include <string>

#include "base/check_op.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"

namespace media_preview_metrics {

namespace {

constexpr char kUiPrefix[] = "MediaPreviews.UI.";
constexpr char kDeviceSelectionPrefix[] = "MediaPreviews.UI.DeviceSelection.";
constexpr char kVideoPrefix[] = "MediaPreviews.UI.Preview.";

base::HistogramBase* GetMediaPreviewDurationHistogram(std::string name) {
  // Duration buckets as powers of 2
  const std::vector<int> custom_ranges{1, 2, 4, 8, 16, 32, 64, 128, 256, 512};
  return base::CustomHistogram::FactoryGet(
      name, custom_ranges, base::HistogramBase::kUmaTargetedHistogramFlag);
}

std::string GetUiLocationString(const Context& context) {
  switch (context.ui_location) {
    case UiLocation::kPermissionPrompt:
      return "Permissions";
    case UiLocation::kPageInfo:
      return "PageInfo";
  }
}

std::string GetPreviewTypeString(const Context& context) {
  switch (context.preview_type) {
    case PreviewType::kCamera:
      return "Camera";
    case PreviewType::kMic:
      return "Mic";
    case PreviewType::kCameraAndMic:
      return "CameraAndMic";
  }
}

// Doesn't accept a Context with PreviewType::kCameraAndMic.
std::string GetUiLocationAndPreviewTypeString(const Context& context) {
  CHECK_NE(context.preview_type, PreviewType::kCameraAndMic);
  return GetUiLocationString(context) + "." + GetPreviewTypeString(context);
}

std::string GetUiLocationAndPreviewTypeStringAllowingBoth(
    const Context& context) {
  if (context.preview_type == PreviewType::kCameraAndMic) {
    CHECK_EQ(context.ui_location, UiLocation::kPermissionPrompt);
  }
  return GetUiLocationString(context) + "." + GetPreviewTypeString(context);
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

Context::Context(UiLocation ui_location, PreviewType preview_type)
    : ui_location(ui_location), preview_type(preview_type) {}
Context::~Context() = default;

void RecordPageInfoNumInUseDevices(const Context& context, int devices) {
  CHECK_EQ(context.ui_location, UiLocation::kPageInfo);
  std::string location_plus_type = GetUiLocationAndPreviewTypeString(context);
  std::string metric_name = kUiPrefix + location_plus_type + ".NumInUseDevices";
  base::UmaHistogramExactLinear(metric_name, devices, /*exclusive_max=*/5);
}

void RecordMediaPreviewDuration(const Context& context,
                                const base::TimeDelta& delta) {
  std::string location_plus_type =
      GetUiLocationAndPreviewTypeStringAllowingBoth(context);
  std::string metric_name = kUiPrefix + location_plus_type + ".Duration";
  GetMediaPreviewDurationHistogram(metric_name)->Add(delta.InSeconds());
}

void RecordDeviceSelectionTotalDevices(const Context& context, int devices) {
  std::string location_plus_type = GetUiLocationAndPreviewTypeString(context);
  std::string metric_name =
      kDeviceSelectionPrefix + location_plus_type + ".NumDevices";
  base::UmaHistogramExactLinear(metric_name, devices, /*exclusive_max=*/5);
}

void RecordDeviceSelectionAction(
    const Context& context,
    MediaPreviewDeviceSelectionUserAction user_action) {
  std::string location_plus_type = GetUiLocationAndPreviewTypeString(context);
  std::string metric_name =
      kDeviceSelectionPrefix + location_plus_type + ".Action";
  base::UmaHistogramEnumeration(metric_name, user_action);
}

void RecordPreviewCameraPixelHeight(const Context& context, int pixel_height) {
  CHECK_EQ(context.preview_type, PreviewType::kCamera);
  std::string metric_name =
      kUiPrefix + GetUiLocationString(context) + ".Camera.PixelHeight";
  // This really has 8 buckets for 1-1080, but we have to add 2 for underflow
  // and overflow.
  UmaHistogramLinearCounts(metric_name, pixel_height, /*minimum=*/1,
                           /*maximum=*/1080, /*bucket_count=*/10);
}

void RecordPreviewVideoExpectedFPS(const Context& context, int expected_fps) {
  CHECK_EQ(context.preview_type, PreviewType::kCamera);
  std::string metric_name =
      kVideoPrefix + GetUiLocationString(context) + ".Video.ExpectedFPS";
  base::UmaHistogramExactLinear(metric_name, expected_fps,
                                /*exclusive_max=*/61);
}

void RecordPreviewVideoActualFPS(const Context& context, int actual_fps) {
  CHECK_EQ(context.preview_type, PreviewType::kCamera);
  std::string metric_name =
      kVideoPrefix + GetUiLocationString(context) + ".Video.ActualFPS";
  base::UmaHistogramExactLinear(metric_name, actual_fps,
                                /*exclusive_max=*/61);
}

void RecordPreviewVideoFramesRenderedPercent(const Context& context,
                                             float percent) {
  CHECK_EQ(context.preview_type, PreviewType::kCamera);
  std::string metric_name =
      kVideoPrefix + GetUiLocationString(context) + ".Video.RenderedPercent";
  // Convert percentage to 0-100 integer.
  int integer_percent = std::clamp(percent, /*lo=*/0.0f, /*hi=*/1.0f) * 100;
  base::UmaHistogramPercentage(metric_name, integer_percent);
}

}  // namespace media_preview_metrics
