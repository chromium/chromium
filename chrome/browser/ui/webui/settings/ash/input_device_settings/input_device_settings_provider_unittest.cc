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
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/webui/settings/ash/input_device_settings/input_device_settings_provider.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {
namespace {
const ::ash::mojom::Keyboard kKeyboard1 =
    ::ash::mojom::Keyboard(/*name=*/"AT Translated Set 2",
                           /*is_external=*/false,
                           /*id=*/1,
                           /*device_key=*/"fake-device-key1",
                           /*meta_key=*/::ash::mojom::MetaKey::kLauncher,
                           /*modifier_keys=*/{},
                           ::ash::mojom::KeyboardSettings::New());
const ::ash::mojom::Keyboard kKeyboard2 =
    ::ash::mojom::Keyboard(/*name=*/"Logitech K580",
                           /*is_external=*/true,
                           /*id=*/2,
                           /*device_key=*/"fake-device-key2",
                           /*meta_key=*/::ash::mojom::MetaKey::kExternalMeta,
                           /*modifier_keys=*/{},
                           ::ash::mojom::KeyboardSettings::New());
const ::ash::mojom::Keyboard kKeyboard3 =
    ::ash::mojom::Keyboard(/*name=*/"HP 910 White Bluetooth Keyboard",
                           /*is_external=*/true,
                           /*id=*/3,
                           /*device_key=*/"fake-device-key3",
                           /*meta_key=*/::ash::mojom::MetaKey::kExternalMeta,
                           /*modifier_keys=*/{},
                           ::ash::mojom::KeyboardSettings::New());
const ::ash::mojom::Touchpad kTouchpad1 =
    ::ash::mojom::Touchpad(/*name=*/"test touchpad",
                           /*is_external=*/false,
                           /*id=*/3,
                           /*device_key=*/"fake-device-key3",
                           ::ash::mojom::TouchpadSettings::New());
const ::ash::mojom::Touchpad kTouchpad2 =
    ::ash::mojom::Touchpad(/*name=*/"Logitech T650",
                           /*is_external=*/true,
                           /*id=*/4,
                           /*device_key=*/"fake-device-key4",
                           ::ash::mojom::TouchpadSettings::New());
const ::ash::mojom::PointingStick kPointingStick1 =
    ::ash::mojom::PointingStick(/*name=*/"test pointing stick",
                                /*is_external=*/false,
                                /*id=*/5,
                                /*device_key=*/"fake-device-key5",
                                ::ash::mojom::PointingStickSettings::New());
const ::ash::mojom::PointingStick kPointingStick2 =
    ::ash::mojom::PointingStick(/*name=*/"Lexmark-Unicomp FSR",
                                /*is_external=*/true,
                                /*id=*/6,
                                /*device_key=*/"fake-device-key6",
                                ::ash::mojom::PointingStickSettings::New());
const ::ash::mojom::Mouse kMouse1 =
    ::ash::mojom::Mouse(/*name=*/"Razer Basilisk V3",
                        /*is_external=*/false,
                        /*id=*/7,
                        /*device_key=*/"fake-device-key7",
                        ::ash::mojom::MouseSettings::New());
const ::ash::mojom::Mouse kMouse2 =
    ::ash::mojom::Mouse(/*name=*/"MX Anywhere 2S",
                        /*is_external=*/true,
                        /*id=*/8,
                        /*device_key=*/"fake-device-key8",
                        ::ash::mojom::MouseSettings::New());
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
    auto actual_iter = base::ranges::find(actual_list, expected_list[i]);
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
    ++num_times_called_;
  }

  const std::vector<::ash::mojom::MousePtr>& mice() { return mice_; }

  int num_times_called() { return num_times_called_; }
  mojo::Receiver<mojom::MouseSettingsObserver> receiver{this};

 private:
  std::vector<::ash::mojom::MousePtr> mice_;
  int num_times_called_ = 0;
};

