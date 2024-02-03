// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/heatmap/heatmap_palm_detector.h"

#include "base/run_loop.h"
#include "chromeos/dbus/machine_learning/machine_learning_client.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

class HeatmapPalmDetectorTest : public testing::Test {
 public:
  void SetUp() override {
    chromeos::MachineLearningClient::InitializeFake();
    chromeos::machine_learning::ServiceConnection::
        UseFakeServiceConnectionForTesting(&fake_service_connection_);
    chromeos::machine_learning::ServiceConnection::GetInstance()->Initialize();
  }

  void TearDown() override { chromeos::MachineLearningClient::Shutdown(); }

 protected:
  chromeos::machine_learning::FakeServiceConnectionImpl
      fake_service_connection_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(HeatmapPalmDetectorTest, StartsService) {
  HeatmapPalmDetector detector;
  EXPECT_FALSE(detector.IsReady());
  detector.Start(HeatmapPalmDetector::DeviceId::kRex, "/dev/hidraw0");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(detector.IsReady());
  EXPECT_EQ(detector.GetDetectionResult(),
            HeatmapPalmDetector::DetectionResult::kNoPalm);

  auto palm_event =
      chromeos::machine_learning::mojom::HeatmapProcessedEvent::New();
  palm_event->is_palm = true;
  fake_service_connection_.SendHeatmapPalmRejectionEvent(std::move(palm_event));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(detector.GetDetectionResult(),
            HeatmapPalmDetector::DetectionResult::kPalm);
}

}  // namespace
}  // namespace ash
