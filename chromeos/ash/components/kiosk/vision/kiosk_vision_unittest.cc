// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/kiosk_vision.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/ash/components/kiosk/vision/internal/detection_processor.h"
#include "chromeos/ash/components/kiosk/vision/internal/fake_cros_camera_service.h"
#include "chromeos/ash/components/kiosk/vision/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-forward.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace ash::kiosk_vision {

namespace {

void RegisterKioskVisionPrefs(TestingPrefServiceSimple& local_state) {
  RegisterLocalStatePrefs(local_state.registry());
}

void EnableKioskVisionTelemetryPref(PrefService& pref_service) {
  pref_service.SetBoolean(prefs::kKioskVisionTelemetryEnabled, true);
}

DlcserviceClient::InstallResult InstallKioskVisionDlc(
    FakeDlcserviceClient& service) {
  base::test::TestFuture<const DlcserviceClient::InstallResult&> future;
  dlcservice::InstallRequest request;
  request.set_id(std::string(kKioskVisionDlcId));
  service.Install(request, future.GetCallback(), base::DoNothing());
  return future.Take();
}

dlcservice::DlcsWithContent GetExistingDlcs(FakeDlcserviceClient& service) {
  base::test::TestFuture<const dlcservice::DlcsWithContent&> future;
  service.GetExistingDlcs(
      base::BindOnce([](const std::string_view err,
                        const dlcservice::DlcsWithContent& dlcs) {
        return dlcs;
      }).Then(future.GetCallback()));
  return future.Take();
}

bool IsKioskVisionDlcInstalled(FakeDlcserviceClient& service) {
  const dlcservice::DlcsWithContent& dlcs = GetExistingDlcs(service);
  return base::ranges::any_of(dlcs.dlc_infos(), [](const auto& info) {
    return info.id() == kKioskVisionDlcId;
  });
}

cros::mojom::KioskVisionDetectionPtr NewFakeDetection() {
  std::vector<cros::mojom::KioskVisionAppearancePtr> appearances;
  appearances.push_back(cros::mojom::KioskVisionAppearance::New(42));
  appearances.push_back(cros::mojom::KioskVisionAppearance::New(13));
  return cros::mojom::KioskVisionDetection::New(std::move(appearances));
}

// Simple observer implementation that exposes events via `NextDetection` and
// `NextError`.
class FakeObserver : public DetectionProcessor {
 public:
  FakeObserver() = default;
  FakeObserver(const FakeObserver&) = delete;
  FakeObserver& operator=(const FakeObserver&) = delete;
  ~FakeObserver() override = default;

  cros::mojom::KioskVisionDetectionPtr NextDetection() {
    return detection_future_.Take();
  }

  cros::mojom::KioskVisionError NextError() { return error_future_.Take(); }

  // `DetectionProcessor` implementation.
  void OnDetection(
      const cros::mojom::KioskVisionDetection& detection) override {
    detection_future_.SetValue(detection.Clone());
  }

  void OnError(cros::mojom::KioskVisionError error) override {
    error_future_.SetValue(error);
  }

 private:
  base::test::TestFuture<cros::mojom::KioskVisionDetectionPtr>
      detection_future_;
  base::test::TestFuture<cros::mojom::KioskVisionError> error_future_;
};

}  // namespace

class KioskVisionTest : public testing::Test {
 public:
  void SetUp() override { RegisterKioskVisionPrefs(local_state_); }

 protected:
  base::test::TaskEnvironment task_environment_;
  FakeCrosCameraService fake_cros_camera_service_;
  TestingPrefServiceSimple local_state_;
  FakeDlcserviceClient fake_dlcservice_;
};

TEST_F(KioskVisionTest, InstallsDlcWhenEnabled) {
  ASSERT_FALSE(IsKioskVisionDlcInstalled(fake_dlcservice_));
  EnableKioskVisionTelemetryPref(local_state_);

  KioskVision vision(&local_state_);

  EXPECT_TRUE(IsKioskVisionDlcInstalled(fake_dlcservice_));
}

TEST_F(KioskVisionTest, UninstallsDlcWhenDisabled) {
  const auto& result = InstallKioskVisionDlc(fake_dlcservice_);
  ASSERT_EQ(result.error, dlcservice::kErrorNone);
  EXPECT_TRUE(IsKioskVisionDlcInstalled(fake_dlcservice_));

  KioskVision vision(&local_state_);

  EXPECT_FALSE(IsKioskVisionDlcInstalled(fake_dlcservice_));
}

TEST_F(KioskVisionTest, BindsDetectionObserver) {
  ASSERT_FALSE(fake_cros_camera_service_.HasObserver());
  EnableKioskVisionTelemetryPref(local_state_);

  KioskVision vision(&local_state_);

  ASSERT_TRUE(fake_cros_camera_service_.WaitForObserver());
  ASSERT_TRUE(fake_cros_camera_service_.HasObserver());
}

TEST_F(KioskVisionTest, ProcessorReceivesDetections) {
  EnableKioskVisionTelemetryPref(local_state_);

  FakeObserver observer;
  KioskVision vision(&local_state_, &observer);

  ASSERT_TRUE(fake_cros_camera_service_.WaitForObserver());

  auto detection = NewFakeDetection();
  fake_cros_camera_service_.EmitFakeDetection(detection->Clone());

  ASSERT_EQ(detection, observer.NextDetection());
}

TEST_F(KioskVisionTest, ProcessorReceivesErrors) {
  EnableKioskVisionTelemetryPref(local_state_);

  FakeObserver observer;
  KioskVision vision(&local_state_, &observer);

  ASSERT_TRUE(fake_cros_camera_service_.WaitForObserver());

  auto error = cros::mojom::KioskVisionError::MODEL_ERROR;
  fake_cros_camera_service_.EmitFakeError(error);

  ASSERT_EQ(error, observer.NextError());
}

}  // namespace ash::kiosk_vision
