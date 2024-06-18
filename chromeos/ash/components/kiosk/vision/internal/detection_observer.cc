// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/internal/detection_observer.h"

#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "chromeos/ash/components/kiosk/vision/internal/detection_processor.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-forward.h"

namespace ash::kiosk_vision {

BASE_FEATURE(kEmitKioskVisionFakes,
             "EmitKioskVisionFakes",
             base::FEATURE_DISABLED_BY_DEFAULT);

DetectionObserver::DetectionObserver(DetectionProcessors processors)
    : processors_(std::move(processors)) {
  CHECK_GT(processors_.size(), 0ul) << "No processors given";
  if (base::FeatureList::IsEnabled(kEmitKioskVisionFakes)) {
    constexpr base::TimeDelta kFakeEmissionPeriod = base::Seconds(5);
    fake_detection_timer_.Start(
        FROM_HERE, kFakeEmissionPeriod,
        base::BindRepeating(&DetectionObserver::EmitFakeDetection,
                            base::Unretained(this)));
  }
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

void DetectionObserver::EmitFakeDetection() {
  constexpr int kLargeCount = 3;
  constexpr int kSmallCount = 2;
  constexpr int kMaxOffset = 20;

  // Makes 2 or 3 fake appearances depending on `fake_detection_flag_`.
  auto fake_detection = cros::mojom::KioskVisionDetection::New();
  for (int i = 0; i < (fake_detection_flag_ ? kLargeCount : kSmallCount); i++) {
    auto fake_appearance = cros::mojom::KioskVisionAppearance::New();
    fake_appearance->person_id = i + fake_detection_offset_;
    fake_detection->appearances.push_back(std::move(fake_appearance));
  }
  fake_detection_flag_ = !fake_detection_flag_;
  fake_detection_offset_ = (fake_detection_offset_ + 1) % kMaxOffset;

  OnDetection(std::move(fake_detection));
}

}  // namespace ash::kiosk_vision
