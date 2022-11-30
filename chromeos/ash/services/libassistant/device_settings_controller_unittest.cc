// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/device_settings_controller.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/libassistant/public/mojom/device_settings_delegate.mojom.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/proto/shared/proto/device_args.pb.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace assistant {

bool operator==(const DeviceSetting& left, const DeviceSetting& right) {
  return (left.is_supported == right.is_supported) &&
         (left.setting_id == right.setting_id);
}

void PrintTo(const DeviceSetting& settings, std::ostream* out) {
  *out << "DeviceSettings {" << std::endl;
  *out << "    setting_id: " << settings.setting_id << std::endl;
  *out << "    is_supported: " << settings.is_supported << std::endl;
  *out << "}";
}

}  // namespace assistant
}  // namespace chromeos

namespace ash::libassistant {

namespace {

constexpr double kEpsilon = 0.001;

using ::assistant::api::client_op::GetDeviceSettingsArgs;
using ::assistant::api::client_op::ModifySettingArgs;
using chromeos::assistant::DeviceSetting;
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

class DeviceSettingsDelegateMock : public mojom::DeviceSettingsDelegate {
 public:
  // mojom::DeviceSettingsDelegate implementation:
  void GetScreenBrightnessLevel(
      GetScreenBrightnessLevelCallback callback) override {
    std::move(callback).Run(
        mojom::GetBrightnessResult::New(current_brightness_));
  }
  MOCK_METHOD(void, SetBluetoothEnabled, (bool enabled));
  MOCK_METHOD(void, SetDoNotDisturbEnabled, (bool enabled));
  MOCK_METHOD(void, SetNightLightEnabled, (bool enabled));
  MOCK_METHOD(void, SetScreenBrightnessLevel, (double level, bool gradual));
  MOCK_METHOD(void, SetSwitchAccessEnabled, (bool enabled));
  MOCK_METHOD(void, SetWifiEnabled, (bool enabled));

  mojo::PendingRemote<mojom::DeviceSettingsDelegate>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  void set_current_brightness(double value) { current_brightness_ = value; }

 private:
  mojo::Receiver<DeviceSettingsDelegate> receiver_{this};
  double current_brightness_;
};

}  // namespace

class AssistantDeviceSettingsControllerTest : public testing::Test {
 public:
  AssistantDeviceSettingsControllerTest() = default;

  void SetUp() override {
    controller().Bind(delegate_mock_.BindNewPipeAndPassRemote());
  }

  DeviceSetting GetSupportedDeviceSettings(const std::string& setting_id) {
    GetDeviceSettingsArgs args;
    args.add_setting_ids(setting_id);

    std::vector<DeviceSetting> result =
        controller().GetSupportedDeviceSettings(args);
    EXPECT_EQ(result.size(), 1u);
    return result[0];
  }

  void ModifySetting(const ModifySettingArgs& args) {
    controller().OnModifyDeviceSetting(args);
    delegate_mock().FlushForTesting();
    // When we're changing the brightness, we first fetch the current value
    // and then need to run a callback on the main thread.
    base::RunLoop().RunUntilIdle();
  }

  DeviceSettingsController& controller() { return controller_; }

