// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_touch_device_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_hotplug_event_observer.h"
#include "ui/gfx/geometry/rect.h"

namespace {

constexpr int64_t kDisplayId = 1;
constexpr int kTouchDeviceId = 100;

}  // namespace

class CastTouchDeviceManagerTest : public testing::Test {
 public:
  CastTouchDeviceManagerTest() {}

  void SetUp() override {
    ui::DeviceDataManager::CreateInstance();
    touch_device_manager_ =
        std::make_unique<chromecast::shell::CastTouchDeviceManager>();
  }

  void TearDown() override {
    touch_device_manager_.reset();
    ui::DeviceDataManager::DeleteInstance();
  }

  ui::DeviceHotplugEventObserver* GetHotplugObserver() {
    return ui::DeviceDataManager::GetInstance();
  }

 protected:
  std::unique_ptr<chromecast::shell::CastTouchDeviceManager>
      touch_device_manager_;
};

TEST_F(CastTouchDeviceManagerTest, CheckOneToOneMapping) {
  std::vector<ui::TouchscreenDevice> touchscreens;
  const gfx::Size display_size = gfx::Size(1280, 720);
  touch_device_manager_->OnDisplayConfigured(
      kDisplayId, display::Display::ROTATE_0, gfx::Rect(display_size));
  touchscreens.push_back(ui::TouchscreenDevice(kTouchDeviceId,
                                               ui::INPUT_DEVICE_INTERNAL,
                                               "Touchscreen", display_size, 1));
  GetHotplugObserver()->OnTouchscreenDevicesUpdated(touchscreens);

  // 1:1 mapping for touch coordinates to display coordinates.
  {
    float x = display_size.width(), y = display_size.height();
    ui::DeviceDataManager::GetInstance()->ApplyTouchTransformer(kTouchDeviceId,
                                                                &x, &y);
    EXPECT_EQ(display_size.width(), x);
    EXPECT_EQ(display_size.height(), y);
  }

  {
    float x = display_size.width(), y = 0;
    ui::DeviceDataManager::GetInstance()->ApplyTouchTransformer(kTouchDeviceId,
                                                                &x, &y);
    EXPECT_EQ(display_size.width(), x);
    EXPECT_EQ(0, y);
  }

  {
    float x = 0, y = display_size.height();
    ui::DeviceDataManager::GetInstance()->ApplyTouchTransformer(kTouchDeviceId,
                                                                &x, &y);
    EXPECT_EQ(0, x);
    EXPECT_EQ(display_size.height(), y);
  }

  {
    float x = display_size.width() / 2, y = display_size.height() / 2;
    ui::DeviceDataManager::GetInstance()->ApplyTouchTransformer(1, &x, &y);
    EXPECT_EQ(display_size.width() / 2, x);
    EXPECT_EQ(display_size.height() / 2, y);
  }
}

TEST_F(CastTouchDeviceManagerTest, CheckMappingWithLargerTouchscreen) {
  std::vector<ui::TouchscreenDevice> touchscreens;
  const gfx::Size display_size = gfx::Size(1280, 720);
  touch_device_manager_->OnDisplayConfigured(
      kDisplayId, display::Display::ROTATE_0, gfx::Rect(display_size));
  touchscreens.push_back(ui::TouchscreenDevice(
      kTouchDeviceId, ui::INPUT_DEVICE_INTERNAL, "Touchscreen",
      gfx::ScaleToRoundedSize(display_size, 2, 2), 1));
  GetHotplugObserver()->OnTouchscreenDevicesUpdated(touchscreens);

  // Touch screen is twice the size, so transformed events will be half the
  // reported value.
  {
    float x = display_size.width(), y = display_size.height();
    ui::DeviceDataManager::GetInstance()->ApplyTouchTransformer(kTouchDeviceId,
                                                                &x, &y);
    EXPECT_EQ(display_size.width() / 2, x);
    EXPECT_EQ(display_size.height() / 2, y);
  }

  {
    float x = display_size.width(), y = 0;
    ui::DeviceDataManager::GetInstance()->ApplyTouchTransformer(kTouchDeviceId,
                                                                &x, &y);
    EXPECT_EQ(display_size.width() / 2, x);
    EXPECT_EQ(0, y);
  }

  {
    float x = 0, y = display_size.height();
    ui::DeviceDataManager::GetInstance()->ApplyTouchTransformer(kTouchDeviceId,
                                                                &x, &y);
    EXPECT_EQ(0, x);
    EXPECT_EQ(display_size.height() / 2, y);
  }

  {
    float x = display_size.width() / 2, y = display_size.height() / 2;
    ui::DeviceDataManager::GetInstance()->ApplyTouchTransformer(kTouchDeviceId,
                                                                &x, &y);
    EXPECT_EQ(display_size.width() / 4, x);
    EXPECT_EQ(display_size.height() / 4, y);
  }
}
