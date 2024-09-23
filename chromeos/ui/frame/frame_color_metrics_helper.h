// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_FRAME_COLOR_METRICS_HELPER_H_
#define CHROMEOS_UI_FRAME_FRAME_COLOR_METRICS_HELPER_H_

#include <string>

#include "base/timer/timer.h"
#include "chromeos/ui/base/app_types.h"

namespace chromeos {

class FrameColorMetricsHelper {
 public:
  explicit FrameColorMetricsHelper(chromeos::AppType app_type);
  FrameColorMetricsHelper(const FrameColorMetricsHelper& other) = delete;
  FrameColorMetricsHelper& operator=(const FrameColorMetricsHelper&) = delete;
  ~FrameColorMetricsHelper();

  // When the frame color is changed, the count is incremented.
  void UpdateFrameColorChangesCount();

  static std::string GetFrameColorChangeHistogramName(
      chromeos::AppType app_type);

 private:
  // Start the timer for counting the frame color changes.
  void StartTracing();

  // When the timer expires, stop counting the frame color changes, and record
  // the result in UMA.
  void FinalizeFrameColorTracing();

  void RecordFrameColorChangeCount();

  // Timer for frame color changes counting.
  base::OneShotTimer frame_start_timer_;

  const chromeos::AppType app_type_;

  uint32_t frame_color_change_count_ = 0;

  base::WeakPtrFactory<FrameColorMetricsHelper> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_FRAME_COLOR_METRICS_HELPER_H_
