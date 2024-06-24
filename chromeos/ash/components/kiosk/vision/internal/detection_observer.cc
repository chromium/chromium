// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/internal/detection_observer.h"

#include <cstdint>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "chromeos/ash/components/kiosk/vision/internal/detection_processor.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-forward.h"

namespace ash::kiosk_vision {

namespace {

int64_t CurrentTimestampInMicroseconds() {
  return base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
}

void Validate(const cros::mojom::KioskVisionDetection& detection) {
  bool face_or_body_are_present = base::ranges::all_of(
      detection.appearances, [](auto& a) { return a->face || a->body; });
  CHECK(face_or_body_are_present)
      << "Appearances must have either a face or body or both";
}

}  // namespace

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

void DetectionObserver::OnFrameProcessed(
    cros::mojom::KioskVisionDetectionPtr detection) {
  Validate(*detection);
  for (const auto& processor : processors_) {
    processor->OnFrameProcessed(*detection);
  }
}

void DetectionObserver::OnError(cros::mojom::KioskVisionError error) {
  for (const auto& processor : processors_) {
    processor->OnError(error);
  }
}

void DetectionObserver::EmitFakeDetection() {
  constexpr int kLargeCount = 3;
  constexpr int kSmallCount = 2;
  constexpr int kBoxesPerRow = 6;
  constexpr int kMaxOffset = 20;
  constexpr float kConfidence = 0.7;
  constexpr float kRollFaceAngle = 10;
  constexpr float kPanFaceAngle = 20;
  constexpr float kTiltFaceAngle = 30;

  // Makes 2 or 3 fake appearances depending on `fake_detection_flag_`.
  auto fake_detection = cros::mojom::KioskVisionDetection::New();
  for (int i = 0; i < (fake_detection_flag_ ? kLargeCount : kSmallCount); i++) {
    int offset = fake_detection_offset_ + i;

    auto fake_appearance = cros::mojom::KioskVisionAppearance::New(
        /*timestamp_in_us=*/CurrentTimestampInMicroseconds(),
        /*person_id=*/i,
        /*face=*/
        cros::mojom::KioskVisionFaceDetection::New(
            /*confidence=*/kConfidence,
            /*roll=*/kRollFaceAngle,
            /*pan=*/kPanFaceAngle,
            /*tilt=*/kTiltFaceAngle,
            /*box=*/
            cros::mojom::KioskVisionBoundingBox::New(
                /*x=*/20 + 50 * (offset % kBoxesPerRow),
                /*y=*/20 + 50 * (offset / kBoxesPerRow),
                /*width=*/20,
                /*height=*/20)),
        /*body=*/
        cros::mojom::KioskVisionBodyDetection::New(
            /*confidence=*/kConfidence,
            /*box=*/cros::mojom::KioskVisionBoundingBox::New(
                /*x=*/10 + 50 * (offset % kBoxesPerRow),
                /*y=*/20 + 50 * (offset / kBoxesPerRow),
                /*width=*/40,
                /*height=*/40)));

    fake_detection->appearances.push_back(std::move(fake_appearance));
  }
  fake_detection_flag_ = !fake_detection_flag_;
  fake_detection_offset_ = (fake_detection_offset_ + 1) % kMaxOffset;

  OnFrameProcessed(std::move(fake_detection));
}

}  // namespace ash::kiosk_vision
