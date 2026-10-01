// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/device/input_device_settings/input_device_settings_provider.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/cpp/test/mock_input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/keyboard_brightness_control_delegate.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/webui/ash/settings/pages/device/input_device_settings/input_device_settings_provider.mojom.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace ash::settings {
namespace {
template <bool sorted = false, typename T>
void ExpectListsEqual(const std::vector<T>& expected_list,
                      const std::vector<T>& actual_list) {
  ASSERT_EQ(expected_list.size(), actual_list.size());
  if constexpr (sorted) {
    for (size_t i = 0; i < expected_list.size(); i++) {
      EXPECT_EQ(expected_list[i], actual_list[i]);
    }
    return;
  }

  for (size_t i = 0; i < expected_list.size(); i++) {
    auto actual_iter = std::ranges::find(actual_list, expected_list[i]);
    EXPECT_NE(actual_list.end(), actual_iter);
    if (actual_iter != actual_list.end()) {
      EXPECT_EQ(expected_list[i], *actual_iter);
    }
  }
}

class FakeKeyboardSettingsObserver : public mojom::KeyboardSettingsObserver {
 public:
  void OnKeyboardListUpdated(
      std::vector<::ash::mojom::KeyboardPtr> keyboards) override {
    keyboards_ = std::move(keyboards);
    ++num_times_keyboard_list_updated_;
  }

  void OnKeyboardPoliciesUpdated(
      ::ash::mojom::KeyboardPoliciesPtr keyboard_policies) override {
    ++num_times_keyboard_policies_updated_;
  }

  const std::vector<::ash::mojom::KeyboardPtr>& keyboards() {
    return keyboards_;
  }

  int num_times_keyboard_list_updated() {
    return num_times_keyboard_list_updated_;
  }

  int num_times_keyboard_policies_updated() {
    return num_times_keyboard_policies_updated_;
  }

  mojo::Receiver<mojom::KeyboardSettingsObserver> receiver{this};

 private:
  std::vector<::ash::mojom::KeyboardPtr> keyboards_;
  int num_times_keyboard_list_updated_ = 0;
  int num_times_keyboard_policies_updated_ = 0;
};

class FakeTouchpadSettingsObserver : public mojom::TouchpadSettingsObserver {
 public:
  void OnTouchpadListUpdated(
      std::vector<::ash::mojom::TouchpadPtr> touchpads) override {
    touchpads_ = std::move(touchpads);
    ++num_times_called_;
  }

  const std::vector<::ash::mojom::TouchpadPtr>& touchpads() {
    return touchpads_;
  }

  int num_times_called() { return num_times_called_; }
  mojo::Receiver<mojom::TouchpadSettingsObserver> receiver{this};

 private:
  std::vector<::ash::mojom::TouchpadPtr> touchpads_;
  int num_times_called_ = 0;
};

class FakePointingStickSettingsObserver
    : public mojom::PointingStickSettingsObserver {
 public:
  void OnPointingStickListUpdated(
      std::vector<::ash::mojom::PointingStickPtr> pointing_sticks) override {
    pointing_sticks_ = std::move(pointing_sticks);
    ++num_times_called_;
  }

  const std::vector<::ash::mojom::PointingStickPtr>& pointing_sticks() {
    return pointing_sticks_;
  }

  int num_times_called() { return num_times_called_; }
  mojo::Receiver<mojom::PointingStickSettingsObserver> receiver{this};

 private:
  std::vector<::ash::mojom::PointingStickPtr> pointing_sticks_;
  int num_times_called_ = 0;
};

class FakeMouseSettingsObserver : public mojom::MouseSettingsObserver {
 public:
  void OnMouseListUpdated(std::vector<::ash::mojom::MousePtr> mice) override {
    mice_ = std::move(mice);
    ++num_times_mouse_list_updated_;
  }

  void OnMousePoliciesUpdated(
      ::ash::mojom::MousePoliciesPtr mouse_policies) override {
    ++num_times_mouse_policies_updated_;
  }

  const std::vector<::ash::mojom::MousePtr>& mice() { return mice_; }

  int num_times_mouse_list_updated() { return num_times_mouse_list_updated_; }
  int num_times_mouse_policies_updated() {
    return num_times_mouse_policies_updated_;
  }
  mojo::Receiver<mojom::MouseSettingsObserver> receiver{this};

 private:
  std::vector<::ash::mojom::MousePtr> mice_;
  int num_times_mouse_list_updated_ = 0;
  int num_times_mouse_policies_updated_ = 0;
};

class FakeGraphicsTabletSettingsObserver
    : public mojom::GraphicsTabletSettingsObserver {
 public:
  void OnGraphicsTabletListUpdated(
      std::vector<::ash::mojom::GraphicsTabletPtr> graphics_tablets) override {
    graphics_tablets_ = std::move(graphics_tablets);
    ++num_times_graphics_tablet_list_updated_;
  }

  const std::vector<::ash::mojom::GraphicsTabletPtr>& graphics_tablets() {
    return graphics_tablets_;
  }

  int num_times_graphics_tablet_list_updated() {
    return num_times_graphics_tablet_list_updated_;
  }
  mojo::Receiver<mojom::GraphicsTabletSettingsObserver> receiver{this};

 private:
  std::vector<::ash::mojom::GraphicsTabletPtr> graphics_tablets_;
  int num_times_graphics_tablet_list_updated_ = 0;
};

class FakeButtonPressObserver : public mojom::ButtonPressObserver {
 public:
  void OnButtonPressed(::ash::mojom::ButtonPtr button) override {
    last_pressed_button_ = std::move(button);
  }

  bool has_last_pressed_button() {
    return last_pressed_button_.get() != nullptr;
  }

  const ::ash::mojom::Button& last_pressed_button() {
    DCHECK(last_pressed_button_);
    return *last_pressed_button_;
  }

  mojo::Receiver<mojom::ButtonPressObserver> receiver{this};

 private:
  ::ash::mojom::ButtonPtr last_pressed_button_;
};

class FakeKeyboardBrightnessObserver
    : public mojom::KeyboardBrightnessObserver {
 public:
  void OnKeyboardBrightnessChanged(double percent) override {
    keyboard_brightness_ = percent;
    ++num_times_called_;
  }
  double keyboard_brightness() { return keyboard_brightness_; }

  int num_times_called() { return num_times_called_; }

  mojo::Receiver<mojom::KeyboardBrightnessObserver> receiver{this};

 private:
  int num_times_called_ = 0;
  double keyboard_brightness_ = 0;
};

class FakeKeyboardAmbientLightSensorObserver
    : public mojom::KeyboardAmbientLightSensorObserver {
 public:
  void OnKeyboardAmbientLightSensorEnabledChanged(bool enabled) override {
    keyboard_ambient_light_sensor_enabled_ = enabled;
    ++num_times_called_;
  }
  bool keyboard_ambient_light_sensor_enabled() {
    return keyboard_ambient_light_sensor_enabled_;
  }
  int num_times_called() { return num_times_called_; }
  mojo::Receiver<mojom::KeyboardAmbientLightSensorObserver> receiver{this};

 private:
  int num_times_called_ = 0;
  bool keyboard_ambient_light_sensor_enabled_ = true;
};

class FakeLidStateObserver : public mojom::LidStateObserver {
 public:
  // mojom::LidStateObserver:
  void OnLidStateChanged(bool is_lid_open) override {
    ++num_lid_state_change_calls_;
    is_lid_open_ = is_lid_open;
  }

