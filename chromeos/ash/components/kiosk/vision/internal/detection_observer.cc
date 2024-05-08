// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/internal/detection_observer.h"

#include <utility>

#include "base/check_op.h"
#include "chromeos/ash/components/kiosk/vision/internal/detection_processor.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-forward.h"

namespace ash::kiosk_vision {

DetectionObserver::DetectionObserver(DetectionProcessors processors)
    : processors_(std::move(processors)) {
  CHECK_GT(processors_.size(), 0ul) << "No processors given";
}

DetectionObserver::~DetectionObserver() = default;

void DetectionObserver::OnDetection(
    cros::mojom::KioskVisionDetectionPtr detection) {
  for (auto& processor : processors_) {
    processor->OnDetection(*detection);
  }
}

void DetectionObserver::OnError(cros::mojom::KioskVisionError error) {
  for (auto& processor : processors_) {
    processor->OnError(error);
  }
}

}  // namespace ash::kiosk_vision