class FakeInputDeviceSettingsController : public InputDeviceSettingsController {
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
      ::ash::mojom::KeyboardSettingsPtr settings) override {
    ++num_times_set_keyboard_settings_called_;
  }
  void AddObserver(Observer* observer) override { observer_ = observer; }
  void RemoveObserver(Observer* observer) override { observer_ = nullptr; }
  void SetTouchpadSettings(
      DeviceId id,
      ::ash::mojom::TouchpadSettingsPtr settings) override {
    ++num_times_set_touchpad_settings_called_;
  }
  void SetMouseSettings(DeviceId id,
                        ::ash::mojom::MouseSettingsPtr settings) override {
    ++num_times_set_mouse_settings_called_;
  }
  void SetPointingStickSettings(
      DeviceId id,
      ::ash::mojom::PointingStickSettingsPtr settings) override {
    ++num_times_set_pointing_stick_settings_called_;
  }

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
    observer_->OnMouseConnected(*mice_.back());
  }
  void RemoveMouse(uint32_t device_id) {
    auto iter = base::ranges::find_if(mice_, [device_id](const auto& mouse) {
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
    observer_->OnPointingStickConnected(*pointing_sticks_.back());
  }
  void RemovePointingStick(uint32_t device_id) {
    auto iter = base::ranges::find_if(pointing_sticks_,
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
  int num_times_set_keyboard_settings_called() {
    return num_times_set_keyboard_settings_called_;
  }
  int num_times_set_pointing_stick_settings_called() {
    return num_times_set_pointing_stick_settings_called_;
  }
  int num_times_set_mouse_settings_called() {
    return num_times_set_mouse_settings_called_;
  }
  int num_times_set_touchpad_settings_called() {
    return num_times_set_touchpad_settings_called_;
  }

 private:
  std::vector<::ash::mojom::KeyboardPtr> keyboards_;
  std::vector<::ash::mojom::TouchpadPtr> touchpads_;
  std::vector<::ash::mojom::MousePtr> mice_;
  std::vector<::ash::mojom::PointingStickPtr> pointing_sticks_;
  raw_ptr<InputDeviceSettingsController::Observer> observer_ = nullptr;
  int num_times_set_keyboard_settings_called_ = 0;
  int num_times_set_pointing_stick_settings_called_ = 0;
  int num_times_set_mouse_settings_called_ = 0;
  int num_times_set_touchpad_settings_called_ = 0;
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

TEST_F(InputDeviceSettingsProviderTest, TestSetKeyboardSettings) {
  controller_->AddKeyboard(kKeyboard1.Clone());
  provider_->SetKeyboardSettings(kKeyboard1.id, kKeyboard1.settings->Clone());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, controller_->num_times_set_keyboard_settings_called());

  controller_->AddKeyboard(kKeyboard2.Clone());
  provider_->SetKeyboardSettings(kKeyboard2.id, kKeyboard1.settings->Clone());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, controller_->num_times_set_keyboard_settings_called());
}

TEST_F(InputDeviceSettingsProviderTest, TestSetPointingStickSettings) {
  controller_->AddPointingStick(kPointingStick1.Clone());
  provider_->SetPointingStickSettings(kPointingStick1.id,
                                      kPointingStick1.settings->Clone());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, controller_->num_times_set_pointing_stick_settings_called());

  controller_->AddPointingStick(kPointingStick2.Clone());
  provider_->SetPointingStickSettings(kPointingStick2.id,
                                      kPointingStick1.settings->Clone());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, controller_->num_times_set_pointing_stick_settings_called());
}

TEST_F(InputDeviceSettingsProviderTest, TestSetMouseSettings) {
  controller_->AddMouse(kMouse1.Clone());
  provider_->SetMouseSettings(kMouse1.id, kMouse1.settings->Clone());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, controller_->num_times_set_mouse_settings_called());

  controller_->AddMouse(kMouse2.Clone());
  provider_->SetMouseSettings(kMouse2.id, kMouse1.settings->Clone());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, controller_->num_times_set_mouse_settings_called());
}

TEST_F(InputDeviceSettingsProviderTest, TestSetTouchpadSettings) {
  controller_->AddTouchpad(kTouchpad1.Clone());
  provider_->SetTouchpadSettings(kTouchpad1.id, kTouchpad1.settings->Clone());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, controller_->num_times_set_touchpad_settings_called());

  controller_->AddTouchpad(kTouchpad2.Clone());
  provider_->SetTouchpadSettings(kTouchpad2.id, kTouchpad1.settings->Clone());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, controller_->num_times_set_touchpad_settings_called());
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

