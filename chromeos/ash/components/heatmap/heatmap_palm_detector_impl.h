// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_HEATMAP_HEATMAP_PALM_DETECTOR_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_HEATMAP_HEATMAP_PALM_DETECTOR_IMPL_H_

#include <optional>
#include <queue>
#include <unordered_set>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/services/machine_learning/public/mojom/heatmap_palm_rejection.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/events/ozone/evdev/heatmap_palm_detector.h"

namespace ash {

// The client of the heatmap palm detection service running in ML service. It
// also provide palm detection result to ozone.
class HeatmapPalmDetectorImpl
    : public ui::HeatmapPalmDetector,
      chromeos::machine_learning::mojom::HeatmapPalmRejectionClient {
 public:
  HeatmapPalmDetectorImpl();
  ~HeatmapPalmDetectorImpl() override;

  // ui::HeatmapPalmDetector:
  void Start(ModelId model_id,
             std::string_view hidraw_path,
             std::optional<CropHeatmap> crop_heatmap) override;
  bool IsPalm(int tracking_id) const override;
  bool IsReady() const override;
  void AddTouchRecord(base::Time timestamp,
                      const std::vector<int>& tracking_ids) override;
  void RemoveTouch(int tracking_id) override;

  // chromeos::machine_learning::mojom::HeatmapPalmRejectionClient
  void OnHeatmapProcessedEvent(
      chromeos::machine_learning::mojom::HeatmapProcessedEventPtr event)
      override;

 private:
  void OnLoadHeatmapPalmRejection(
      chromeos::machine_learning::mojom::LoadHeatmapPalmRejectionResult result);

  void OnConnectionError();

  bool is_ready_ = false;

  std::optional<CropHeatmap> crop_heatmap_;
  ModelId model_id_;
  std::string hidraw_path_;

  std::queue<TouchRecord> touch_records_;
  std::unordered_set<int> palm_tracking_ids_;
  base::TimeDelta reconnect_delay_;
  base::OneShotTimer delay_timer_;

  mojo::Remote<chromeos::machine_learning::mojom::MachineLearningService>
      ml_service_;
  mojo::Receiver<chromeos::machine_learning::mojom::HeatmapPalmRejectionClient>
      client_;

  base::WeakPtrFactory<HeatmapPalmDetectorImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_HEATMAP_HEATMAP_PALM_DETECTOR_IMPL_H_
