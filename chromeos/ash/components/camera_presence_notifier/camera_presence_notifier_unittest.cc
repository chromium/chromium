// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/camera_presence_notifier/camera_presence_notifier.h"

#include <string>

#include "ash/capture_mode/fake_video_source_provider.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "content/public/browser/video_capture_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kFakeCameraDeviceId[] = "/dev/videoX";
constexpr char kFakeCameraDisplayName[] = "Fake Camera";
constexpr char kFakeCameraModelId[] = "0def:c000";

}  // namespace

class FakeVideoCaptureService
    : public video_capture::mojom::VideoCaptureService {
 public:
  FakeVideoCaptureService() = default;
  ~FakeVideoCaptureService() override = default;

  void AddFakeCamera() {
    const std::string camera_id =
        kFakeCameraDeviceId + base::NumberToString(next_id_++);
    used_ids_.push_back(camera_id);
    fake_provider_.AddFakeCameraWithoutNotifying(
        camera_id, kFakeCameraDisplayName, kFakeCameraModelId,
        media::MEDIA_VIDEO_FACING_NONE);
  }

  void RemoveFakeCamera() {
    ASSERT_FALSE(used_ids_.empty());
    fake_provider_.RemoveFakeCameraWithoutNotifying(used_ids_.back());
    used_ids_.pop_back();
  }

  // mojom::VideoCaptureService:
  void InjectGpuDependencies(
      mojo::PendingRemote<video_capture::mojom::AcceleratorFactory>
          accelerator_factory) override {}

  void ConnectToCameraAppDeviceBridge(
      mojo::PendingReceiver<cros::mojom::CameraAppDeviceBridge> receiver)
      override {}

  void BindVideoCaptureDeviceFactory(
      mojo::PendingReceiver<crosapi::mojom::VideoCaptureDeviceFactory> receiver)
      override {}

  void ConnectToVideoSourceProvider(
      mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider> receiver)
      override {
    fake_provider_.Bind(std::move(receiver));
  }

  void BindControlsForTesting(
      mojo::PendingReceiver<video_capture::mojom::TestingControls> receiver)
      override {}

 private:
  FakeVideoSourceProvider fake_provider_;
  std::vector<std::string> used_ids_;
  int next_id_{0};
};

class CameraPresenceNotifierTest : public testing::Test {
 public:
  CameraPresenceNotifierTest() = default;

  CameraPresenceNotifierTest(const CameraPresenceNotifierTest&) = delete;
  CameraPresenceNotifierTest& operator=(const CameraPresenceNotifierTest&) =
      delete;

  ~CameraPresenceNotifierTest() override = default;

  void AddFakeCamera() { fake_service_.AddFakeCamera(); }

  void RemoveFakeCamera() { fake_service_.RemoveFakeCamera(); }

  // Advances time past the poll interval.
  void StepClock() { task_environment_.FastForwardBy(base::Seconds(10)); }

  // testing::Test:
  void SetUp() override {
    content::OverrideVideoCaptureServiceForTesting(&fake_service_);
  }

  void TearDown() override {
    content::OverrideVideoCaptureServiceForTesting(nullptr);
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeVideoCaptureService fake_service_;
};

// Tests that the observer of CameraPresenceNotifier works correctly when the
// camera is added/removed (using the presence callback).
TEST_F(CameraPresenceNotifierTest, TestPresenceObserver) {
  std::vector<bool> values;
  CameraPresenceNotifier::CameraPresenceCallback callback = base::BindRepeating(
      [](std::vector<bool>* values, bool camera_is_present) {
        values->push_back(camera_is_present);
      },
      &values);

  CameraPresenceNotifier notifier(callback);

  // No events before start.
  ASSERT_TRUE(values.empty());
  notifier.Start();
  StepClock();
  // The first result should be false since there are no available cameras.
  ASSERT_EQ(1U, values.size());
  EXPECT_FALSE(values.back());

  // Advance clock to verify that unchanged values don't cause callbacks.
  StepClock();
  ASSERT_EQ(1U, values.size());

  // Add a camera.
  AddFakeCamera();
  StepClock();
  ASSERT_EQ(2U, values.size());
  // There is a camera now.
  EXPECT_TRUE(values.back());

  // Camera removed. Next callback is false again.
  RemoveFakeCamera();
  StepClock();
  ASSERT_EQ(3U, values.size());
  EXPECT_FALSE(values.back());
}

// Tests that the observer of CameraPresenceNotifier works correctly when the
// cameras are added/removed (using the count callback).
TEST_F(CameraPresenceNotifierTest, TestCountObserver) {
  std::vector<int> values;
  CameraPresenceNotifier::CameraCountCallback callback = base::BindRepeating(
      [](std::vector<int>* values, int camera_count) {
        values->push_back(camera_count);
      },
      &values);

  CameraPresenceNotifier notifier(callback);

  // No events before start.
  ASSERT_TRUE(values.empty());
  notifier.Start();
  StepClock();
  // The first result should be 0 since there are no available cameras.
  ASSERT_EQ(1U, values.size());
  EXPECT_EQ(0, values.back());

  // Advance clock to verify that unchanged values don't cause callbacks.
  StepClock();
  ASSERT_EQ(1U, values.size());

  // Add a camera.
  AddFakeCamera();
  StepClock();
  ASSERT_EQ(2U, values.size());
  // There is a camera now.
  EXPECT_EQ(1, values.back());

  // Camera removed.
  RemoveFakeCamera();
  StepClock();
  ASSERT_EQ(3U, values.size());
  EXPECT_EQ(0, values.back());

  // Add 1 camera.
  AddFakeCamera();
  StepClock();
  ASSERT_EQ(4U, values.size());
  EXPECT_EQ(1, values.back());
  // Add 2nd camera.
  AddFakeCamera();
  StepClock();
  ASSERT_EQ(5U, values.size());
  EXPECT_EQ(2, values.back());

  // Remove 2nd camera
  RemoveFakeCamera();
  StepClock();
  ASSERT_EQ(6U, values.size());
  EXPECT_EQ(1, values.back());
  // Remove 1st camera
  RemoveFakeCamera();
  StepClock();
  ASSERT_EQ(7U, values.size());
  EXPECT_EQ(0, values.back());
}

}  // namespace ash