  bool is_lid_open() { return is_lid_open_; }

  int num_lid_state_change_calls() const { return num_lid_state_change_calls_; }

  mojo::Receiver<mojom::LidStateObserver> receiver{this};

 private:
  int num_lid_state_change_calls_ = 0;
  bool is_lid_open_ = true;
};

class FakeKeyboardBrightnessControlDelegate
    : public KeyboardBrightnessControlDelegate {
 public:
  FakeKeyboardBrightnessControlDelegate() = default;
  ~FakeKeyboardBrightnessControlDelegate() override = default;

  // override methods:
  void HandleKeyboardBrightnessDown() override {}
  void HandleKeyboardBrightnessUp() override {}
  void HandleToggleKeyboardBacklight() override {}
  void HandleSetKeyboardBrightness(
      double percent,
      bool gradual,
      KeyboardBrightnessChangeSource source) override {
    keyboard_brightness_ = percent;
    keyboard_brightness_change_source_ = source;
  }
  void HandleGetKeyboardBrightness(
      base::OnceCallback<void(std::optional<double>)> callback) override {
    std::move(callback).Run(keyboard_brightness_);
  }
  void HandleSetKeyboardAmbientLightSensorEnabled(
      bool enabled,
      KeyboardAmbientLightSensorEnabledChangeSource source) override {
    keyboard_ambient_light_sensor_enabled_ = enabled;
  }
  void HandleGetKeyboardAmbientLightSensorEnabled(
      base::OnceCallback<void(std::optional<bool>)> callback) override {
    std::move(callback).Run(keyboard_ambient_light_sensor_enabled_);
  }

  double keyboard_brightness() { return keyboard_brightness_; }
  KeyboardBrightnessChangeSource keyboard_brightness_change_source() const {
    return keyboard_brightness_change_source_;
  }
  bool keyboard_ambient_light_sensor_enabled() {
    return keyboard_ambient_light_sensor_enabled_;
  }

 private:
  double keyboard_brightness_ = 0;
  bool keyboard_ambient_light_sensor_enabled_ = true;
  KeyboardBrightnessChangeSource keyboard_brightness_change_source_ =
      KeyboardBrightnessChangeSource::kRestoredFromUserPref;
};

class FakeInputDeviceSettingsController
    : public MockInputDeviceSettingsController {
 public:
  // InputDeviceSettingsController:
  std::vector<::ash::mojom::KeyboardPtr> GetConnectedKeyboards() override {
    return mojo::Clone(keyboards_);
  }
  std::vector<::ash::mojom::TouchpadPtr> GetConnectedTouchpads() override {
    return mojo::Clone(touchpads_);
  }
  std::vector<::ash::mojom::MousePtr> GetConnectedMice() override {
    return mojo::Clone(mice_);
  }
  std::vector<::ash::mojom::PointingStickPtr> GetConnectedPointingSticks()
      override {
    return mojo::Clone(pointing_sticks_);
  }
  std::vector<::ash::mojom::GraphicsTabletPtr> GetConnectedGraphicsTablets()
      override {
    return mojo::Clone(graphics_tablets_);
  }
  const ::ash::mojom::KeyboardPolicies& GetKeyboardPolicies() override {
    return *keyboard_policies_;
  }
  const ::ash::mojom::MousePolicies& GetMousePolicies() override {
    return *mouse_policies_;
  }
  void RestoreDefaultKeyboardRemappings(DeviceId id) override {
    ++num_times_restore_default_keyboard_remappings_called_;
  }
  bool SetKeyboardSettings(
      DeviceId id,
      ::ash::mojom::KeyboardSettingsPtr settings) override {
    return MockInputDeviceSettingsController::SetKeyboardSettings(
        id, std::move(settings));
  }
  void AddObserver(Observer* observer) override { observer_ = observer; }
  void RemoveObserver(Observer* observer) override { observer_ = nullptr; }
  bool SetTouchpadSettings(
      DeviceId id,
      ::ash::mojom::TouchpadSettingsPtr settings) override {
    return MockInputDeviceSettingsController::SetTouchpadSettings(
        id, std::move(settings));
  }
  bool SetMouseSettings(DeviceId id,
                        ::ash::mojom::MouseSettingsPtr settings) override {
    return MockInputDeviceSettingsController::SetMouseSettings(
        id, std::move(settings));
  }
  bool SetPointingStickSettings(
      DeviceId id,
      ::ash::mojom::PointingStickSettingsPtr settings) override {
    return MockInputDeviceSettingsController::SetPointingStickSettings(
        id, std::move(settings));
  }
  bool SetGraphicsTabletSettings(
      DeviceId id,
      ::ash::mojom::GraphicsTabletSettingsPtr settings) override {
    return MockInputDeviceSettingsController::SetGraphicsTabletSettings(
        id, std::move(settings));
  }

  void StartObservingButtons(DeviceId id) override {
    observed_currently_ = true;
  }
  void StopObservingButtons() override { observed_currently_ = false; }

  void AddKeyboard(::ash::mojom::KeyboardPtr keyboard) {
    keyboards_.push_back(std::move(keyboard));
    observer_->OnKeyboardConnected(*keyboards_.back());
  }
  void RemoveKeyboard(uint32_t device_id) {
    auto iter =
        std::ranges::find_if(keyboards_, [device_id](const auto& keyboard) {
          return keyboard->id == device_id;
        });
    if (iter == keyboards_.end()) {
      return;
    }
    auto temp_keyboard = std::move(*iter);
    keyboards_.erase(iter);
    observer_->OnKeyboardDisconnected(*temp_keyboard);
  }
  void SetKeyboardPolicies(::ash::mojom::KeyboardPoliciesPtr policies) {
    keyboard_policies_ = std::move(policies);
    observer_->OnKeyboardPoliciesUpdated(*keyboard_policies_);
  }
  void SetMousePolicies(::ash::mojom::MousePoliciesPtr policies) {
    mouse_policies_ = std::move(policies);
    observer_->OnMousePoliciesUpdated(*mouse_policies_);
  }
  void AddMouse(::ash::mojom::MousePtr mouse) {
    mice_.push_back(std::move(mouse));
    observer_->OnMouseConnected(*mice_.back());
  }
  void RemoveMouse(uint32_t device_id) {
    auto iter = std::ranges::find_if(mice_, [device_id](const auto& mouse) {
      return mouse->id == device_id;
    });
    if (iter == mice_.end()) {
      return;
    }
    auto temp_mouse = std::move(*iter);
    mice_.erase(iter);
    observer_->OnMouseDisconnected(*temp_mouse);
  }
  void AddTouchpad(::ash::mojom::TouchpadPtr touchpad) {
    touchpads_.push_back(std::move(touchpad));
    observer_->OnTouchpadConnected(*touchpads_.back());
  }
  void RemoveTouchpad(uint32_t device_id) {
    auto iter =
        std::ranges::find_if(touchpads_, [device_id](const auto& touchpad) {
          return touchpad->id == device_id;
        });
    if (iter == touchpads_.end()) {
      return;
    }
    auto temp_touchpad = std::move(*iter);
    touchpads_.erase(iter);
    observer_->OnTouchpadDisconnected(*temp_touchpad);
  }
  void AddPointingStick(::ash::mojom::PointingStickPtr pointing_stick) {
    pointing_sticks_.push_back(std::move(pointing_stick));
    observer_->OnPointingStickConnected(*pointing_sticks_.back());
  }
  void RemovePointingStick(uint32_t device_id) {
    auto iter = std::ranges::find_if(pointing_sticks_,
                                     [device_id](const auto& pointing_stick) {
                                       return pointing_stick->id == device_id;
                                     });
    if (iter == pointing_sticks_.end()) {
      return;
    }
    auto temp_pointing_stick = std::move(*iter);
    pointing_sticks_.erase(iter);
    observer_->OnPointingStickDisconnected(*temp_pointing_stick);
  }
  void AddGraphicsTablet(::ash::mojom::GraphicsTabletPtr graphics_tablet) {
    graphics_tablets_.push_back(std::move(graphics_tablet));
    observer_->OnGraphicsTabletConnected(*graphics_tablets_.back());
  }
  void RemoveGraphicsTablet(uint32_t device_id) {
    auto iter = std::ranges::find_if(graphics_tablets_,
                                     [device_id](const auto& graphics_tablet) {
                                       return graphics_tablet->id == device_id;
                                     });
    if (iter == graphics_tablets_.end()) {
      return;
    }
    auto temp_pointing_stick = std::move(*iter);
    graphics_tablets_.erase(iter);
    observer_->OnGraphicsTabletDisconnected(*temp_pointing_stick);
  }
  int num_times_restore_default_keyboard_remappings_called() {
    return num_times_restore_default_keyboard_remappings_called_;
  }
  bool observed_currently() { return observed_currently_; }

 private:
  std::vector<::ash::mojom::KeyboardPtr> keyboards_;
  std::vector<::ash::mojom::TouchpadPtr> touchpads_;
  std::vector<::ash::mojom::MousePtr> mice_;
  std::vector<::ash::mojom::PointingStickPtr> pointing_sticks_;
  std::vector<::ash::mojom::GraphicsTabletPtr> graphics_tablets_;
  ::ash::mojom::KeyboardPoliciesPtr keyboard_policies_ =
      ::ash::mojom::KeyboardPolicies::New();
  ::ash::mojom::MousePoliciesPtr mouse_policies_ =
      ::ash::mojom::MousePolicies::New();

  raw_ptr<InputDeviceSettingsController::Observer> observer_ = nullptr;
  int num_times_restore_default_keyboard_remappings_called_ = 0;
  bool observed_currently_ = false;
};

}  // namespace