TEST_F(InputDeviceSettingsProviderTest, TestDuplicatesRemoved) {
  std::vector<::ash::mojom::KeyboardPtr> expected_keyboards;

  auto keyboard1 = kKeyboard1.Clone();
  keyboard1->device_key = "test-key1";
  expected_keyboards.push_back(keyboard1.Clone());
  controller_->AddKeyboard(keyboard1.Clone());

  FakeKeyboardSettingsObserver fake_observer;
  provider_->ObserveKeyboardSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_called());
  ExpectListsEqual</*sorted=*/true>(expected_keyboards,
                                    fake_observer.keyboards());

  auto keyboard2 = kKeyboard2.Clone();
  keyboard2->device_key = "test-key1";
  controller_->AddKeyboard(keyboard2.Clone());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_observer.num_times_called());
  ExpectListsEqual</*sorted=*/true>(expected_keyboards,
                                    fake_observer.keyboards());

  controller_->RemoveKeyboard(kKeyboard2.id);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, fake_observer.num_times_called());
  ExpectListsEqual</*sorted=*/true>(expected_keyboards,
                                    fake_observer.keyboards());
}

TEST_F(InputDeviceSettingsProviderTest, TestSortingExternalFirst) {
  std::vector<::ash::mojom::KeyboardPtr> expected_keyboards;

  auto keyboard1 = kKeyboard1.Clone();
  auto keyboard2 = kKeyboard2.Clone();

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

  controller_->AddKeyboard(kKeyboard1.Clone());
  ASSERT_FALSE(kKeyboard1.is_external);

  controller_->AddKeyboard(kKeyboard2.Clone());
  ASSERT_TRUE(kKeyboard2.is_external);

  controller_->AddKeyboard(kKeyboard3.Clone());
  ASSERT_TRUE(kKeyboard3.is_external);
  ASSERT_LT(kKeyboard2.id, kKeyboard3.id);

  expected_keyboards.push_back(kKeyboard3.Clone());
  expected_keyboards.push_back(kKeyboard2.Clone());
  expected_keyboards.push_back(kKeyboard1.Clone());

  FakeKeyboardSettingsObserver fake_observer;
  provider_->ObserveKeyboardSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  base::RunLoop().RunUntilIdle();
  ExpectListsEqual</*sorted=*/true>(expected_keyboards,
                                    fake_observer.keyboards());
}

TEST_F(InputDeviceSettingsProviderTest, TestMouseSettingsObeserver) {
  std::vector<::ash::mojom::MousePtr> expected_mice;
  expected_mice.push_back(kMouse1.Clone());
  controller_->AddMouse(kMouse1.Clone());

  FakeMouseSettingsObserver fake_observer;
  provider_->ObserveMouseSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_called());
  ExpectListsEqual(expected_mice, fake_observer.mice());

  expected_mice.push_back(kMouse2.Clone());
  controller_->AddMouse(kMouse2.Clone());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_observer.num_times_called());
  ExpectListsEqual(expected_mice, fake_observer.mice());

  expected_mice.pop_back();
  controller_->RemoveMouse(kMouse2.id);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, fake_observer.num_times_called());
  ExpectListsEqual(expected_mice, fake_observer.mice());
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

TEST_F(InputDeviceSettingsProviderTest, TestPointingStickSettingsObeserver) {
  std::vector<::ash::mojom::PointingStickPtr> expected_pointing_sticks;
  expected_pointing_sticks.push_back(kPointingStick1.Clone());
  controller_->AddPointingStick(kPointingStick1.Clone());

  FakePointingStickSettingsObserver fake_observer;
  provider_->ObservePointingStickSettings(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_observer.num_times_called());
  ExpectListsEqual(expected_pointing_sticks, fake_observer.pointing_sticks());

  expected_pointing_sticks.push_back(kPointingStick2.Clone());
  controller_->AddPointingStick(kPointingStick2.Clone());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, fake_observer.num_times_called());
  ExpectListsEqual(expected_pointing_sticks, fake_observer.pointing_sticks());

  expected_pointing_sticks.pop_back();
  controller_->RemovePointingStick(kPointingStick2.id);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, fake_observer.num_times_called());
  ExpectListsEqual(expected_pointing_sticks, fake_observer.pointing_sticks());
}

}  // namespace ash::settings
