// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/input_device_settings/input_device_settings_provider.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/webui/settings/ash/input_device_settings/input_device_settings_provider.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {
namespace {
const ::ash::mojom::Keyboard kKeyboard1 =
    ::ash::mojom::Keyboard(/*name=*/"AT Translated Set 2",
                           /*is_external=*/false,
                           /*id=*/0,
                           /*device_key=*/"fake-device-key1",
                           /*meta_key=*/::ash::mojom::MetaKey::kLauncher,
                           /*modifier_keys=*/{},
                           ::ash::mojom::KeyboardSettings::New());
const ::ash::mojom::Keyboard kKeyboard2 =
    ::ash::mojom::Keyboard(/*name=*/"Logitech K580",
                           /*is_external=*/true,
                           /*id=*/1,
                           /*device_key=*/"fake-device-key2",
                           /*meta_key=*/::ash::mojom::MetaKey::kExternalMeta,
                           /*modifier_keys=*/{},
                           ::ash::mojom::KeyboardSettings::New());
const ::ash::mojom::Touchpad kTouchpad1 =
    ::ash::mojom::Touchpad(/*name=*/"test touchpad",
                           /*id=*/2,
                           /*device_key=*/"fake-device-key3",
                           ::ash::mojom::TouchpadSettings::New());
const ::ash::mojom::Touchpad kTouchpad2 =
    ::ash::mojom::Touchpad(/*name=*/"Logitech T650",
                           /*id=*/3,
                           /*device_key=*/"fake-device-key4",
                           ::ash::mojom::TouchpadSettings::New());
template <typename T>
void ExpectListsEqual(const std::vector<T>& expected_list,
                      const std::vector<T>& actual_list) {
  ASSERT_EQ(expected_list.size(), actual_list.size());
  for (size_t i = 0; i < actual_list.size(); i++) {
    EXPECT_EQ(expected_list[i], actual_list[i]);
  }
}

template <typename T>
void ExpectListsEqualByValue(std::vector<T> expected_list,
                             std::vector<T> actual_list) {
  ExpectListsEqual(expected_list, actual_list);
}

template <typename T>
std::vector<T> CloneMojomVector(const std::vector<T>& devices) {
  std::vector<T> devices_copy;
  devices_copy.reserve(devices.size());
  for (const auto& device : devices) {
    devices_copy.push_back(device->Clone());
  }
  return devices_copy;
}

class FakeKeyboardSettingsObserver : public mojom::KeyboardSettingsObserver {
 public:
  void OnKeyboardListUpdated(
      std::vector<::ash::mojom::KeyboardPtr> keyboards) override {
    keyboards_ = std::move(keyboards);
    ++num_times_called_;
  }

  const std::vector<::ash::mojom::KeyboardPtr>& keyboards() {
    return keyboards_;
  }

  int num_times_called() { return num_times_called_; }
  mojo::Receiver<mojom::KeyboardSettingsObserver> receiver{this};

 private:
  std::vector<::ash::mojom::KeyboardPtr> keyboards_;
  int num_times_called_ = 0;
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

class FakeInputDeviceSettingsController : public InputDeviceSettingsController {
 public:
  // InputDeviceSettingsController:
  std::vector<::ash::mojom::KeyboardPtr> GetConnectedKeyboards() override {
    return CloneMojomVector(keyboards_);
  }
  std::vector<::ash::mojom::TouchpadPtr> GetConnectedTouchpads() override {
    return CloneMojomVector(touchpads_);
  }
  std::vector<::ash::mojom::MousePtr> GetConnectedMice() override {
    return CloneMojomVector(mice_);
  }
  std::vector<::ash::mojom::PointingStickPtr> GetConnectedPointingSticks()
      override {
    return CloneMojomVector(pointing_sticks_);
  }
  const ::ash::mojom::KeyboardSettings* GetKeyboardSettings(
      DeviceId id) override {
    return nullptr;
  }
  const ::ash::mojom::TouchpadSettings* GetTouchpadSettings(
      DeviceId id) override {
    return nullptr;
  }
  const ::ash::mojom::MouseSettings* GetMouseSettings(DeviceId id) override {
    return nullptr;
  }
  const ::ash::mojom::PointingStickSettings* GetPointingStickSettings(
      DeviceId id) override {
    return nullptr;
  }
  void SetKeyboardSettings(
      DeviceId id,
      ::ash::mojom::KeyboardSettingsPtr settings) override {}
  void AddObserver(Observer* observer) override { observer_ = observer; }
  void RemoveObserver(Observer* observer) override { observer_ = nullptr; }