class InputDeviceSettingsProviderTest : public views::ViewsTestBase {
 public:
  InputDeviceSettingsProviderTest() = default;
  ~InputDeviceSettingsProviderTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();
    widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->Show();
    scoped_resetter_ = std::make_unique<
        InputDeviceSettingsController::ScopedResetterForTest>();
    controller_ = std::make_unique<FakeInputDeviceSettingsController>();
    provider_ = std::make_unique<InputDeviceSettingsProvider>();
    provider_->SetWidgetForTesting(widget_.get());
    keyboard_brightness_control_delegate_ =
        std::make_unique<FakeKeyboardBrightnessControlDelegate>();
    provider_->SetKeyboardBrightnessControlDelegateForTesting(
        keyboard_brightness_control_delegate_.get());
    power_manager_client_ =
        std::make_unique<chromeos::FakePowerManagerClient>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    provider_.reset();
    controller_.reset();
    keyboard_brightness_control_delegate_.reset();
    power_manager_client_.reset();
    scoped_resetter_.reset();
    widget_.reset();
    views::ViewsTestBase::TearDown();
    feature_list_.reset();
    histogram_tester_.reset();
  }

 protected:
  std::unique_ptr<FakeInputDeviceSettingsController> controller_;
  std::unique_ptr<InputDeviceSettingsProvider> provider_;
  std::unique_ptr<FakeKeyboardBrightnessControlDelegate>
      keyboard_brightness_control_delegate_;
  std::unique_ptr<chromeos::FakePowerManagerClient> power_manager_client_;
  std::unique_ptr<base::test::ScopedFeatureList> feature_list_;
  std::unique_ptr<InputDeviceSettingsController::ScopedResetterForTest>
      scoped_resetter_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  const ::ash::mojom::Keyboard keyboard1_ =
      ::ash::mojom::Keyboard(/*name=*/"AT Translated Set 2",
                             /*is_external=*/false,
                             /*id=*/1,
                             /*device_key=*/"fake-device-key1",
                             /*meta_key=*/::ui::mojom::MetaKey::kLauncher,
                             /*modifier_keys=*/{},
                             /*top_row_action_keys=*/{},
                             ::ash::mojom::KeyboardSettings::New(),
                             ::ash::mojom::BatteryInfo::New(),
                             ::ash::mojom::CompanionAppInfo::New());
  const ::ash::mojom::Keyboard keyboard2_ =
      ::ash::mojom::Keyboard(/*name=*/"Logitech K580",
                             /*is_external=*/true,
                             /*id=*/2,
                             /*device_key=*/"fake-device-key2",
                             /*meta_key=*/::ui::mojom::MetaKey::kExternalMeta,
                             /*modifier_keys=*/{},
                             /*top_row_action_keys=*/{},
                             ::ash::mojom::KeyboardSettings::New(),
                             ::ash::mojom::BatteryInfo::New(),
                             ::ash::mojom::CompanionAppInfo::New());
  const ::ash::mojom::Keyboard keyboard3_ =
      ::ash::mojom::Keyboard(/*name=*/"HP 910 White Bluetooth Keyboard",
                             /*is_external=*/true,
                             /*id=*/3,
                             /*device_key=*/"fake-device-key3",
                             /*meta_key=*/::ui::mojom::MetaKey::kExternalMeta,
                             /*modifier_keys=*/{},
                             /*top_row_action_keys=*/{},
                             ::ash::mojom::KeyboardSettings::New(),
                             ::ash::mojom::BatteryInfo::New(),
                             ::ash::mojom::CompanionAppInfo::New());
  const ::ash::mojom::Touchpad touchpad1_ =
      ::ash::mojom::Touchpad(/*name=*/"test touchpad",
                             /*is_external=*/false,
                             /*id=*/3,
                             /*device_key=*/"fake-device-key3",
                             /*is_haptic=*/true,
                             ::ash::mojom::TouchpadSettings::New(),
                             ::ash::mojom::BatteryInfo::New(),
                             ::ash::mojom::CompanionAppInfo::New());
  const ::ash::mojom::Touchpad touchpad2_ =
      ::ash::mojom::Touchpad(/*name=*/"Logitech T650",
                             /*is_external=*/true,
                             /*id=*/4,
                             /*device_key=*/"fake-device-key4",
                             /*is_haptic=*/false,
                             ::ash::mojom::TouchpadSettings::New(),
                             ::ash::mojom::BatteryInfo::New(),
                             ::ash::mojom::CompanionAppInfo::New());
  const ::ash::mojom::PointingStick pointing_stick1_ =
      ::ash::mojom::PointingStick(/*name=*/"test pointing stick",
                                  /*is_external=*/false,
                                  /*id=*/5,
                                  /*device_key=*/"fake-device-key5",
                                  ::ash::mojom::PointingStickSettings::New());
  const ::ash::mojom::PointingStick pointing_stick2_ =
      ::ash::mojom::PointingStick(/*name=*/"Lexmark-Unicomp FSR",
                                  /*is_external=*/true,
                                  /*id=*/6,
                                  /*device_key=*/"fake-device-key6",
                                  ::ash::mojom::PointingStickSettings::New());
  const ::ash::mojom::Mouse mouse1_ = ::ash::mojom::Mouse(
      /*name=*/"Razer Basilisk V3",
      /*is_external=*/false,
      /*id=*/7,
      /*device_key=*/"fake-device-key7",
      /*customization_restriction=*/
      ::ash::mojom::CustomizationRestriction::kAllowCustomizations,
      /*mouse_button_config=*/::ash::mojom::MouseButtonConfig::kNoConfig,
      ::ash::mojom::MouseSettings::New(),
      ::ash::mojom::BatteryInfo::New(),
      ::ash::mojom::CompanionAppInfo::New());
  const ::ash::mojom::Mouse mouse2_ = ::ash::mojom::Mouse(
      /*name=*/"MX Anywhere 2S",
      /*is_external=*/true,
      /*id=*/8,
      /*device_key=*/"fake-device-key8",
      /*customization_restriction=*/
      ::ash::mojom::CustomizationRestriction::kAllowCustomizations,
      /*mouse_button_config=*/::ash::mojom::MouseButtonConfig::kNoConfig,
      ::ash::mojom::MouseSettings::New(),
      ::ash::mojom::BatteryInfo::New(),
      ::ash::mojom::CompanionAppInfo::New());
  const ::ash::mojom::GraphicsTablet graphics_tablet1_ =
      ::ash::mojom::GraphicsTablet(
          /*name=*/"Wacom Intuos S",
          /*id=*/9,
          /*device_key=*/"fake-device-key9",
          /*customization_restriction=*/
          ::ash::mojom::CustomizationRestriction::kAllowCustomizations,
          ::ash::mojom::GraphicsTabletButtonConfig::kNoConfig,
          ::ash::mojom::GraphicsTabletSettings::New(),
          ::ash::mojom::BatteryInfo::New(),
          ::ash::mojom::CompanionAppInfo::New());
  const ::ash::mojom::GraphicsTablet graphics_tablet2_ =
      ::ash::mojom::GraphicsTablet(
          /*name=*/"Huion H1060P",
          /*id=*/10,
          /*device_key=*/"fake-device-key10",
          /*customization_restriction=*/
          ::ash::mojom::CustomizationRestriction::kAllowCustomizations,
          ::ash::mojom::GraphicsTabletButtonConfig::kNoConfig,
          ::ash::mojom::GraphicsTabletSettings::New(),
          ::ash::mojom::BatteryInfo::New(),
          ::ash::mojom::CompanionAppInfo::New());
};

