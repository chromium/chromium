// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_HEATMAP_HEATMAP_PALM_DETECTOR_H_
#define CHROMEOS_ASH_COMPONENTS_HEATMAP_HEATMAP_PALM_DETECTOR_H_

#include "chromeos/services/machine_learning/public/mojom/heatmap_palm_rejection.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/ozone/public/palm_detector.h"

namespace ash {

// The client of the heatmap palm detection service running in ML service. It
// also provide palm detection result to ozone.
class HeatmapPalmDetector
    : public ui::PalmDetector,
      chromeos::machine_learning::mojom::HeatmapPalmRejectionClient {
 public:
  HeatmapPalmDetector();
  ~HeatmapPalmDetector() override;

  // ui::PalmDetector:
  void Start(DeviceId device, std::string_view path) override;
  DetectionResult GetDetectionResult() const override;

  // chromeos::machine_learning::mojom::HeatmapPalmRejectionClient
  void OnHeatmapProcessedEvent(
      chromeos::machine_learning::mojom::HeatmapProcessedEventPtr event)
      override;

 private:
  void OnLoadHeatmapPalmRejection(
      chromeos::machine_learning::mojom::LoadHeatmapPalmRejectionResult result);

  void OnConnectionError();

  bool is_palm_ = false;

  mojo::Remote<chromeos::machine_learning::mojom::MachineLearningService>
      ml_service_;
  mojo::Receiver<chromeos::machine_learning::mojom::HeatmapPalmRejectionClient>
      client_;

  base::WeakPtrFactory<HeatmapPalmDetector> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_HEATMAP_HEATMAP_PALM_DETECTOR_H_
