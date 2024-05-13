// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/heatmap/heatmap_palm_detector_impl.h"

#include <memory>
#include <optional>

#include "base/run_loop.h"
#include "chromeos/dbus/machine_learning/machine_learning_client.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

base::Time TestTime(int64_t millis) {
  return base::Time::FromMillisecondsSinceUnixEpoch(millis);
}

class HeatmapPalmDetectorImplTest : public testing::Test {
 public:
  void SetUp() override {
    chromeos::MachineLearningClient::InitializeFake();
    chromeos::machine_learning::ServiceConnection::
        UseFakeServiceConnectionForTesting(&fake_service_connection_);
    chromeos::machine_learning::ServiceConnection::GetInstance()->Initialize();

    detector_ = std::make_unique<HeatmapPalmDetectorImpl>();
    EXPECT_FALSE(detector_->IsReady());
    detector_->Start(HeatmapPalmDetectorImpl::ModelId::kRex, "/dev/hidraw0",
                     std::nullopt);
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(detector_->IsReady());
  }

  void TearDown() override { chromeos::MachineLearningClient::Shutdown(); }

 protected:
  std::unique_ptr<HeatmapPalmDetectorImpl> detector_;
  chromeos::machine_learning::FakeServiceConnectionImpl
      fake_service_connection_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(HeatmapPalmDetectorImplTest, DetectsPalm) {
  detector_->AddTouchRecord(TestTime(40), {0});
  detector_->AddTouchRecord(TestTime(50), {1});

  auto palm_event =
      chromeos::machine_learning::mojom::HeatmapProcessedEvent::New();
  palm_event->is_palm = true;
  palm_event->timestamp = TestTime(52);
  fake_service_connection_.SendHeatmapPalmRejectionEvent(std::move(palm_event));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(detector_->IsPalm(0));
  EXPECT_TRUE(detector_->IsPalm(1));
}

TEST_F(HeatmapPalmDetectorImplTest, MatchesClosestRecord) {
  detector_->AddTouchRecord(TestTime(40), {0});
  detector_->AddTouchRecord(TestTime(50), {1});

  auto palm_event =
      chromeos::machine_learning::mojom::HeatmapProcessedEvent::New();
  palm_event->is_palm = true;
  palm_event->timestamp = TestTime(46);
  fake_service_connection_.SendHeatmapPalmRejectionEvent(std::move(palm_event));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(detector_->IsPalm(0));
  EXPECT_TRUE(detector_->IsPalm(1));
}

TEST_F(HeatmapPalmDetectorImplTest,
       DoesNotMatchFarawayRecordEvenIfItIsClosest) {
  detector_->AddTouchRecord(TestTime(40), {0});
  detector_->AddTouchRecord(TestTime(100), {1});

  auto palm_event =
      chromeos::machine_learning::mojom::HeatmapProcessedEvent::New();
  palm_event->is_palm = true;
  palm_event->timestamp = TestTime(70);
  fake_service_connection_.SendHeatmapPalmRejectionEvent(std::move(palm_event));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(detector_->IsPalm(0));
  EXPECT_FALSE(detector_->IsPalm(1));
}

}  // namespace
}  // namespace ash
