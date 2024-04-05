// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/kiosk_vision.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/ash/components/kiosk/vision/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
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
  request.set_id(kKioskVisionDlcId);
  service.Install(request, future.GetCallback(), base::DoNothing());
  return future.Take();
}

dlcservice::DlcsWithContent GetExistingDlcs(FakeDlcserviceClient& service) {
  base::test::TestFuture<const dlcservice::DlcsWithContent&> future;
  service.GetExistingDlcs(
      base::BindOnce([](const std::string& err,
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

}  // namespace

class KioskVisionTest : public testing::Test {
 public:
  void SetUp() override { RegisterKioskVisionPrefs(local_state_); }

 protected:
  base::test::TaskEnvironment task_environment_;
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

}  // namespace ash::kiosk_vision
