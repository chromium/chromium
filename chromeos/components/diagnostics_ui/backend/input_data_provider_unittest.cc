// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/input_data_provider.h"

#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/device/device_event_observer.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"

namespace chromeos {
namespace diagnostics {

class FakeDeviceManager : public ui::DeviceManager {
 public:
  FakeDeviceManager() {}
  FakeDeviceManager(const FakeDeviceManager&) = delete;
  FakeDeviceManager& operator=(const FakeDeviceManager&) = delete;
  ~FakeDeviceManager() override {}

  // DeviceManager:
  void ScanDevices(ui::DeviceEventObserver* observer) override {}
  void AddObserver(ui::DeviceEventObserver* observer) override {}
  void RemoveObserver(ui::DeviceEventObserver* observer) override {}
};

class TestInputDataProvider : public InputDataProvider {
 public:
  TestInputDataProvider(std::unique_ptr<ui::DeviceManager> device_manager)
      : InputDataProvider(std::move(device_manager)) {}
  TestInputDataProvider(const TestInputDataProvider&) = delete;
  TestInputDataProvider& operator=(const TestInputDataProvider&) = delete;

 protected:
  std::unique_ptr<ui::EventDeviceInfo> GetDeviceInfo(
      base::FilePath path) override {
    std::unique_ptr<ui::EventDeviceInfo> dev_info =
        std::make_unique<ui::EventDeviceInfo>();
    ui::DeviceCapabilities device_caps;
    std::string base_name = path.BaseName().value();
    if (base_name == "event0") {
      device_caps = ui::kLinkKeyboard;
    } else if (base_name == "event1") {
      device_caps = ui::kLinkTouchpad;
    } else if (base_name == "event2") {
      device_caps = ui::kKohakuTouchscreen;
    } else if (base_name == "event3") {
      device_caps = ui::kKohakuStylus;
    } else if (base_name == "event4") {
      device_caps = ui::kHpUsbKeyboard;
    }

    EXPECT_TRUE(ui::CapabilitiesToDeviceInfo(device_caps, dev_info.get()));
    return dev_info;
  }
};

class InputDataProviderTest : public testing::Test {
 public:
  InputDataProviderTest() {
    auto manager = std::make_unique<FakeDeviceManager>();
    manager_ = manager.get();
    provider_ = std::make_unique<TestInputDataProvider>(std::move(manager));
  }

  ~InputDataProviderTest() override {
    provider_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  FakeDeviceManager* manager_;
  std::unique_ptr<InputDataProvider> provider_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(InputDataProviderTest, GetConnectedDevices_DeviceInfoMapping) {
  base::RunLoop run_loop;
  ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event0"));
  ui::DeviceEvent event1(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event1"));
  ui::DeviceEvent event2(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event2"));
  ui::DeviceEvent event3(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event3"));
  provider_->OnDeviceEvent(event0);
  provider_->OnDeviceEvent(event1);
  provider_->OnDeviceEvent(event2);
  provider_->OnDeviceEvent(event3);

  provider_->GetConnectedDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::KeyboardInfoPtr> keyboards,
          std::vector<mojom::TouchDeviceInfoPtr> touch_devices) {
        EXPECT_EQ(1ul, keyboards.size());
        // The stylus device should be filtered out, hence only 2 touch devices.
        EXPECT_EQ(2ul, touch_devices.size());

        mojom::KeyboardInfoPtr keyboard = keyboards[0].Clone();
        EXPECT_EQ(0u, keyboard->id);
        EXPECT_EQ(mojom::ConnectionType::kInternal, keyboard->connection_type);
        EXPECT_EQ("AT Translated Set 2 keyboard", keyboard->name);

        mojom::TouchDeviceInfoPtr touchpad = touch_devices[0].Clone();
        EXPECT_EQ(1u, touchpad->id);
        EXPECT_EQ(mojom::ConnectionType::kInternal, touchpad->connection_type);
        EXPECT_EQ(mojom::TouchDeviceType::kPointer, touchpad->type);
        EXPECT_EQ("Atmel maXTouch Touchpad", touchpad->name);

        mojom::TouchDeviceInfoPtr touchscreen = touch_devices[1].Clone();
        EXPECT_EQ(2u, touchscreen->id);
        EXPECT_EQ(mojom::ConnectionType::kInternal,
                  touchscreen->connection_type);
        EXPECT_EQ(mojom::TouchDeviceType::kDirect, touchscreen->type);
        EXPECT_EQ("Atmel maXTouch Touchscreen", touchscreen->name);

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(InputDataProviderTest, GetConnectedDevices_AddEventAfterFirstCall) {
  base::RunLoop run_loop;
  provider_->GetConnectedDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::KeyboardInfoPtr> keyboards,
          std::vector<mojom::TouchDeviceInfoPtr> touch_devices) {
        EXPECT_EQ(0ul, keyboards.size());
        EXPECT_EQ(0ul, touch_devices.size());
      }));

  ui::DeviceEvent event(ui::DeviceEvent::DeviceType::INPUT,
                        ui::DeviceEvent::ActionType::ADD,
                        base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(event);
  provider_->GetConnectedDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::KeyboardInfoPtr> keyboards,
          std::vector<mojom::TouchDeviceInfoPtr> touch_devices) {
        EXPECT_EQ(1ul, keyboards.size());
        mojom::KeyboardInfoPtr keyboard = keyboards[0].Clone();
        EXPECT_EQ(4u, keyboard->id);
        EXPECT_EQ(mojom::ConnectionType::kUsb, keyboard->connection_type);
        EXPECT_EQ("Chicony HP Elite USB Keyboard", keyboard->name);

        EXPECT_EQ(0ul, touch_devices.size());

        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(InputDataProviderTest, GetConnectedDevices_Remove) {
  base::RunLoop run_loop;
  ui::DeviceEvent add_touch_event(ui::DeviceEvent::DeviceType::INPUT,
                                  ui::DeviceEvent::ActionType::ADD,
                                  base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(add_touch_event);
  ui::DeviceEvent add_kbd_event(ui::DeviceEvent::DeviceType::INPUT,
                                ui::DeviceEvent::ActionType::ADD,
                                base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(add_kbd_event);
  provider_->GetConnectedDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::KeyboardInfoPtr> keyboards,
          std::vector<mojom::TouchDeviceInfoPtr> touch_devices) {
        EXPECT_EQ(1ul, keyboards.size());
        EXPECT_EQ(4u, keyboards[0]->id);

        EXPECT_EQ(1ul, touch_devices.size());
        EXPECT_EQ(1u, touch_devices[0]->id);
      }));

  ui::DeviceEvent remove_touch_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::REMOVE,
                                     base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(remove_touch_event);
  ui::DeviceEvent remove_kbd_event(ui::DeviceEvent::DeviceType::INPUT,
                                   ui::DeviceEvent::ActionType::REMOVE,
                                   base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(remove_kbd_event);
  provider_->GetConnectedDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::KeyboardInfoPtr> keyboards,
          std::vector<mojom::TouchDeviceInfoPtr> touch_devices) {
        EXPECT_EQ(0ul, keyboards.size());
        EXPECT_EQ(0ul, touch_devices.size());

        run_loop.Quit();
      }));

  run_loop.Run();
}

}  // namespace diagnostics
}  // namespace chromeos
