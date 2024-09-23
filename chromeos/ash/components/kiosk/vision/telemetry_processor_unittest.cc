// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/telemetry_processor.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "chromeos/ash/components/kiosk/vision/internal/detection_observer.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::kiosk_vision {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

namespace {

cros::mojom::KioskVisionDetectionPtr NewFakeDetectionOfPersons(
    std::vector<int> person_ids) {
  constexpr int64_t kFakeTimestamp = 1718727537817601;

  std::vector<cros::mojom::KioskVisionAppearancePtr> appearances;
  for (int person_id : person_ids) {
    auto appearance = cros::mojom::KioskVisionAppearance::New();
    appearance->person_id = person_id;
    appearance->face = cros::mojom::KioskVisionFaceDetection::New();
    appearance->face->box = cros::mojom::KioskVisionBoundingBox::New();
    appearances.push_back(std::move(appearance));
  }
  return cros::mojom::KioskVisionDetection::New(kFakeTimestamp,
                                                std::move(appearances));
}

}  // namespace

TEST(TelemetryProcessorTest, StartsWithoutDetections) {
  TelemetryProcessor processor;
  ASSERT_THAT(processor.TakeIdsProcessed(), IsEmpty());
  ASSERT_THAT(processor.TakeErrors(), IsEmpty());
}

TEST(TelemetryProcessorTest, ReceivesDetections) {
  TelemetryProcessor processor;
  auto observer = DetectionObserver({&processor});
  observer.OnFrameProcessed(NewFakeDetectionOfPersons({123, 45}));

  EXPECT_THAT(processor.TakeIdsProcessed(), ElementsAreArray({123, 45}));
  EXPECT_THAT(processor.TakeErrors(), IsEmpty());
}

TEST(TelemetryProcessorTest, ReceivesErrors) {
  TelemetryProcessor processor;
  auto observer = DetectionObserver({&processor});

  auto error = cros::mojom::KioskVisionError::MODEL_ERROR;
  observer.OnError(error);

  EXPECT_THAT(processor.TakeIdsProcessed(), IsEmpty());
  EXPECT_THAT(processor.TakeErrors(), ElementsAre(error));
}

TEST(TelemetryProcessorTest, TelemetryDataWithoutDetectionsOrErrors) {
  TelemetryProcessor processor;

  auto telemetry_data = processor.GenerateTelemetryData();

  EXPECT_TRUE(telemetry_data.has_kiosk_vision_telemetry());
  EXPECT_TRUE(telemetry_data.has_kiosk_vision_status());

  // TODO(b/343029419): Add more expectations for content of telemetry data
  // without detections.
}

}  // namespace ash::kiosk_vision