  DeviceSettingsDelegateMock& delegate_mock() { return delegate_mock_; }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  DeviceSettingsController controller_;
  DeviceSettingsDelegateMock delegate_mock_;
};

std::ostream& operator<<(std::ostream& stream, const DeviceSetting& value) {
  stream << "{ " << value.setting_id << ": " << value.is_supported << "}";
  return stream;
}

TEST_F(AssistantDeviceSettingsControllerTest,
       GetDeviceSettingsShouldReturnFalseForUnknownSetting) {
  EXPECT_EQ(GetSupportedDeviceSettings("UNKNOWN_SETTING"),
            DeviceSetting("UNKNOWN_SETTING", false));
}

TEST_F(AssistantDeviceSettingsControllerTest,
       GetDeviceSettingsShouldReturnTrueForKnownSettings) {
  for (const std::string& setting : kAlwaysSupportedSettings) {
    EXPECT_EQ(GetSupportedDeviceSettings(setting), DeviceSetting(setting, true))
        << "Error for " << setting;
  }
}

TEST_F(AssistantDeviceSettingsControllerTest,
       GetDeviceSettingsShouldMultipleSettingsAtTheSameTime) {
  GetDeviceSettingsArgs args;
  args.add_setting_ids("UNKNOWN_SETTING");
  args.add_setting_ids(kWiFi);

  std::vector<DeviceSetting> result =
      controller().GetSupportedDeviceSettings(args);

  EXPECT_THAT(controller().GetSupportedDeviceSettings(args),
              ElementsAre(DeviceSetting("UNKNOWN_SETTING", false),
                          DeviceSetting(kWiFi, true)));
}

TEST_F(AssistantDeviceSettingsControllerTest, ShouldTurnWifiOnAndOff) {
  ModifySettingArgs args;
  args.set_setting_id(kWiFi);

  args.set_change(Change::ModifySettingArgs_Change_ON);
  EXPECT_CALL(delegate_mock(), SetWifiEnabled(true));
  ModifySetting(args);

  args.set_change(Change::ModifySettingArgs_Change_OFF);
  EXPECT_CALL(delegate_mock(), SetWifiEnabled(false));
  ModifySetting(args);
}

TEST_F(AssistantDeviceSettingsControllerTest, ShouldTurnBluetoothOnAndOff) {
  ModifySettingArgs args;
  args.set_setting_id(kBluetooth);

  args.set_change(Change::ModifySettingArgs_Change_ON);
  EXPECT_CALL(delegate_mock(), SetBluetoothEnabled(true));
  ModifySetting(args);

  args.set_change(Change::ModifySettingArgs_Change_OFF);
  EXPECT_CALL(delegate_mock(), SetBluetoothEnabled(false));
  ModifySetting(args);
}

TEST_F(AssistantDeviceSettingsControllerTest, ShouldTurnQuietModeOnAndOff) {
  ModifySettingArgs args;
  args.set_setting_id(kDoNotDisturb);

  args.set_change(Change::ModifySettingArgs_Change_ON);
  EXPECT_CALL(delegate_mock(), SetDoNotDisturbEnabled(true));
  ModifySetting(args);

  args.set_change(Change::ModifySettingArgs_Change_OFF);
  EXPECT_CALL(delegate_mock(), SetDoNotDisturbEnabled(false));
  ModifySetting(args);
}

TEST_F(AssistantDeviceSettingsControllerTest, ShouldTurnSwitchAccessOnAndOff) {
  ModifySettingArgs args;
  args.set_setting_id(kSwitchAccess);

  args.set_change(Change::ModifySettingArgs_Change_ON);
  EXPECT_CALL(delegate_mock(), SetSwitchAccessEnabled(true));
  ModifySetting(args);

  args.set_change(Change::ModifySettingArgs_Change_OFF);
  EXPECT_CALL(delegate_mock(), SetSwitchAccessEnabled(false));
  ModifySetting(args);
}

TEST_F(AssistantDeviceSettingsControllerTest, ShouldTurnNightLightOnAndOff) {
  ModifySettingArgs args;
  args.set_setting_id(kNightLight);

  args.set_change(Change::ModifySettingArgs_Change_ON);
  EXPECT_CALL(delegate_mock(), SetNightLightEnabled(true));
  ModifySetting(args);

  args.set_change(Change::ModifySettingArgs_Change_OFF);
  EXPECT_CALL(delegate_mock(), SetNightLightEnabled(false));
  ModifySetting(args);
}

TEST_F(AssistantDeviceSettingsControllerTest, ShouldSetBrightness) {
  ModifySettingArgs args;
  args.set_setting_id(kScreenBrightness);
  args.set_change(Change::ModifySettingArgs_Change_SET);

  // Set brightness to 20%
  args.set_numeric_value(0.2);
  args.set_unit(Unit::ModifySettingArgs_Unit_RANGE);
  EXPECT_CALL(delegate_mock(),
              SetScreenBrightnessLevel(DoubleNear(0.2, kEpsilon),
                                       /*gradual=*/true));
  ModifySetting(args);

  // Set brightness to 30.
  // This will be converted to a percentage
  args.set_numeric_value(30);
  args.set_unit(Unit::ModifySettingArgs_Unit_STEP);
  EXPECT_CALL(delegate_mock(),
              SetScreenBrightnessLevel(DoubleNear(0.3, kEpsilon),
                                       /*gradual=*/true));
  ModifySetting(args);
}

TEST_F(AssistantDeviceSettingsControllerTest,
       ShouldIncreaseAndDecreaseBrightness) {
  ModifySettingArgs args;
  args.set_setting_id(kScreenBrightness);

  // Increase brightness - this will use a default increment of 10%
  args.set_change(Change::ModifySettingArgs_Change_INCREASE);
  args.set_unit(Unit::ModifySettingArgs_Unit_UNKNOWN_UNIT);
  delegate_mock().set_current_brightness(0.2);
  EXPECT_CALL(delegate_mock(),
              SetScreenBrightnessLevel(DoubleNear(0.3, kEpsilon),
                                       /*gradual=*/true));
  ModifySetting(args);

  // Decrease brightness - this will use a default decrement of 10%
  args.set_change(Change::ModifySettingArgs_Change_DECREASE);
  args.set_unit(Unit::ModifySettingArgs_Unit_UNKNOWN_UNIT);
  delegate_mock().set_current_brightness(0.2);
  EXPECT_CALL(delegate_mock(),
              SetScreenBrightnessLevel(DoubleNear(0.1, kEpsilon),
                                       /*gradual=*/true));
  ModifySetting(args);
}

}  // namespace ash::libassistant
