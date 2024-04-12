// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/cast_device_selector_view.h"

#include "chrome/test/views/chrome_views_test_base.h"
#include "components/global_media_controls/public/test/mock_device_service.h"
#include "components/media_message_center/mock_media_notification_view.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/button_test_api.h"

using ::global_media_controls::test::MockDeviceListHost;
using ::media_message_center::test::MockMediaNotificationView;
using ::testing::NiceMock;

namespace {

const char kTestDeviceId[] = "test-device-id";
const char kTestDeviceName[] = "test-device-name";
const char kTestDeviceStatusText[] = "test-device-status-text";

}  // anonymous namespace

class CastDeviceSelectorViewTest : public ChromeViewsTestBase {
 public:
  CastDeviceSelectorViewTest() = default;
  CastDeviceSelectorViewTest(const CastDeviceSelectorViewTest&) = delete;
  CastDeviceSelectorViewTest& operator=(const CastDeviceSelectorViewTest&) =
      delete;
  ~CastDeviceSelectorViewTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    device_list_host_ = std::make_unique<MockDeviceListHost>();
  }

  void TearDown() override {
    cast_device_selector_view_ = nullptr;
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void CreateCastDeviceSelectorView(bool show_devices) {
    cast_device_selector_view_ =
        widget_->SetContentsView(std::make_unique<CastDeviceSelectorView>(
            device_list_host_->PassRemote(),
            client_remote_.BindNewPipeAndPassReceiver(),
            media_message_center::MediaColorTheme(), show_devices));
    widget_->Show();
  }

  std::vector<global_media_controls::mojom::DevicePtr> CreateDevices() {
    global_media_controls::mojom::DevicePtr device =
        global_media_controls::mojom::Device::New(
            kTestDeviceId, kTestDeviceName, kTestDeviceStatusText,
            global_media_controls::mojom::IconType::kTv);
    std::vector<global_media_controls::mojom::DevicePtr> devices;
    devices.push_back(std::move(device));
    return devices;
  }

  CastDeviceSelectorView* view() { return cast_device_selector_view_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<MockDeviceListHost> device_list_host_;
  mojo::Remote<global_media_controls::mojom::DeviceListClient> client_remote_;
  raw_ptr<CastDeviceSelectorView> cast_device_selector_view_;
};

TEST_F(CastDeviceSelectorViewTest, ShowDevicesAfterDevicesUpdated) {
  CreateCastDeviceSelectorView(/*show_devices=*/true);
  EXPECT_FALSE(view()->GetVisible());
  view()->OnDevicesUpdated(CreateDevices());
  EXPECT_TRUE(view()->GetVisible());
}

TEST_F(CastDeviceSelectorViewTest, ShowOrHideDevices) {
  CreateCastDeviceSelectorView(/*show_devices=*/false);
  view()->OnDevicesUpdated(CreateDevices());
  EXPECT_FALSE(view()->GetVisible());

  view()->ShowDevices();
  EXPECT_TRUE(view()->GetVisible());

  view()->HideDevices();
  EXPECT_FALSE(view()->GetVisible());
}

TEST_F(CastDeviceSelectorViewTest, CloseButtonCheck) {
  CreateCastDeviceSelectorView(/*show_devices=*/true);
  view()->OnDevicesUpdated(CreateDevices());
  EXPECT_NE(view()->GetCloseButtonForTesting(), nullptr);
  EXPECT_TRUE(view()->GetCloseButtonForTesting()->GetVisible());

  views::test::ButtonTestApi(view()->GetCloseButtonForTesting())
      .NotifyClick(ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
  EXPECT_FALSE(view()->GetVisible());
}