  void AddKeyboard(::ash::mojom::KeyboardPtr keyboard) {
    keyboards_.push_back(std::move(keyboard));
    observer_->OnKeyboardConnected(*keyboards_.back());
  }
  void RemoveKeyboard(uint32_t device_id) {
    auto iter =
        base::ranges::find_if(keyboards_, [device_id](const auto& keyboard) {
          return keyboard->id == device_id;
        });
    if (iter == keyboards_.end()) {
      return;
    }
    auto temp_keyboard = std::move(*iter);
    keyboards_.erase(iter);
    observer_->OnKeyboardDisconnected(*temp_keyboard);
  }
  void AddMouse(::ash::mojom::MousePtr mouse) {
    mice_.push_back(std::move(mouse));
  }
  void RemoveMouse(uint32_t device_id) {
    base::EraseIf(mice_, [device_id](const auto& mouse) {
      return mouse->id == device_id;
    });
  }
  void AddTouchpad(::ash::mojom::TouchpadPtr touchpad) {
    touchpads_.push_back(std::move(touchpad));
    observer_->OnTouchpadConnected(*touchpads_.back());
  }
  void RemoveTouchpad(uint32_t device_id) {
    auto iter =
        base::ranges::find_if(touchpads_, [device_id](const auto& touchpad) {
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
  }
  void RemovePointingStick(uint32_t device_id) {
    base::EraseIf(pointing_sticks_, [device_id](const auto& pointing_stick) {
      return pointing_stick->id == device_id;
    });
  }

 private:
  std::vector<::ash::mojom::KeyboardPtr> keyboards_;
  std::vector<::ash::mojom::TouchpadPtr> touchpads_;
  std::vector<::ash::mojom::MousePtr> mice_;
  std::vector<::ash::mojom::PointingStickPtr> pointing_sticks_;
  raw_ptr<InputDeviceSettingsController::Observer> observer_ = nullptr;
};

}  // namespace

class InputDeviceSettingsProviderTest : public testing::Test {
 public:
  InputDeviceSettingsProviderTest() = default;
  ~InputDeviceSettingsProviderTest() override = default;

  void SetUp() override {
    feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    feature_list_->InitAndEnableFeature(features::kInputDeviceSettingsSplit);

    controller_ = std::make_unique<FakeInputDeviceSettingsController>();
    provider_ = std::make_unique<InputDeviceSettingsProvider>();
  }

  void TearDown() override {
    provider_.reset();
    controller_.reset();
    feature_list_.reset();
  }

 protected:
  std::unique_ptr<FakeInputDeviceSettingsController> controller_;
  std::unique_ptr<InputDeviceSettingsProvider> provider_;
  std::unique_ptr<base::test::ScopedFeatureList> feature_list_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(InputDeviceSettingsProviderTest, TestGetConnectedKeyboards) {
  std::vector<::ash::mojom::KeyboardPtr> expected_keyboards;
  expected_keyboards.push_back(kKeyboard1.Clone());
  controller_->AddKeyboard(kKeyboard1.Clone());
  provider_->GetConnectedKeyboards(
      base::BindOnce(ExpectListsEqualByValue<::ash::mojom::KeyboardPtr>,
                     CloneMojomVector(expected_keyboards)));

  expected_keyboards.push_back(kKeyboard2.Clone());
  controller_->AddKeyboard(kKeyboard2.Clone());
  provider_->GetConnectedKeyboards(
      base::BindOnce(ExpectListsEqualByValue<::ash::mojom::KeyboardPtr>,
                     CloneMojomVector(expected_keyboards)));
}

TEST_F(InputDeviceSettingsProviderTest, TestKeyboardSettingsObeserver) {
  std::vector<::ash::mojom::KeyboardPtr> expected_keyboards;
  expected_keyboards.push_back(kKeyboard1.Clone());
  controller_->AddKeyboard(kKeyboard1.Clone());

  FakeKeyboardSettingsObserver fake_observer;
  provider_->ObserveKeyboardSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_called());
  ExpectListsEqual(expected_keyboards, fake_observer.keyboards());

  expected_keyboards.push_back(kKeyboard2.Clone());
  controller_->AddKeyboard(kKeyboard2.Clone());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_observer.num_times_called());
  ExpectListsEqual(expected_keyboards, fake_observer.keyboards());

  expected_keyboards.pop_back();
  controller_->RemoveKeyboard(kKeyboard2.id);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, fake_observer.num_times_called());
  ExpectListsEqual(expected_keyboards, fake_observer.keyboards());
}

TEST_F(InputDeviceSettingsProviderTest, TestTouchpadSettingsObeserver) {
  std::vector<::ash::mojom::TouchpadPtr> expected_touchpads;
  expected_touchpads.push_back(kTouchpad1.Clone());
  controller_->AddTouchpad(kTouchpad1.Clone());

  FakeTouchpadSettingsObserver fake_observer;
  provider_->ObserveTouchpadSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_called());
  ExpectListsEqual(expected_touchpads, fake_observer.touchpads());

  expected_touchpads.push_back(kTouchpad2.Clone());
  controller_->AddTouchpad(kTouchpad2.Clone());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_observer.num_times_called());
  ExpectListsEqual(expected_touchpads, fake_observer.touchpads());

  expected_touchpads.pop_back();
  controller_->RemoveTouchpad(kTouchpad2.id);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, fake_observer.num_times_called());
  ExpectListsEqual(expected_touchpads, fake_observer.touchpads());
}

}  // namespace ash::settings
