// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_device_settings_delegate.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/assistant/controller/assistant_notification_controller.h"
#include "base/command_line.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/proto/google3/assistant/api/client_op/device_args.pb.h"
#include "chromeos/services/assistant/test_support/fake_service_context.h"
#include "chromeos/services/assistant/test_support/scoped_device_actions.h"
#include "libassistant/shared/public/platform_audio_output.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_switches.h"

namespace chromeos {
namespace assistant {

namespace {

constexpr double kEpsilon = 0.001;

using ::assistant::api::client_op::GetDeviceSettingsArgs;
using ::assistant::api::client_op::ModifySettingArgs;
using Change = ::assistant::api::client_op::ModifySettingArgs::Change;
using Unit = ::assistant::api::client_op::ModifySettingArgs::Unit;
using ::testing::AnyNumber;
using ::testing::DoubleNear;
using ::testing::ElementsAre;
using ::testing::FloatNear;
using ::testing::Return;
using ::testing::StrictMock;

constexpr char kWiFi[] = "WIFI";
constexpr char kBluetooth[] = "BLUETOOTH";
constexpr char kScreenBrightness[] = "BRIGHTNESS_LEVEL";
constexpr char kDoNotDisturb[] = "DO_NOT_DISTURB";
constexpr char kNightLight[] = "NIGHT_LIGHT_SWITCH";
constexpr char kSwitchAccess[] = "SWITCH_ACCESS";

// Returns the settings that are always supported.
const std::vector<std::string> kAlwaysSupportedSettings = {
    kWiFi,         kBluetooth,  kScreenBrightness,
    kDoNotDisturb, kNightLight, kSwitchAccess,
};

class ScopedDeviceActionsMock : public ScopedDeviceActions {
 public:
  // DeviceActions implementation:
  MOCK_METHOD(void, SetWifiEnabled, (bool enabled));
  MOCK_METHOD(void, SetBluetoothEnabled, (bool enabled));
  MOCK_METHOD(void, SetScreenBrightnessLevel, (double level, bool gradual));
  MOCK_METHOD(void, SetNightLightEnabled, (bool enabled));
  MOCK_METHOD(void, SetSwitchAccessEnabled, (bool enabled));
  MOCK_METHOD(bool,
              OpenAndroidApp,
              (const chromeos::assistant::AndroidAppInfo& app_info));
  MOCK_METHOD(chromeos::assistant::AppStatus,
              GetAndroidAppStatus,
              (const chromeos::assistant::AndroidAppInfo& app_info));
  MOCK_METHOD(void, LaunchAndroidIntent, (const std::string& intent));
  MOCK_METHOD(void,
              AddAppListEventSubscriber,
              (chromeos::assistant::AppListEventSubscriber * subscriber));
  MOCK_METHOD(void,
              RemoveAppListEventSubscriber,
              (chromeos::assistant::AppListEventSubscriber * subscriber));
};

class AssistantNotificationControllerMock
    : public ash::AssistantNotificationController {
 public:
  using AssistantNotification = chromeos::assistant::AssistantNotification;

  // ash::AssistantNotificationController implementation:
  MOCK_METHOD(void,
              AddOrUpdateNotification,
              (AssistantNotification && notification));
  MOCK_METHOD(void,
              RemoveNotificationById,
              (const std::string& id, bool from_server));
  MOCK_METHOD(void,
              RemoveNotificationByGroupingKey,
              (const std::string& grouping_id, bool from_server));
  MOCK_METHOD(void, RemoveAllNotifications, (bool from_server));
  MOCK_METHOD(void, SetQuietMode, (bool enabled));
};

class AssistantDeviceSettingsDelegateTest : public testing::Test {
 public:
  AssistantDeviceSettingsDelegateTest() = default;

  void SetUp() override {
    service_context_ = std::make_unique<FakeServiceContext>();

    CreateAssistantDeviceSettingsDelegate();
  }

  DeviceSetting GetDeviceSettings(const std::string& setting_id) {
    GetDeviceSettingsArgs args;
    args.add_setting_ids(setting_id);

    std::vector<DeviceSetting> result = delegate()->GetDeviceSettings(args);
    EXPECT_EQ(result.size(), 1u);
    return result[0];
  }

  // Warning: Use |CreateAssistantDeviceSettingsDelegate| to apply any changes
  // made to the service context.
  FakeServiceContext* service_context() { return service_context_.get(); }

