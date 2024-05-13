// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/frame_color_metrics_helper.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chromeos/ui/base/app_types.h"

namespace {

constexpr char kArcHistogramName[] = "ArcApp";
constexpr char kBrowserHistogramName[] = "Browser";
constexpr char kChromeAppHistogramName[] = "ChromeApp";
constexpr char kSystemAppHistogramName[] = "SystemApp";
constexpr char kCrostiniAppHistogramName[] = "CrostiniApp";

// The tracing duration for frame color changes.
constexpr base::TimeDelta kFrameColorTracingTime = base::Seconds(3);

}  // namespace

namespace chromeos {

FrameColorMetricsHelper::FrameColorMetricsHelper(chromeos::AppType app_type)
    : app_type_(app_type) {
  StartTracing();
}

FrameColorMetricsHelper::~FrameColorMetricsHelper() = default;

void FrameColorMetricsHelper::UpdateFrameColorChangesCount() {
  frame_color_change_count_++;
}

// static
std::string FrameColorMetricsHelper::GetFrameColorChangeHistogramName(
    chromeos::AppType app_type) {
  std::string app_type_str_;
  switch (app_type) {
    case chromeos::AppType::ARC_APP:
      app_type_str_ = kArcHistogramName;
      break;
    case chromeos::AppType::BROWSER:
      app_type_str_ = kBrowserHistogramName;
      break;
    case chromeos::AppType::CHROME_APP:
      app_type_str_ = kChromeAppHistogramName;
      break;
    case chromeos::AppType::SYSTEM_APP:
      app_type_str_ = kSystemAppHistogramName;
      break;
    case chromeos::AppType::CROSTINI_APP:
      app_type_str_ = kCrostiniAppHistogramName;
      break;
    default:
      app_type_str_ = "Others";
  }
  return base::StrCat({"Ash.Frame.ColorChangeCount.", app_type_str_});
}

void FrameColorMetricsHelper::StartTracing() {
  CHECK(!frame_start_timer_.IsRunning());
  frame_start_timer_.Start(
      FROM_HERE, kFrameColorTracingTime,
      base::BindOnce(&FrameColorMetricsHelper::FinalizeFrameColorTracing,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FrameColorMetricsHelper::FinalizeFrameColorTracing() {
  frame_start_timer_.Stop();
  RecordFrameColorChangeCount();
}

void FrameColorMetricsHelper::RecordFrameColorChangeCount() {
  const auto histogram_name = GetFrameColorChangeHistogramName(app_type_);
  base::UmaHistogramCounts100(histogram_name, frame_color_change_count_);
}

}  // namespace chromeos
