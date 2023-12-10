// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/oobe_display_chooser.h"

#include <memory>
#include <vector>

#include "ash/display/display_configuration_controller.h"
#include "ash/shell.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/test/touch_device_manager_test_api.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/touchscreen_device.h"

namespace ash {

namespace {

class TestCrosDisplayConfig
    : public crosapi::mojom::CrosDisplayConfigController {
 public:
  TestCrosDisplayConfig() = default;

  TestCrosDisplayConfig(const TestCrosDisplayConfig&) = delete;
  TestCrosDisplayConfig& operator=(const TestCrosDisplayConfig&) = delete;

  mojo::PendingRemote<crosapi::mojom::CrosDisplayConfigController>
  CreateRemoteAndBind() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // crosapi::mojom::CrosDisplayConfigController:
  void AddObserver(
      mojo::PendingAssociatedRemote<crosapi::mojom::CrosDisplayConfigObserver>
          observer) override {}
  void GetDisplayLayoutInfo(GetDisplayLayoutInfoCallback callback) override {}
  void SetDisplayLayoutInfo(crosapi::mojom::DisplayLayoutInfoPtr info,
                            SetDisplayLayoutInfoCallback callback) override {}
  void GetDisplayUnitInfoList(
      bool single_unified,
      GetDisplayUnitInfoListCallback callback) override {}
  void SetDisplayProperties(
      const std::string& id,
      crosapi::mojom::DisplayConfigPropertiesPtr properties,
      crosapi::mojom::DisplayConfigSource source,
      SetDisplayPropertiesCallback callback) override {
    if (properties->set_primary) {
      int64_t display_id;
      base::StringToInt64(id, &display_id);
      Shell::Get()->window_tree_host_manager()->SetPrimaryDisplayId(display_id);
    }
    std::move(callback).Run(crosapi::mojom::DisplayConfigResult::kSuccess);
  }
  void SetUnifiedDesktopEnabled(bool enabled) override {}
  void OverscanCalibration(const std::string& display_id,
                           crosapi::mojom::DisplayConfigOperation op,
                           const std::optional<gfx::Insets>& delta,
                           OverscanCalibrationCallback callback) override {}
  void TouchCalibration(const std::string& display_id,
                        crosapi::mojom::DisplayConfigOperation op,
                        crosapi::mojom::TouchCalibrationPtr calibration,
                        TouchCalibrationCallback callback) override {}
  void HighlightDisplay(int64_t id) override {}
  void DragDisplayDelta(int64_t display_id,
                        int32_t delta_x,
                        int32_t delta_y) override {}

 private:
  mojo::Receiver<crosapi::mojom::CrosDisplayConfigController> receiver_{this};
};

class OobeDisplayChooserTest : public ChromeAshTestBase {
 public:
  OobeDisplayChooserTest() : ChromeAshTestBase() {}

  OobeDisplayChooserTest(const OobeDisplayChooserTest&) = delete;
  OobeDisplayChooserTest& operator=(const OobeDisplayChooserTest&) = delete;

  int64_t GetPrimaryDisplay() {
    return display::Screen::GetScreen()->GetPrimaryDisplay().id();
  }

  // ChromeAshTestBase:
  void SetUp() override {
    ChromeAshTestBase::SetUp();

    cros_display_config_ = std::make_unique<TestCrosDisplayConfig>();
    display_chooser_ = std::make_unique<OobeDisplayChooser>();
    display_chooser_->set_cros_display_config_for_test(
        cros_display_config_->CreateRemoteAndBind());

    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
  }

  OobeDisplayChooser* display_chooser() { return display_chooser_.get(); }

 private:
  std::unique_ptr<TestCrosDisplayConfig> cros_display_config_;
  std::unique_ptr<OobeDisplayChooser> display_chooser_;
};

const uint16_t kAllowlistedId = 0x266e;

}  // namespace

TEST_F(OobeDisplayChooserTest, PreferTouchAsPrimary) {
  // Setup 2 displays, second one is intended to be a touch display
  std::vector<display::ManagedDisplayInfo> display_info;
  display_info.push_back(
      display::ManagedDisplayInfo::CreateFromSpecWithID("0+0-3000x2000", 1));
  display_info.push_back(
      display::ManagedDisplayInfo::CreateFromSpecWithID("3000+0-800x600", 2));
  display_manager()->OnNativeDisplaysChanged(display_info);
  base::RunLoop().RunUntilIdle();

  // Make sure the non-touch display is primary
  Shell::Get()->window_tree_host_manager()->SetPrimaryDisplayId(1);

  // Setup corresponding TouchscreenDevice object
  ui::TouchscreenDevice touchscreen =
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_USB,
                            "Touchscreen", gfx::Size(800, 600), 1);
  touchscreen.vendor_id = kAllowlistedId;
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({touchscreen});
  base::RunLoop().RunUntilIdle();

  // Associate touchscreen device with display
  display::test::TouchDeviceManagerTestApi(
      display_manager()->touch_device_manager())
      .Associate(&display_info[1], touchscreen);
  display_manager()->OnNativeDisplaysChanged(display_info);
  base::RunLoop().RunUntilIdle();

  // For mus we have to explicitly tell the InputDeviceClient the
  // TouchscreenDevices. Normally InputDeviceClient is told of the
  // TouchscreenDevices by way of implementing
  // ws::mojom::InputDeviceObserverMojo. In unit tests InputDeviceClient is not
  // wired to the window server (the window server isn't running).
  touchscreen.target_display_id = display_info[1].id();
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({touchscreen}, true);

  EXPECT_EQ(1, GetPrimaryDisplay());
  display_chooser()->TryToPlaceUiOnTouchDisplay();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, GetPrimaryDisplay());
}

TEST_F(OobeDisplayChooserTest, DontSwitchFromTouch) {
  // Setup 2 displays, second one is intended to be a touch display
  std::vector<display::ManagedDisplayInfo> display_info;
  display_info.push_back(
      display::ManagedDisplayInfo::CreateFromSpecWithID("0+0-3000x2000", 1));
  display_info.push_back(
      display::ManagedDisplayInfo::CreateFromSpecWithID("3000+0-800x600", 2));
  display_info[0].set_touch_support(display::Display::TouchSupport::AVAILABLE);
  display_manager()->OnNativeDisplaysChanged(display_info);
  base::RunLoop().RunUntilIdle();

  // Make sure the non-touch display is primary
  Shell::Get()->window_tree_host_manager()->SetPrimaryDisplayId(1);

  // Setup corresponding TouchscreenDevice object
  ui::TouchscreenDevice touchscreen =
      ui::TouchscreenDevice(1, ui::InputDeviceType::INPUT_DEVICE_USB,
                            "Touchscreen", gfx::Size(800, 600), 1);
  touchscreen.vendor_id = kAllowlistedId;
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({touchscreen});
  base::RunLoop().RunUntilIdle();

  // Associate touchscreen device with display
  display::test::TouchDeviceManagerTestApi(
      display_manager()->touch_device_manager())
      .Associate(&display_info[1], touchscreen);
  display_manager()->OnNativeDisplaysChanged(display_info);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, GetPrimaryDisplay());
  display_chooser()->TryToPlaceUiOnTouchDisplay();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, GetPrimaryDisplay());
}

}  // namespace ash
