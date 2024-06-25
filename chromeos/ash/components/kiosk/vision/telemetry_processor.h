// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_TELEMETRY_PROCESSOR_H_
#define CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_TELEMETRY_PROCESSOR_H_

#include <vector>

#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "chromeos/ash/components/kiosk/vision/internal/detection_processor.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-forward.h"

namespace ash::kiosk_vision {

// Processes detections and surfaces them as needed for the Kiosk Vision
// telemetry API.
class COMPONENT_EXPORT(KIOSK_VISION) TelemetryProcessor
    : public DetectionProcessor {
 public:
  TelemetryProcessor();
  TelemetryProcessor(const TelemetryProcessor&) = delete;
  TelemetryProcessor& operator=(const TelemetryProcessor&) = delete;
  ~TelemetryProcessor() override;

  // Generates and returns a current representation of the accumulated
  // detections and errors in the form of TelemetryData which can then be
  // reported.
  // Only new detections and errors since the last call to this function will be
  // returned.
  ::reporting::TelemetryData GenerateTelemetryData();

  // Returns the IDs detected since the last time this function was called.
  std::vector<int> TakeIdsProcessed();

  // Returns the errors emitted since the last time this function was called.
  std::vector<cros::mojom::KioskVisionError> TakeErrors();

 private:
  // `DetectionProcessor` implementation.
  void OnFrameProcessed(
      const cros::mojom::KioskVisionDetection& detection) override;
  void OnTrackCompleted(const cros::mojom::KioskVisionTrack& track) override;
  void OnError(cros::mojom::KioskVisionError error) override;

  base::circular_deque<int> latest_ids_processed_;
  base::circular_deque<cros::mojom::KioskVisionError> latest_errors_;
};

}  // namespace ash::kiosk_vision

#endif  // CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_TELEMETRY_PROCESSOR_H_
