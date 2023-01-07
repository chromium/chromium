// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/device_actions.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/assistant/device_actions_delegate.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::ash::assistant::AndroidAppInfo;
using ::ash::assistant::AppStatus;

namespace {

constexpr char kRegisteredAppName[] = "registered_app_name";
constexpr char kOtherRegisteredAppName[] = "other_registered_app_name";
constexpr char kUnregisteredAppName[] = "unregistered_app_name";

class FakeDeviceActionsDelegate : public DeviceActionsDelegate {
  AppStatus GetAndroidAppStatus(const std::string& package_name) override {
    return apps_.find(package_name) != apps_.end() ? AppStatus::kAvailable
                                                   : AppStatus::kUnavailable;
  }

 private:
  std::set<std::string> apps_ = {kRegisteredAppName, kOtherRegisteredAppName};
};

}  // namespace

class DeviceActionsTest : public ChromeAshTestBase {
 public:
  DeviceActionsTest() = default;
  ~DeviceActionsTest() override = default;

  void SetUp() override {
    ChromeAshTestBase::SetUp();
    device_actions_ = std::make_unique<DeviceActions>(
        std::make_unique<FakeDeviceActionsDelegate>());
  }

  void TearDown() override {
    device_actions_.reset();
    ChromeAshTestBase::TearDown();
  }

  DeviceActions* device_actions() { return device_actions_.get(); }

  AppStatus GetAppStatus(std::string package_name) {
    AndroidAppInfo app_info;
    app_info.package_name = package_name;

    return device_actions()->GetAndroidAppStatus(app_info);
  }

 private:
  std::unique_ptr<DeviceActions> device_actions_;
};

TEST_F(DeviceActionsTest, RegisteredAppShouldBeAvailable) {
  ASSERT_EQ(GetAppStatus(kRegisteredAppName), AppStatus::kAvailable);
}

TEST_F(DeviceActionsTest, UnregisteredAppShouldBeUnavailable) {
  ASSERT_EQ(GetAppStatus(kUnregisteredAppName), AppStatus::kUnavailable);
}

TEST_F(DeviceActionsTest, UnknownAppShouldBeUnknown) {
}

TEST_F(DeviceActionsTest, MultipleAppsShouldBeVerifiedCorrectly) {
  ASSERT_EQ(GetAppStatus(kRegisteredAppName), AppStatus::kAvailable);
  ASSERT_EQ(GetAppStatus(kUnregisteredAppName), AppStatus::kUnavailable);
  ASSERT_EQ(GetAppStatus(kOtherRegisteredAppName), AppStatus::kAvailable);
}
