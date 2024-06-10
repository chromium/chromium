// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/telemetry_processor.h"

#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::kiosk_vision {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

namespace {

cros::mojom::KioskVisionDetection NewFakeDetectionOfPersons(
    std::vector<int> person_ids) {
  std::vector<cros::mojom::KioskVisionAppearancePtr> appearances;
  for (int person_id : person_ids) {
    appearances.push_back(cros::mojom::KioskVisionAppearance::New(person_id));
  }
  return cros::mojom::KioskVisionDetection(std::move(appearances));
}

}  // namespace

TEST(TelemetryProcessorTest, StartsWithoutDetections) {
  TelemetryProcessor processor;
  ASSERT_THAT(processor.TakeIdsProcessed(), IsEmpty());
  ASSERT_THAT(processor.TakeErrors(), IsEmpty());
}

TEST(TelemetryProcessorTest, ReceivesDetections) {
  TelemetryProcessor processor;
  DetectionProcessor& detection_processor = processor;
  detection_processor.OnDetection(NewFakeDetectionOfPersons({123, 45}));

  EXPECT_THAT(processor.TakeIdsProcessed(), ElementsAreArray({123, 45}));
  EXPECT_THAT(processor.TakeErrors(), IsEmpty());
}

TEST(TelemetryProcessorTest, ReceivesErrors) {
  TelemetryProcessor processor;
  DetectionProcessor& detection_processor = processor;

  auto error = cros::mojom::KioskVisionError::MODEL_ERROR;
  detection_processor.OnError(error);

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
