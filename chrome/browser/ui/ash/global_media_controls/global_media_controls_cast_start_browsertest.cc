// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/status_area_widget.h"
#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/global_media_controls/media_notification_provider_impl.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_device_selector_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "components/global_media_controls/public/test/mock_device_service.h"
#include "components/global_media_controls/public/views/media_item_ui_list_view.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "content/public/browser/media_session.h"
#include "content/public/test/browser_test.h"
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
  GlobalMediaControlsCastStartTest() = default;
  ~GlobalMediaControlsCastStartTest() override = default;

 protected:
  // `device_service_id` is the ID of the DeviceService resonsible for
  // binding the DeviceListHost that provides the list of devices.
  MediaItemUIDeviceSelectorView* ShowDevicePicker(
      MediaNotificationService* device_service,
      const base::UnguessableToken& device_service_id) {
    device_service->supplemental_device_picker_producer_
        ->GetOrCreateNotificationItem(device_service_id);
    device_service->supplemental_device_picker_producer_->ShowItem();

    MediaTray* media_tray = Shell::GetPrimaryRootWindowController()
                                ->shelf()
                                ->GetStatusAreaWidget()
                                ->media_tray();
    media_tray->ShowBubble();

    auto* list_view = static_cast<global_media_controls::MediaItemUIListView*>(
        media_tray->content_view_for_testing());
    auto* item_view = static_cast<global_media_controls::MediaItemUIView*>(
        list_view->contents()->children().at(0));
    return static_cast<MediaItemUIDeviceSelectorView*>(
        item_view->device_selector_view_for_testing());
  }

  // Returns the ID of the device that was added.
  std::string AddDevice(MediaItemUIDeviceSelectorView* device_picker) {
    const std::string device_id = "device123";
    global_media_controls::mojom::DevicePtr device =
        global_media_controls::mojom::Device::New();
    device->id = device_id;
    std::vector<global_media_controls::mojom::DevicePtr> devices;
    devices.push_back(std::move(device));
    device_picker->OnDevicesUpdated(std::move(devices));
    return device_id;
  }

  void SelectDevice(MediaItemUIDeviceSelectorView* selector_view,
                    const std::string& device_id) {
    views::test::ButtonTestApi(
        selector_view->GetCastDeviceEntryViewsForTesting().at(0))
        .NotifyClick(pressed_event);
  }
};

IN_PROC_BROWSER_TEST_F(GlobalMediaControlsCastStartTest, StartCasting) {
  auto* profile = GetProfile();
  auto* device_service =
      MediaNotificationServiceFactory::GetForProfile(profile);
  base::UnguessableToken device_service_id =
      content::MediaSession::GetSourceId(Profile::FromBrowserContext(profile));
  MediaItemUIDeviceSelectorView* device_picker =
      ShowDevicePicker(device_service, device_service_id);
  std::string device_id = AddDevice(device_picker);
  SelectDevice(device_picker, device_id);
}

}  // namespace ash
