// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_HEATMAP_HEATMAP_PALM_DETECTOR_H_
#define CHROMEOS_ASH_COMPONENTS_HEATMAP_HEATMAP_PALM_DETECTOR_H_

#include "chromeos/ash/components/heatmap/heatmap_ml_agent.h"
#include "ui/ozone/public/palm_detector.h"

namespace ash {

// A class that detects whether there is a palm in the given heatmap data calls
// a provided callback method with the detection result.
class HeatmapPalmDetector : public ui::PalmDetector {
 public:
  HeatmapPalmDetector();
  ~HeatmapPalmDetector() override;

  // ui::PalmDetector:
  void DetectPalm(const std::vector<double>& data,
                  DetectionDoneCallback callback) override;

 private:
  void OnExecuteDone(DetectionDoneCallback callback,
                     absl::optional<double> result);

  std::unique_ptr<HeatmapMlAgent> ml_agent_;

  base::WeakPtrFactory<HeatmapPalmDetector> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_HEATMAP_HEATMAP_PALM_DETECTOR_H_
