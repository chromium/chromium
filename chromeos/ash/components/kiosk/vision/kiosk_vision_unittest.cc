// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/kiosk_vision.h"

#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/ash/components/kiosk/vision/internal/fake_cros_camera_service.h"
#include "chromeos/ash/components/kiosk/vision/internal/pref_observer.h"
#include "chromeos/ash/components/kiosk/vision/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-forward.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace ash::kiosk_vision {

using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::IsEmpty;

namespace {

void RegisterKioskVisionPrefs(TestingPrefServiceSimple& local_state) {
  RegisterLocalStatePrefs(local_state.registry());
}

void EnableKioskVisionTelemetryPref(PrefService& pref_service) {
  pref_service.SetBoolean(prefs::kKioskVisionTelemetryEnabled, true);
}

void DisableKioskVisionTelemetryPref(PrefService& pref_service) {
  pref_service.SetBoolean(prefs::kKioskVisionTelemetryEnabled, false);
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

cros::mojom::KioskVisionDetectionPtr NewFakeDetectionOfPersons(
    std::vector<int> person_ids) {
  std::vector<cros::mojom::KioskVisionAppearancePtr> appearances;
  for (int person_id : person_ids) {
    appearances.push_back(cros::mojom::KioskVisionAppearance::New(person_id));
  }
  return cros::mojom::KioskVisionDetection::New(std::move(appearances));
}

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

TEST_F(KioskVisionTest, TelemetryPrefIsDisabledByDefault) {
  ASSERT_FALSE(IsTelemetryPrefEnabled(local_state_));
}

TEST_F(KioskVisionTest, InstallsDlcWhenEnabled) {
  ASSERT_FALSE(IsKioskVisionDlcInstalled(fake_dlcservice_));
  EnableKioskVisionTelemetryPref(local_state_);

  KioskVision vision(&local_state_);

  EXPECT_TRUE(IsKioskVisionDlcInstalled(fake_dlcservice_));
}

TEST_F(KioskVisionTest, UninstallsDlcWhenDisabled) {
  DisableKioskVisionTelemetryPref(local_state_);

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

TEST_F(KioskVisionTest, TelemetryProcessorIsNullWhenDisabled) {
  DisableKioskVisionTelemetryPref(local_state_);

  KioskVision vision(&local_state_);

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(vision.GetTelemetryProcessor(), nullptr);
}

TEST_F(KioskVisionTest, TelemetryProcessorIsNotNullWhenEnabled) {
  EnableKioskVisionTelemetryPref(local_state_);

  KioskVision vision(&local_state_);

  base::RunLoop().RunUntilIdle();
  ASSERT_NE(vision.GetTelemetryProcessor(), nullptr);
}

TEST_F(KioskVisionTest, TelemetryProcessorBecomesNullOnceDisabled) {
  EnableKioskVisionTelemetryPref(local_state_);

  KioskVision vision(&local_state_);

  base::RunLoop().RunUntilIdle();
  EXPECT_NE(vision.GetTelemetryProcessor(), nullptr);

  DisableKioskVisionTelemetryPref(local_state_);

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(vision.GetTelemetryProcessor(), nullptr);
}

TEST_F(KioskVisionTest, TelemetryProcessorStartsWithoutDetections) {
  EnableKioskVisionTelemetryPref(local_state_);

  KioskVision vision(&local_state_);

  ASSERT_TRUE(fake_cros_camera_service_.WaitForObserver());

  auto& processor = CHECK_DEREF(vision.GetTelemetryProcessor());

  base::RunLoop().RunUntilIdle();

  ASSERT_THAT(processor.TakeIdsProcessed(), IsEmpty());
  ASSERT_THAT(processor.TakeErrors(), IsEmpty());
}

TEST_F(KioskVisionTest, TelemetryProcessorReceivesDetections) {
  EnableKioskVisionTelemetryPref(local_state_);

  KioskVision vision(&local_state_);

  ASSERT_TRUE(fake_cros_camera_service_.WaitForObserver());

  auto& processor = CHECK_DEREF(vision.GetTelemetryProcessor());

  fake_cros_camera_service_.EmitFakeDetection(
      NewFakeDetectionOfPersons({123, 45}));

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(processor.TakeIdsProcessed(), ElementsAreArray({123, 45}));
  EXPECT_THAT(processor.TakeErrors(), IsEmpty());
}

TEST_F(KioskVisionTest, TelemetryProcessorReceivesErrors) {
  EnableKioskVisionTelemetryPref(local_state_);

  KioskVision vision(&local_state_);

  ASSERT_TRUE(fake_cros_camera_service_.WaitForObserver());

  auto& processor = CHECK_DEREF(vision.GetTelemetryProcessor());

  auto error = cros::mojom::KioskVisionError::MODEL_ERROR;
  fake_cros_camera_service_.EmitFakeError(error);

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(processor.TakeIdsProcessed(), IsEmpty());
  EXPECT_THAT(processor.TakeErrors(), ElementsAre(error));
}

}  // namespace ash::kiosk_vision