TEST_F(InputDeviceSettingsProviderTest, TestSetKeyboardSettings) {
  controller_->AddKeyboard(keyboard1_.Clone());
  controller_->AddKeyboard(keyboard2_.Clone());

  FakeKeyboardSettingsObserver fake_observer;
  provider_->ObserveKeyboardSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  EXPECT_CALL(*controller_, SetKeyboardSettings(keyboard1_.id, testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  provider_->SetKeyboardSettings(keyboard1_.id, keyboard1_.settings->Clone());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_keyboard_list_updated());

  EXPECT_CALL(*controller_, SetKeyboardSettings(keyboard2_.id, testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  provider_->SetKeyboardSettings(keyboard2_.id, keyboard1_.settings->Clone());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_keyboard_list_updated());

  EXPECT_CALL(*controller_, SetKeyboardSettings(keyboard2_.id, testing::_))
      .Times(1)
      .WillOnce(testing::Return(false));
  provider_->SetKeyboardSettings(keyboard2_.id, keyboard1_.settings->Clone());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_observer.num_times_keyboard_list_updated());
}

TEST_F(InputDeviceSettingsProviderTest, TestRestoreDefaultKeyboardRemappings) {
  controller_->AddKeyboard(keyboard1_.Clone());
  provider_->RestoreDefaultKeyboardRemappings(keyboard1_.id);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      1, controller_->num_times_restore_default_keyboard_remappings_called());

  controller_->AddKeyboard(keyboard2_.Clone());
  provider_->RestoreDefaultKeyboardRemappings(keyboard2_.id);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      2, controller_->num_times_restore_default_keyboard_remappings_called());
}

