// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNAL_DETECTION_OBSERVER_H_
#define CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNAL_DETECTION_OBSERVER_H_

#include "base/feature_list.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/kiosk/vision/internal/detection_processor.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-forward.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"

namespace ash::kiosk_vision {

// When enabled, `DetectionObserver` will emit fake detections to processors.
// Used for development purposes only.
BASE_DECLARE_FEATURE(kEmitKioskVisionFakes);

// Receives detections from `CrosCameraService` and forwards them to the given
// `processors`.
class DetectionObserver : public cros::mojom::KioskVisionObserver {
 public:
  explicit DetectionObserver(DetectionProcessors processors);
  DetectionObserver(const DetectionObserver&) = delete;
  DetectionObserver& operator=(const DetectionObserver&) = delete;
  ~DetectionObserver() override;

  // `cros::mojom::KioskVisionObserver` implementations.
  void OnFrameProcessed(
      cros::mojom::KioskVisionDetectionPtr detection) override;
  void OnTrackCompleted(cros::mojom::KioskVisionTrackPtr track) override;
  void OnError(cros::mojom::KioskVisionError error) override;

 private:
  void EmitFakeDetection();

  DetectionProcessors processors_;

  bool fake_detection_flag_ = false;
  int fake_detection_offset_ = 0;
  base::RepeatingTimer fake_detection_timer_;
};

}  // namespace ash::kiosk_vision

#endif  // CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_INTERNAL_DETECTION_OBSERVER_H_
