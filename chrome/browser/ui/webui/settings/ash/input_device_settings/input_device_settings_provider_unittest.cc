// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/input_device_settings/input_device_settings_provider.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {
namespace {
const ::ash::mojom::Keyboard keyboard1 =
    ::ash::mojom::Keyboard(/*name=*/"AT Translated Set 2",
                           /*is_external=*/false,
                           /*id=*/0,
                           /*device_key=*/"fake-device-key1",
                           /*meta_key=*/::ash::mojom::MetaKey::kLauncher,
                           /*modifier_keys=*/{},
                           nullptr);
const ::ash::mojom::Keyboard keyboard2 =
    ::ash::mojom::Keyboard(/*name=*/"Logitech K580",
                           /*is_external=*/true,
                           /*id=*/1,
                           /*device_key=*/"fake-device-key2",
                           /*meta_key=*/::ash::mojom::MetaKey::kExternalMeta,
                           /*modifier_keys=*/{},
                           nullptr);

template <typename T>
void ExpectListsEqual(std::vector<T> expected_list,
                      std::vector<T> actual_list) {
  ASSERT_EQ(expected_list.size(), actual_list.size());
  for (size_t i = 0; i < actual_list.size(); i++) {
    EXPECT_EQ(expected_list[i], actual_list[i]);
  }
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

  void SetKeyboardSettings(
      DeviceId id,
      const ::ash::mojom::KeyboardSettings& settings) override {}
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}

  void AddKeyboard(::ash::mojom::KeyboardPtr keyboard) {
    keyboards_.push_back(std::move(keyboard));
  }
  void RemoveKeyboard(uint32_t device_id) {
    base::EraseIf(keyboards_, [device_id](const auto& keyboard) {
      return keyboard->id == device_id;
    });
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
  }
  void RemoveTouchpad(uint32_t device_id) {
    base::EraseIf(touchpads_, [device_id](const auto& touchpad) {
      return touchpad->id == device_id;
    });
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
    provider_ =
        std::make_unique<InputDeviceSettingsProvider>(controller_.get());
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
};

TEST_F(InputDeviceSettingsProviderTest, TestGetConnectedKeyboards) {
  std::vector<::ash::mojom::KeyboardPtr> expected_keyboards;
  expected_keyboards.push_back(keyboard1.Clone());
  controller_->AddKeyboard(keyboard1.Clone());
  provider_->GetConnectedKeyboards(
      base::BindOnce(ExpectListsEqual<::ash::mojom::KeyboardPtr>,
                     CloneMojomVector(expected_keyboards)));

  expected_keyboards.push_back(keyboard2.Clone());
  controller_->AddKeyboard(keyboard2.Clone());
  provider_->GetConnectedKeyboards(
      base::BindOnce(ExpectListsEqual<::ash::mojom::KeyboardPtr>,
                     CloneMojomVector(expected_keyboards)));
}

}  // namespace ash::settings