TEST_F(InputDeviceSettingsProviderTest, TestSetPointingStickSettings) {
  controller_->AddPointingStick(pointing_stick1_.Clone());
  controller_->AddPointingStick(pointing_stick2_.Clone());

  FakePointingStickSettingsObserver fake_observer;
  provider_->ObservePointingStickSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  EXPECT_CALL(*controller_,
              SetPointingStickSettings(pointing_stick1_.id, testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  provider_->SetPointingStickSettings(pointing_stick1_.id,
                                      pointing_stick1_.settings->Clone());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_called());

  EXPECT_CALL(*controller_,
              SetPointingStickSettings(pointing_stick2_.id, testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  provider_->SetPointingStickSettings(pointing_stick2_.id,
                                      pointing_stick1_.settings->Clone());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_called());

  EXPECT_CALL(*controller_,
              SetPointingStickSettings(pointing_stick2_.id, testing::_))
      .Times(1)
      .WillOnce(testing::Return(false));
  provider_->SetPointingStickSettings(pointing_stick2_.id,
                                      pointing_stick1_.settings->Clone());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_observer.num_times_called());
}

TEST_F(InputDeviceSettingsProviderTest, TestSetMouseSettings) {
  controller_->AddMouse(mouse1_.Clone());
  controller_->AddMouse(mouse2_.Clone());

  FakeMouseSettingsObserver fake_observer;
  provider_->ObserveMouseSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  EXPECT_CALL(*controller_, SetMouseSettings(mouse1_.id, testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  provider_->SetMouseSettings(mouse1_.id, mouse1_.settings->Clone());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_mouse_list_updated());

  EXPECT_CALL(*controller_, SetMouseSettings(mouse2_.id, testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  provider_->SetMouseSettings(mouse2_.id, mouse1_.settings->Clone());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_mouse_list_updated());

  EXPECT_CALL(*controller_, SetMouseSettings(mouse2_.id, testing::_))
      .Times(1)
      .WillOnce(testing::Return(false));
  provider_->SetMouseSettings(mouse2_.id, mouse1_.settings->Clone());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_observer.num_times_mouse_list_updated());
}

TEST_F(InputDeviceSettingsProviderTest, TestSetTouchpadSettings) {
  controller_->AddTouchpad(touchpad1_.Clone());
  controller_->AddTouchpad(touchpad2_.Clone());

  FakeTouchpadSettingsObserver fake_observer;
  provider_->ObserveTouchpadSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  EXPECT_CALL(*controller_, SetTouchpadSettings(touchpad1_.id, testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  provider_->SetTouchpadSettings(touchpad1_.id, touchpad1_.settings->Clone());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_called());

  EXPECT_CALL(*controller_, SetTouchpadSettings(touchpad2_.id, testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  provider_->SetTouchpadSettings(touchpad2_.id, touchpad1_.settings->Clone());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_called());

  EXPECT_CALL(*controller_, SetTouchpadSettings(touchpad2_.id, testing::_))
      .Times(1)
      .WillOnce(testing::Return(false));
  provider_->SetTouchpadSettings(touchpad2_.id, touchpad1_.settings->Clone());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_observer.num_times_called());
}

TEST_F(InputDeviceSettingsProviderTest, TestSetGraphicsTabletSettings) {
  controller_->AddGraphicsTablet(graphics_tablet1_.Clone());
  controller_->AddGraphicsTablet(graphics_tablet2_.Clone());

  FakeGraphicsTabletSettingsObserver fake_observer;
  provider_->ObserveGraphicsTabletSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  EXPECT_CALL(*controller_,
              SetGraphicsTabletSettings(graphics_tablet1_.id, testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  provider_->SetGraphicsTabletSettings(graphics_tablet1_.id,
                                       graphics_tablet1_.settings->Clone());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_graphics_tablet_list_updated());

  EXPECT_CALL(*controller_,
              SetGraphicsTabletSettings(graphics_tablet2_.id, testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  provider_->SetGraphicsTabletSettings(graphics_tablet2_.id,
                                       graphics_tablet2_.settings->Clone());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_graphics_tablet_list_updated());

  EXPECT_CALL(*controller_,
              SetGraphicsTabletSettings(graphics_tablet2_.id, testing::_))
      .Times(1)
      .WillOnce(testing::Return(false));
  provider_->SetGraphicsTabletSettings(graphics_tablet2_.id,
                                       graphics_tablet2_.settings->Clone());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_observer.num_times_graphics_tablet_list_updated());
}

TEST_F(InputDeviceSettingsProviderTest, TestKeyboardSettingsObeserver) {
  std::vector<::ash::mojom::KeyboardPtr> expected_keyboards;
  expected_keyboards.push_back(keyboard1_.Clone());
  controller_->AddKeyboard(keyboard1_.Clone());

  FakeKeyboardSettingsObserver fake_observer;
  provider_->ObserveKeyboardSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_keyboard_list_updated());
  EXPECT_EQ(1, fake_observer.num_times_keyboard_policies_updated());
  ExpectListsEqual(expected_keyboards, fake_observer.keyboards());

  expected_keyboards.push_back(keyboard2_.Clone());
  controller_->AddKeyboard(keyboard2_.Clone());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_observer.num_times_keyboard_list_updated());
  EXPECT_EQ(1, fake_observer.num_times_keyboard_policies_updated());
  ExpectListsEqual(expected_keyboards, fake_observer.keyboards());

  expected_keyboards.pop_back();
  controller_->RemoveKeyboard(keyboard2_.id);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, fake_observer.num_times_keyboard_list_updated());
  EXPECT_EQ(1, fake_observer.num_times_keyboard_policies_updated());
  ExpectListsEqual(expected_keyboards, fake_observer.keyboards());
}

TEST_F(InputDeviceSettingsProviderTest,
       TestKeyboardSettingsObeserverPolicyUpdates) {
  controller_->AddKeyboard(keyboard1_.Clone());

  FakeKeyboardSettingsObserver fake_observer;
  provider_->ObserveKeyboardSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_keyboard_list_updated());
  EXPECT_EQ(1, fake_observer.num_times_keyboard_policies_updated());

  controller_->SetKeyboardPolicies(::ash::mojom::KeyboardPolicies::New());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_keyboard_list_updated());
  EXPECT_EQ(2, fake_observer.num_times_keyboard_policies_updated());
}

TEST_F(InputDeviceSettingsProviderTest, TestDuplicatesRemoved) {
  std::vector<::ash::mojom::KeyboardPtr> expected_keyboards;

  auto keyboard1 = keyboard1_.Clone();
  keyboard1->device_key = "test-key1";
  expected_keyboards.push_back(keyboard1.Clone());
  controller_->AddKeyboard(keyboard1.Clone());

  FakeKeyboardSettingsObserver fake_observer;
  provider_->ObserveKeyboardSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_keyboard_list_updated());
  EXPECT_EQ(1, fake_observer.num_times_keyboard_policies_updated());
  ExpectListsEqual</*sorted=*/true>(expected_keyboards,
                                    fake_observer.keyboards());

  auto keyboard2 = keyboard2_.Clone();
  keyboard2->device_key = "test-key1";
  controller_->AddKeyboard(keyboard2.Clone());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_observer.num_times_keyboard_list_updated());
  EXPECT_EQ(1, fake_observer.num_times_keyboard_policies_updated());
  ExpectListsEqual</*sorted=*/true>(expected_keyboards,
                                    fake_observer.keyboards());

  controller_->RemoveKeyboard(keyboard2_.id);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, fake_observer.num_times_keyboard_list_updated());
  EXPECT_EQ(1, fake_observer.num_times_keyboard_policies_updated());
  ExpectListsEqual</*sorted=*/true>(expected_keyboards,
                                    fake_observer.keyboards());
}