  AssistantDeviceSettingsDelegate* delegate() { return delegate_.get(); }

  void CreateAssistantDeviceSettingsDelegate() {
    delegate_ = std::make_unique<AssistantDeviceSettingsDelegate>(
        service_context_.get());
  }

 private:
  std::unique_ptr<FakeServiceContext> service_context_;
  std::unique_ptr<AssistantDeviceSettingsDelegate> delegate_;
};

}  // namespace

bool operator==(const DeviceSetting& left, const DeviceSetting& right) {
  return (left.is_supported == right.is_supported) &&
         (left.setting_id == right.setting_id);
}

std::ostream& operator<<(std::ostream& stream, const DeviceSetting& value) {
  stream << "{ " << value.setting_id << ": " << value.is_supported << "}";
  return stream;
}

TEST_F(AssistantDeviceSettingsDelegateTest,
       IsSettingSupportedShouldReturnFalseForUnknownSetting) {
  EXPECT_FALSE(delegate()->IsSettingSupported("<unknown-setting>"));
}

TEST_F(AssistantDeviceSettingsDelegateTest,
       IsSettingSupportedShouldReturnTrueForKnownSettings) {
  for (const std::string& setting : kAlwaysSupportedSettings) {
    EXPECT_TRUE(delegate()->IsSettingSupported(setting))
        << "Error for " << setting;
  }
}

TEST_F(AssistantDeviceSettingsDelegateTest,
       GetDeviceSettingsShouldReturnFalseForUnknownSetting) {
  EXPECT_EQ(GetDeviceSettings("UNKNOWN_SETTING"),
            DeviceSetting("UNKNOWN_SETTING", false));
}

TEST_F(AssistantDeviceSettingsDelegateTest,
       GetDeviceSettingsShouldReturnTrueForKnownSettings) {
  for (const std::string& setting : kAlwaysSupportedSettings) {
    EXPECT_EQ(GetDeviceSettings(setting), DeviceSetting(setting, true))
        << "Error for " << setting;
  }
}

TEST_F(AssistantDeviceSettingsDelegateTest,
       GetDeviceSettingsShouldMultipleSettingsAtTheSameTime) {
  GetDeviceSettingsArgs args;
  args.add_setting_ids("UNKNOWN_SETTING");
  args.add_setting_ids(kWiFi);

  std::vector<DeviceSetting> result = delegate()->GetDeviceSettings(args);

  EXPECT_THAT(delegate()->GetDeviceSettings(args),
              ElementsAre(DeviceSetting("UNKNOWN_SETTING", false),
                          DeviceSetting(kWiFi, true)));
}

TEST_F(AssistantDeviceSettingsDelegateTest, ShouldTurnWifiOnAndOff) {
  StrictMock<ScopedDeviceActionsMock> device_actions;
  CreateAssistantDeviceSettingsDelegate();

  ModifySettingArgs args;
  args.set_setting_id(kWiFi);

  args.set_change(Change::ModifySettingArgs_Change_ON);
  EXPECT_CALL(device_actions, SetWifiEnabled(true));
  delegate()->HandleModifyDeviceSetting(args);

  args.set_change(Change::ModifySettingArgs_Change_OFF);
  EXPECT_CALL(device_actions, SetWifiEnabled(false));
  delegate()->HandleModifyDeviceSetting(args);
}

TEST_F(AssistantDeviceSettingsDelegateTest, ShouldTurnBluetoothOnAndOff) {
  StrictMock<ScopedDeviceActionsMock> device_actions;
  CreateAssistantDeviceSettingsDelegate();

  ModifySettingArgs args;
  args.set_setting_id(kBluetooth);

  args.set_change(Change::ModifySettingArgs_Change_ON);
  EXPECT_CALL(device_actions, SetBluetoothEnabled(true));
  delegate()->HandleModifyDeviceSetting(args);

  args.set_change(Change::ModifySettingArgs_Change_OFF);
  EXPECT_CALL(device_actions, SetBluetoothEnabled(false));
  delegate()->HandleModifyDeviceSetting(args);
}

TEST_F(AssistantDeviceSettingsDelegateTest, ShouldTurnQuietModeOnAndOff) {
  StrictMock<AssistantNotificationControllerMock> notification_controller;
  service_context()->set_assistant_notification_controller(
      &notification_controller);
  CreateAssistantDeviceSettingsDelegate();

  ModifySettingArgs args;
  args.set_setting_id(kDoNotDisturb);

  args.set_change(Change::ModifySettingArgs_Change_ON);
  EXPECT_CALL(notification_controller, SetQuietMode(true));
  delegate()->HandleModifyDeviceSetting(args);

  args.set_change(Change::ModifySettingArgs_Change_OFF);
  EXPECT_CALL(notification_controller, SetQuietMode(false));
  delegate()->HandleModifyDeviceSetting(args);
}

TEST_F(AssistantDeviceSettingsDelegateTest, ShouldTurnSwitchAccessOnAndOff) {
  StrictMock<ScopedDeviceActionsMock> device_actions;
  CreateAssistantDeviceSettingsDelegate();

  ModifySettingArgs args;
  args.set_setting_id(kSwitchAccess);

  args.set_change(Change::ModifySettingArgs_Change_ON);
  EXPECT_CALL(device_actions, SetSwitchAccessEnabled(true));
  delegate()->HandleModifyDeviceSetting(args);

  args.set_change(Change::ModifySettingArgs_Change_OFF);
  EXPECT_CALL(device_actions, SetSwitchAccessEnabled(false));
  delegate()->HandleModifyDeviceSetting(args);
}

TEST_F(AssistantDeviceSettingsDelegateTest, ShouldTurnNightLightOnAndOff) {
  StrictMock<ScopedDeviceActionsMock> device_actions;
  CreateAssistantDeviceSettingsDelegate();

  ModifySettingArgs args;
  args.set_setting_id(kNightLight);

  args.set_change(Change::ModifySettingArgs_Change_ON);
  EXPECT_CALL(device_actions, SetNightLightEnabled(true));
  delegate()->HandleModifyDeviceSetting(args);

  args.set_change(Change::ModifySettingArgs_Change_OFF);
  EXPECT_CALL(device_actions, SetNightLightEnabled(false));
  delegate()->HandleModifyDeviceSetting(args);
}

TEST_F(AssistantDeviceSettingsDelegateTest, ShouldSetBrightness) {
  StrictMock<ScopedDeviceActionsMock> device_actions;
  CreateAssistantDeviceSettingsDelegate();

  ModifySettingArgs args;
  args.set_setting_id(kScreenBrightness);
  args.set_change(Change::ModifySettingArgs_Change_SET);

  // Set brightness to 20%
  args.set_numeric_value(0.2);
  args.set_unit(Unit::ModifySettingArgs_Unit_RANGE);
  EXPECT_CALL(device_actions,
              SetScreenBrightnessLevel(DoubleNear(0.2, kEpsilon),
                                       /*gradual=*/true));
  delegate()->HandleModifyDeviceSetting(args);

  // Set brightness to 30.
  // This will be converted to a percentage
  args.set_numeric_value(30);
  args.set_unit(Unit::ModifySettingArgs_Unit_STEP);
  EXPECT_CALL(device_actions, SetScreenBrightnessLevel(
                                  DoubleNear(0.3, kEpsilon), /*gradual=*/true));
  delegate()->HandleModifyDeviceSetting(args);
}

TEST_F(AssistantDeviceSettingsDelegateTest,
       ShouldIncreaseAndDecreaseBrightness) {
  StrictMock<ScopedDeviceActionsMock> device_actions;
  CreateAssistantDeviceSettingsDelegate();

  ModifySettingArgs args;
  args.set_setting_id(kScreenBrightness);

  // Increase brightness - this will use a default increment of 10%
  args.set_change(Change::ModifySettingArgs_Change_INCREASE);
  args.set_unit(Unit::ModifySettingArgs_Unit_UNKNOWN_UNIT);
  device_actions.set_current_brightness(0.2);
  EXPECT_CALL(device_actions, SetScreenBrightnessLevel(
                                  DoubleNear(0.3, kEpsilon), /*gradual=*/true));
  delegate()->HandleModifyDeviceSetting(args);

  // Decrease brightness - this will use a default decrement of 10%
  args.set_change(Change::ModifySettingArgs_Change_DECREASE);
  args.set_unit(Unit::ModifySettingArgs_Unit_UNKNOWN_UNIT);
  device_actions.set_current_brightness(0.2);
  EXPECT_CALL(device_actions, SetScreenBrightnessLevel(
                                  DoubleNear(0.1, kEpsilon), /*gradual=*/true));
  delegate()->HandleModifyDeviceSetting(args);
}

}  // namespace assistant
}  // namespace chromeos
