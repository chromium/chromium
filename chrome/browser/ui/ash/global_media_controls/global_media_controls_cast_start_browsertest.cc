// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/status_area_widget.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/media_ui_ash.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/ash/global_media_controls/media_notification_provider_impl.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_device_selector_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "components/global_media_controls/public/test/mock_device_service.h"
#include "components/global_media_controls/public/views/media_item_ui_list_view.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/test/button_test_api.h"

namespace ash {
namespace {
ui::MouseEvent pressed_event(ui::EventType::kMousePressed,
                             gfx::Point(),
                             gfx::Point(),
                             ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
}  // namespace

class GlobalMediaControlsCastStartTest : public InProcessBrowserTest {
 public:
  GlobalMediaControlsCastStartTest() {
    feature_list_.InitAndEnableFeature(
        media_router::kGlobalMediaControlsCastStartStop);
  }

 protected:
  // Registers a DeviceService that will later bind a DeviceListHost when asked.
  // Returns the ID of the registered DeviceService.
  base::UnguessableToken RegisterDeviceService() {
    const base::UnguessableToken device_service_id =
        base::UnguessableToken::Create();
    EXPECT_CALL(device_service_, SetDevicePickerProvider)
        .WillOnce(testing::WithArg<0>(
            [&](mojo::PendingRemote<
                global_media_controls::mojom::DevicePickerProvider> provider) {
              picker_provider_.Bind(std::move(provider));
            }));
    media_ui_ash()->RegisterDeviceService(device_service_id,
                                          device_service_.PassRemote());
    device_service_.FlushForTesting();
    return device_service_id;
  }

  // `device_service_id` is the ID of the DeviceService resonsible for
  // binding the DeviceListHost that provides the list of devices.
  MediaItemUIDeviceSelectorView* ShowDevicePicker(
      const base::UnguessableToken& device_service_id) {
    CHECK(picker_provider_.is_bound());
    picker_provider_->CreateItem(device_service_id);
    picker_provider_->ShowItem();
    picker_provider_.FlushForTesting();

    EXPECT_CALL(device_service_, GetDeviceListHostForPresentation)
        .WillOnce(
            [&](mojo::PendingReceiver<
                    global_media_controls::mojom::DeviceListHost> host_receiver,
                mojo::PendingRemote<
                    global_media_controls::mojom::DeviceListClient>
                    client_remote) {
              device_list_host_.BindReceiver(std::move(host_receiver));
              device_list_client_.Bind(std::move(client_remote));
            });
    MediaTray* media_tray = Shell::GetPrimaryRootWindowController()
                                ->shelf()
                                ->GetStatusAreaWidget()
                                ->media_tray();
    media_tray->ShowBubble();
    device_service_.FlushForTesting();

    auto* list_view = static_cast<global_media_controls::MediaItemUIListView*>(
        media_tray->content_view_for_testing());
    auto* item_view = static_cast<global_media_controls::MediaItemUIView*>(
        list_view->contents()->children().at(0));
    return static_cast<MediaItemUIDeviceSelectorView*>(
        item_view->device_selector_view_for_testing());
  }

  // Returns the ID of the device that was added.
  std::string AddDevice() {
    const std::string device_id = "device123";
    global_media_controls::mojom::DevicePtr device =
        global_media_controls::mojom::Device::New();
    device->id = device_id;
    std::vector<global_media_controls::mojom::DevicePtr> devices;
    devices.push_back(std::move(device));
    CHECK(device_list_client_.is_bound());
    device_list_client_->OnDevicesUpdated(std::move(devices));
    device_list_client_.FlushForTesting();
    return device_id;
  }

  void SelectDeviceAndVerify(MediaItemUIDeviceSelectorView* selector_view,
                             const std::string& device_id) {
    EXPECT_CALL(device_list_host_, SelectDevice(device_id));
    views::test::ButtonTestApi(
        selector_view->GetCastDeviceEntryViewsForTesting().at(0))
        .NotifyClick(pressed_event);
    device_list_host_.FlushForTesting();
  }

  crosapi::MediaUIAsh* media_ui_ash() {
    return crosapi::CrosapiManager::Get()->crosapi_ash()->media_ui_ash();
  }

  base::test::ScopedFeatureList feature_list_;

  // Remotes to objects in Ash.
  mojo::Remote<global_media_controls::mojom::DevicePickerProvider>
      picker_provider_;
  mojo::Remote<global_media_controls::mojom::DeviceListClient>
      device_list_client_;

  // Mocks for objects in Lacros that communicate with Chromecast devices.
  global_media_controls::test::MockDeviceService device_service_;
  global_media_controls::test::MockDeviceListHost device_list_host_;
};

IN_PROC_BROWSER_TEST_F(GlobalMediaControlsCastStartTest, StartCasting) {
  base::UnguessableToken device_service_id = RegisterDeviceService();
  MediaItemUIDeviceSelectorView* device_picker =
      ShowDevicePicker(device_service_id);
  std::string device_id = AddDevice();
  SelectDeviceAndVerify(device_picker, device_id);
}

}  // namespace ash