TEST_F(InputDeviceSettingsProviderTest, TestSortingExternalFirst) {
  std::vector<::ash::mojom::KeyboardPtr> expected_keyboards;

  auto keyboard1 = keyboard1_.Clone();
  auto keyboard2 = keyboard2_.Clone();

  // Guarantee that keyboard 1 which is internal, has a higher id than keyboard
  // 2 to properly test that external devices always come first in the list.
  keyboard1->id = 2;
  keyboard2->id = 1;
  ASSERT_FALSE(keyboard1->is_external);
  ASSERT_TRUE(keyboard2->is_external);

  controller_->AddKeyboard(keyboard1->Clone());
  controller_->AddKeyboard(keyboard2->Clone());
  expected_keyboards.push_back(keyboard2->Clone());
  expected_keyboards.push_back(keyboard1->Clone());

  FakeKeyboardSettingsObserver fake_observer;
  provider_->ObserveKeyboardSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  base::RunLoop().RunUntilIdle();
  ExpectListsEqual</*sorted=*/true>(expected_keyboards,
                                    fake_observer.keyboards());
}

TEST_F(InputDeviceSettingsProviderTest, TestSortingExternalFirstThenById) {
  std::vector<::ash::mojom::KeyboardPtr> expected_keyboards;

  controller_->AddKeyboard(keyboard1_.Clone());
  ASSERT_FALSE(keyboard1_.is_external);

  controller_->AddKeyboard(keyboard2_.Clone());
  ASSERT_TRUE(keyboard2_.is_external);

  controller_->AddKeyboard(keyboard3_.Clone());
  ASSERT_TRUE(keyboard3_.is_external);
  ASSERT_LT(keyboard2_.id, keyboard3_.id);

  expected_keyboards.push_back(keyboard3_.Clone());
  expected_keyboards.push_back(keyboard2_.Clone());
  expected_keyboards.push_back(keyboard1_.Clone());

  FakeKeyboardSettingsObserver fake_observer;
  provider_->ObserveKeyboardSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  base::RunLoop().RunUntilIdle();
  ExpectListsEqual</*sorted=*/true>(expected_keyboards,
                                    fake_observer.keyboards());
}

TEST_F(InputDeviceSettingsProviderTest, TestMouseSettingsObeserver) {
  std::vector<::ash::mojom::MousePtr> expected_mice;
  expected_mice.push_back(mouse1_.Clone());
  controller_->AddMouse(mouse1_.Clone());

  FakeMouseSettingsObserver fake_observer;
  provider_->ObserveMouseSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_mouse_list_updated());
  EXPECT_EQ(1, fake_observer.num_times_mouse_policies_updated());
  ExpectListsEqual(expected_mice, fake_observer.mice());

  expected_mice.push_back(mouse2_.Clone());
  controller_->AddMouse(mouse2_.Clone());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_observer.num_times_mouse_list_updated());
  EXPECT_EQ(1, fake_observer.num_times_mouse_policies_updated());
  ExpectListsEqual(expected_mice, fake_observer.mice());

  expected_mice.pop_back();
  controller_->RemoveMouse(mouse2_.id);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, fake_observer.num_times_mouse_list_updated());
  EXPECT_EQ(1, fake_observer.num_times_mouse_policies_updated());
  ExpectListsEqual(expected_mice, fake_observer.mice());
}

TEST_F(InputDeviceSettingsProviderTest,
       TestMouseSettingsObeserverPolicyUpdates) {
  controller_->AddMouse(mouse1_.Clone());

  FakeMouseSettingsObserver fake_observer;
  provider_->ObserveMouseSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_mouse_list_updated());
  EXPECT_EQ(1, fake_observer.num_times_mouse_policies_updated());

  controller_->SetMousePolicies(::ash::mojom::MousePolicies::New());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_mouse_list_updated());
  EXPECT_EQ(2, fake_observer.num_times_mouse_policies_updated());
}

TEST_F(InputDeviceSettingsProviderTest, TestTouchpadSettingsObeserver) {
  std::vector<::ash::mojom::TouchpadPtr> expected_touchpads;
  expected_touchpads.push_back(touchpad1_.Clone());
  controller_->AddTouchpad(touchpad1_.Clone());

  FakeTouchpadSettingsObserver fake_observer;
  provider_->ObserveTouchpadSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_called());
  ExpectListsEqual(expected_touchpads, fake_observer.touchpads());

  expected_touchpads.push_back(touchpad2_.Clone());
  controller_->AddTouchpad(touchpad2_.Clone());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_observer.num_times_called());
  ExpectListsEqual(expected_touchpads, fake_observer.touchpads());

  expected_touchpads.pop_back();
  controller_->RemoveTouchpad(touchpad2_.id);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, fake_observer.num_times_called());
  ExpectListsEqual(expected_touchpads, fake_observer.touchpads());
}

TEST_F(InputDeviceSettingsProviderTest, TestPointingStickSettingsObeserver) {
  std::vector<::ash::mojom::PointingStickPtr> expected_pointing_sticks;
  expected_pointing_sticks.push_back(pointing_stick1_.Clone());
  controller_->AddPointingStick(pointing_stick1_.Clone());

  FakePointingStickSettingsObserver fake_observer;
  provider_->ObservePointingStickSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_called());
  ExpectListsEqual(expected_pointing_sticks, fake_observer.pointing_sticks());

  expected_pointing_sticks.push_back(pointing_stick2_.Clone());
  controller_->AddPointingStick(pointing_stick2_.Clone());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_observer.num_times_called());
  ExpectListsEqual(expected_pointing_sticks, fake_observer.pointing_sticks());

  expected_pointing_sticks.pop_back();
  controller_->RemovePointingStick(pointing_stick2_.id);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, fake_observer.num_times_called());
  ExpectListsEqual(expected_pointing_sticks, fake_observer.pointing_sticks());
}

TEST_F(InputDeviceSettingsProviderTest, TestGraphicsTabletSettingsObeserver) {
  std::vector<::ash::mojom::GraphicsTabletPtr> expected_graphics_tablets;
  expected_graphics_tablets.push_back(graphics_tablet1_.Clone());
  controller_->AddGraphicsTablet(graphics_tablet1_.Clone());

  FakeGraphicsTabletSettingsObserver fake_observer;
  provider_->ObserveGraphicsTabletSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_graphics_tablet_list_updated());
  ExpectListsEqual(expected_graphics_tablets, fake_observer.graphics_tablets());

  expected_graphics_tablets.push_back(graphics_tablet2_.Clone());
  controller_->AddGraphicsTablet(graphics_tablet2_.Clone());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_observer.num_times_graphics_tablet_list_updated());
  ExpectListsEqual(expected_graphics_tablets, fake_observer.graphics_tablets());

  expected_graphics_tablets.pop_back();
  controller_->RemoveGraphicsTablet(graphics_tablet2_.id);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, fake_observer.num_times_graphics_tablet_list_updated());
  ExpectListsEqual(expected_graphics_tablets, fake_observer.graphics_tablets());
}

