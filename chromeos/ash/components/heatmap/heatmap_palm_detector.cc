// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/ash/components/heatmap/heatmap_palm_detector.h"

namespace ash {

HeatmapPalmDetector::HeatmapPalmDetector()
    : ml_agent_(std::make_unique<HeatmapMlAgent>()) {}

HeatmapPalmDetector::~HeatmapPalmDetector() = default;

void HeatmapPalmDetector::DetectPalm(const std::vector<double>& data,
                                     DetectionDoneCallback callback) {
  ml_agent_->Execute(
      data, base::BindOnce(&HeatmapPalmDetector::OnExecuteDone,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void HeatmapPalmDetector::OnExecuteDone(DetectionDoneCallback callback,
                                        absl::optional<double> result) {
  std::move(callback).Run(result.value_or(0) > 0 ? DetectionResult::kPalm
                                                 : DetectionResult::kNoPalm);
}

}  // namespace ash