TEST_F(InputDeviceSettingsProviderTest, ObservationMatchesWidget) {
  provider_->StartObserving(mouse1_.id);
  EXPECT_TRUE(controller_->observed_currently());

  widget_->Hide();
  EXPECT_FALSE(controller_->observed_currently());

  widget_->Show();
  EXPECT_TRUE(controller_->observed_currently());

  controller_->StopObservingButtons();
  EXPECT_FALSE(controller_->observed_currently());
}

TEST_F(InputDeviceSettingsProviderTest, ObservationStateRemembered) {
  provider_->StartObserving(mouse1_.id);
  EXPECT_TRUE(controller_->observed_currently());

  widget_->Hide();
  EXPECT_FALSE(controller_->observed_currently());

  provider_->StopObserving();
  EXPECT_FALSE(controller_->observed_currently());

  widget_->Show();
  EXPECT_FALSE(controller_->observed_currently());

  provider_->StartObserving(mouse1_.id);
  EXPECT_TRUE(controller_->observed_currently());
}

TEST_F(InputDeviceSettingsProviderTest, ObservationStateOnDestruction) {
  provider_->StartObserving(mouse1_.id);
  EXPECT_TRUE(controller_->observed_currently());

  widget_.reset();
  EXPECT_FALSE(controller_->observed_currently());
}

TEST_F(InputDeviceSettingsProviderTest, ButtonPressObserverTest) {
  FakeButtonPressObserver fake_observer;
  provider_->ObserveButtonPresses(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  ::ash::mojom::ButtonPtr expected_button =
      ::ash::mojom::Button::NewCustomizableButton(
          ::ash::mojom::CustomizableButton::kMiddle);
  provider_->OnCustomizableMouseButtonPressed(mouse1_, *expected_button);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(*expected_button, fake_observer.last_pressed_button());

  expected_button = ::ash::mojom::Button::NewCustomizableButton(
      ::ash::mojom::CustomizableButton::kForward);
  provider_->OnCustomizablePenButtonPressed(graphics_tablet1_,
                                            *expected_button);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(*expected_button, fake_observer.last_pressed_button());

  expected_button = ::ash::mojom::Button::NewVkey(ui::VKEY_0);
  provider_->OnCustomizablePenButtonPressed(graphics_tablet1_,
                                            *expected_button);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(*expected_button, fake_observer.last_pressed_button());
}

TEST_F(InputDeviceSettingsProviderTest, KeyboardBrightnessObserverTest) {
  FakeKeyboardBrightnessObserver fake_observer;
  EXPECT_EQ(0, fake_observer.num_times_called());

  // Set initial brightness to 40.0.
  double initial_brightness = 40.0;
  keyboard_brightness_control_delegate_->HandleSetKeyboardBrightness(
      initial_brightness, /*gradual=*/false,
      KeyboardBrightnessChangeSource::kSettingsApp);
  EXPECT_EQ(KeyboardBrightnessChangeSource::kSettingsApp,
            keyboard_brightness_control_delegate_
                ->keyboard_brightness_change_source());

  provider_->ObserveKeyboardBrightness(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // OnKeyboardBrightnessChange is called when observer is registered.
  EXPECT_EQ(1, fake_observer.num_times_called());

  double expected_brightness = 66.6;

  power_manager::BacklightBrightnessChange brightness_change;
  brightness_change.set_percent(expected_brightness);
  brightness_change.set_cause(
      power_manager::BacklightBrightnessChange_Cause_USER_REQUEST);
  provider_->KeyboardBrightnessChanged(brightness_change);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(expected_brightness, fake_observer.keyboard_brightness());
  EXPECT_EQ(2, fake_observer.num_times_called());
}

TEST_F(InputDeviceSettingsProviderTest, LidStateObserverTest) {
  FakeLidStateObserver fake_observer;
  base::test::TestFuture<bool> future;

  // Attach a lid state observer.
  provider_->ObserveLidState(fake_observer.receiver.BindNewPipeAndPassRemote(),
                             future.GetCallback());
  base::RunLoop().RunUntilIdle();

  // Open the lid.
  provider_->LidEventReceived(chromeos::PowerManagerClient::LidState::OPEN,
                              /*timestamp=*/{});
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(fake_observer.is_lid_open());
  EXPECT_EQ(1, fake_observer.num_lid_state_change_calls());

  // Close the lid.
  provider_->LidEventReceived(chromeos::PowerManagerClient::LidState::CLOSED,
                              /*timestamp=*/{});
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(fake_observer.is_lid_open());
  EXPECT_EQ(2, fake_observer.num_lid_state_change_calls());
}

TEST_F(InputDeviceSettingsProviderTest,
       KeyboardAmbientLightSensorObserverTest) {
  provider_->SetKeyboardBrightnessControlDelegateForTesting(
      keyboard_brightness_control_delegate_.get());
  FakeKeyboardAmbientLightSensorObserver fake_observer;
  EXPECT_EQ(0, fake_observer.num_times_called());

  // Start observing the keyboard ambient light sensor
  provider_->ObserveKeyboardAmbientLightSensor(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // OnKeyboardAmbientLightSensorEnabledChange is called to set initial value
  // when observer is registered.
  EXPECT_EQ(1, fake_observer.num_times_called());

  // Enable the keyboard ambient light sensor
  {
    bool keyboard_ambient_light_sensor_enabled = true;
    power_manager::AmbientLightSensorChange change;
    change.set_cause(
        power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST);
    change.set_sensor_enabled(keyboard_ambient_light_sensor_enabled);
    provider_->KeyboardAmbientLightSensorEnabledChanged(change);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(keyboard_ambient_light_sensor_enabled,
              fake_observer.keyboard_ambient_light_sensor_enabled());
    EXPECT_EQ(2, fake_observer.num_times_called());
  }

  // Disable the keyboard ambient light sensor
  {
    bool keyboard_ambient_light_sensor_enabled = false;
    power_manager::AmbientLightSensorChange change;
    change.set_cause(
        power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST);
    change.set_sensor_enabled(keyboard_ambient_light_sensor_enabled);
    provider_->KeyboardAmbientLightSensorEnabledChanged(change);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(keyboard_ambient_light_sensor_enabled,
              fake_observer.keyboard_ambient_light_sensor_enabled());
    EXPECT_EQ(3, fake_observer.num_times_called());
  }
}

TEST_F(InputDeviceSettingsProviderTest, SetKeyboardBrightness) {
  double adjustedBrightness = 60.9;
  provider_->SetKeyboardBrightness(adjustedBrightness);
  EXPECT_EQ(adjustedBrightness,
            keyboard_brightness_control_delegate_->keyboard_brightness());
  // When user change keyboard brightness from settings(using provider), the
  // change source should be kSettingsApp.
  EXPECT_EQ(KeyboardBrightnessChangeSource::kSettingsApp,
            keyboard_brightness_control_delegate_
                ->keyboard_brightness_change_source());

  adjustedBrightness = 20.3;
  provider_->SetKeyboardBrightness(adjustedBrightness);
  EXPECT_EQ(adjustedBrightness,
            keyboard_brightness_control_delegate_->keyboard_brightness());
}

TEST_F(InputDeviceSettingsProviderTest, SetKeyboardAmbientLightSensorEnabled) {
  // Verify initial state is enabled.
  EXPECT_TRUE(keyboard_brightness_control_delegate_
                  ->keyboard_ambient_light_sensor_enabled());

  // Disable the ambient light sensor.
  provider_->SetKeyboardAmbientLightSensorEnabled(false);
  EXPECT_FALSE(keyboard_brightness_control_delegate_
                   ->keyboard_ambient_light_sensor_enabled());

  // Re-enable the ambient light sensor and verify.
  provider_->SetKeyboardAmbientLightSensorEnabled(true);
  EXPECT_TRUE(keyboard_brightness_control_delegate_
                  ->keyboard_ambient_light_sensor_enabled());
}

TEST_F(InputDeviceSettingsProviderTest, ButtonPressObserverFollowsWindowFocus) {
  FakeButtonPressObserver fake_observer;
  provider_->ObserveButtonPresses(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  widget_->Hide();

  ::ash::mojom::ButtonPtr expected_button =
      ::ash::mojom::Button::NewCustomizableButton(
          ::ash::mojom::CustomizableButton::kMiddle);

  provider_->OnCustomizableMouseButtonPressed(mouse1_, *expected_button);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_observer.has_last_pressed_button());

  provider_->OnCustomizablePenButtonPressed(graphics_tablet1_,
                                            *expected_button);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_observer.has_last_pressed_button());

  provider_->OnCustomizableTabletButtonPressed(graphics_tablet1_,
                                               *expected_button);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_observer.has_last_pressed_button());

  widget_->Show();
  provider_->OnCustomizableMouseButtonPressed(mouse1_, *expected_button);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(*expected_button, fake_observer.last_pressed_button());
}

TEST_F(InputDeviceSettingsProviderTest, HasKeyboardBacklight) {
  base::test::TestFuture<bool> future;

  power_manager_client_->set_has_keyboard_backlight(true);
  provider_->HasKeyboardBacklight(future.GetCallback());
  EXPECT_TRUE(future.Get<0>());

  future.Clear();
  power_manager_client_->set_has_keyboard_backlight(false);
  provider_->HasKeyboardBacklight(future.GetCallback());
  EXPECT_FALSE(future.Get<0>());
}

TEST_F(InputDeviceSettingsProviderTest, HasAmbientLightSensor) {
  base::test::TestFuture<bool> future;

  power_manager_client_->set_has_ambient_light_sensor(true);
  provider_->HasAmbientLightSensor(future.GetCallback());
  EXPECT_TRUE(future.Get<0>());

  future.Clear();
  power_manager_client_->set_has_ambient_light_sensor(false);
  provider_->HasAmbientLightSensor(future.GetCallback());
  EXPECT_FALSE(future.Get<0>());
}

TEST_F(InputDeviceSettingsProviderTest, RecordKeyboardColorLinkClicked) {
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.ColorLinkClicked", 0);
  provider_->RecordKeyboardColorLinkClicked();
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.ColorLinkClicked", 1);
}

TEST_F(InputDeviceSettingsProviderTest,
       RecordKeyboardBrightnessChangeFromSlider) {
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.BrightnessSliderAdjusted", 0);
  provider_->RecordKeyboardBrightnessChangeFromSlider(40.0);
  base::RunLoop().RunUntilIdle();
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.BrightnessSliderAdjusted", 1);
}

TEST_F(InputDeviceSettingsProviderTest,
       RecordSetKeyboardAutoBrightnessEnabled) {
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.AutoBrightnessEnabled.Changed", 0);

  provider_->SetKeyboardAmbientLightSensorEnabled(true);
  base::RunLoop().RunUntilIdle();

  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.AutoBrightnessEnabled.Changed", 1);
  histogram_tester_->ExpectBucketCount(
      "ChromeOS.Settings.Device.Keyboard.AutoBrightnessEnabled.Changed",
      /*sample=*/true, /*expected_count=*/1);

  provider_->SetKeyboardAmbientLightSensorEnabled(false);
  base::RunLoop().RunUntilIdle();

  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.AutoBrightnessEnabled.Changed", 2);
  histogram_tester_->ExpectBucketCount(
      "ChromeOS.Settings.Device.Keyboard.AutoBrightnessEnabled.Changed",
      /*sample=*/true, /*expected_count=*/1);
  histogram_tester_->ExpectBucketCount(
      "ChromeOS.Settings.Device.Keyboard.AutoBrightnessEnabled.Changed",
      /*sample=*/false, /*expected_count=*/1);
}

TEST_F(InputDeviceSettingsProviderTest,
       RecordKeyboardAmbientLightSensorDisabledCause) {
  // No histograms should have been recorded yet.
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Settings.Keyboard.UserInitiated."
      "AmbientLightSensorDisabledCause",
      /*expected_count=*/0);

  // Verify histogram recording when ALS is disabled via settings app.
  {
    power_manager::AmbientLightSensorChange cause_settings_app;
    cause_settings_app.set_sensor_enabled(false);
    cause_settings_app.set_cause(
        power_manager::
            AmbientLightSensorChange_Cause_USER_REQUEST_SETTINGS_APP);
    provider_->KeyboardAmbientLightSensorEnabledChanged(cause_settings_app);
    histogram_tester_->ExpectUniqueSample(
        "ChromeOS.Settings.Keyboard.UserInitiated."
        "AmbientLightSensorDisabledCause",
        KeyboardAmbientLightSensorDisabledCause::kUserRequestSettingsApp, 1);
  }

  // Ensure enabling ALS does not emit histogram.
  {
    power_manager::AmbientLightSensorChange cause_settings_app;
    cause_settings_app.set_sensor_enabled(true);
    cause_settings_app.set_cause(
        power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST);
    provider_->KeyboardAmbientLightSensorEnabledChanged(cause_settings_app);
    histogram_tester_->ExpectTotalCount(
        "ChromeOS.Settings.Keyboard.UserInitiated."
        "AmbientLightSensorDisabledCause",
        /*expected_count=*/1);
  }

  // Test histogram update when ALS is disabled due to brightness change.
  {
    power_manager::AmbientLightSensorChange cause_user_request;
    cause_user_request.set_sensor_enabled(false);
    cause_user_request.set_cause(
        power_manager::AmbientLightSensorChange_Cause_BRIGHTNESS_USER_REQUEST);
    provider_->KeyboardAmbientLightSensorEnabledChanged(cause_user_request);
    histogram_tester_->ExpectBucketCount(
        "ChromeOS.Settings.Keyboard.UserInitiated."
        "AmbientLightSensorDisabledCause",
        KeyboardAmbientLightSensorDisabledCause::kBrightnessUserRequest, 1);
  }
}

}  // namespace ash::settings
