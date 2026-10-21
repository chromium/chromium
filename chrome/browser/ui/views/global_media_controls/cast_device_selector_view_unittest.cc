// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/cast_device_selector_view.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/global_media_controls/public/test/mock_device_service.h"
#include "components/global_media_controls/public/views/media_item_ui_updated_view.h"
#include "components/media_message_center/mock_media_notification_item.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/button_test_api.h"

using ::global_media_controls::test::MockDeviceListHost;

namespace {

const char kTestDeviceId[] = "test-device-id";
const char kTestDeviceName[] = "test-device-name";
const char kTestDeviceStatusText[] = "test-device-status-text";
const char kTestDeviceId2[] = "test-device-id-2";
const char kTestDeviceName2[] = "test-device-name-2";

// Records the accessible name of `view` at the moment it gets focus.
class FocusedNameRecorder : public views::FocusChangeListener {
 public:
  explicit FocusedNameRecorder(views::View* view) : view_(view) {}

  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override {
    if (focused_now == view_) {
      name_ = view_->GetViewAccessibility().GetCachedName();
    }
  }

  const std::optional<std::u16string>& name() const { return name_; }

 private:
  raw_ptr<views::View> view_;
  std::optional<std::u16string> name_;
};

void ClickButton(views::View* button) {
  views::test::ButtonTestApi(static_cast<views::Button*>(button))
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
}

std::vector<global_media_controls::mojom::DevicePtr> CreateTwoDevices(
    bool reversed) {
  std::vector<global_media_controls::mojom::DevicePtr> devices;
  devices.push_back(global_media_controls::mojom::Device::New(
      kTestDeviceId, kTestDeviceName, kTestDeviceStatusText,
      global_media_controls::mojom::IconType::kTv));
  devices.push_back(global_media_controls::mojom::Device::New(
      kTestDeviceId2, kTestDeviceName2, kTestDeviceStatusText,
      global_media_controls::mojom::IconType::kSpeaker));
  if (reversed) {
    std::swap(devices[0], devices[1]);
  }
  return devices;
}

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
    widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
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

  MockDeviceListHost* device_list_host() { return device_list_host_.get(); }
  CastDeviceSelectorView* view() { return cast_device_selector_view_; }
  views::Widget* widget() { return widget_.get(); }

  // Creates a `MediaItemUIUpdatedView` that hosts a `CastDeviceSelectorView`.
  global_media_controls::MediaItemUIUpdatedView* CreateMediaItemUIUpdatedView(
      media_message_center::test::MockMediaNotificationItem* item) {
    auto device_selector = std::make_unique<CastDeviceSelectorView>(
        device_list_host_->PassRemote(),
        client_remote_.BindNewPipeAndPassReceiver(),
        media_message_center::MediaColorTheme(), /*show_devices=*/false);
    cast_device_selector_view_ = device_selector.get();
    auto* media_item_ui_updated_view = widget_->SetContentsView(
        std::make_unique<global_media_controls::MediaItemUIUpdatedView>(
            "test-id", item->GetWeakPtr(),
            media_message_center::MediaColorTheme(), std::move(device_selector),
            /*footer_view=*/nullptr));
    widget_->Show();
    return media_item_ui_updated_view;
  }

  // Adds a `CastDeviceSelectorView` as a child of `parent`.
  void AddCastDeviceSelectorViewTo(views::View* parent, bool show_devices) {
    cast_device_selector_view_ =
        parent->AddChildView(std::make_unique<CastDeviceSelectorView>(
            device_list_host_->PassRemote(),
            client_remote_.BindNewPipeAndPassReceiver(),
            media_message_center::MediaColorTheme(), show_devices));
  }

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
  base::HistogramTester histogram_tester;
  CreateCastDeviceSelectorView(/*show_devices=*/true);
  view()->OnDevicesUpdated(CreateDevices());
  EXPECT_NE(view()->GetCloseButtonForTesting(), nullptr);
  EXPECT_TRUE(view()->GetCloseButtonForTesting()->GetVisible());

  views::test::ButtonTestApi(view()->GetCloseButtonForTesting())
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
  EXPECT_FALSE(view()->GetVisible());

  histogram_tester.ExpectBucketCount(
      global_media_controls::kMediaItemUIUpdatedViewActionHistogram,
      global_media_controls::MediaItemUIUpdatedViewAction::
          kCloseDeviceListForCasting,
      1);
}

TEST_F(CastDeviceSelectorViewTest, DeviceEntryCheck) {
  CreateCastDeviceSelectorView(/*show_devices=*/true);
  view()->OnDevicesUpdated(CreateDevices());
  EXPECT_FALSE(view()->GetHasDeviceIssueForTesting());
  EXPECT_NE(view()->GetDeviceContainerViewForTesting(), nullptr);
  for (views::View* child :
       view()->GetDeviceContainerViewForTesting()->children()) {
    EXPECT_CALL(*device_list_host(), SelectDevice(kTestDeviceId));
    views::test::ButtonTestApi(static_cast<views::Button*>(child))
        .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                    gfx::Point(), ui::EventTimeForNow(), 0, 0));
  }
}

TEST_F(CastDeviceSelectorViewTest, DeviceEntryWithIssueCheck) {
  global_media_controls::mojom::DevicePtr device =
      global_media_controls::mojom::Device::New(
          kTestDeviceId, kTestDeviceName, kTestDeviceStatusText,
          global_media_controls::mojom::IconType::kInfo);
  std::vector<global_media_controls::mojom::DevicePtr> devices;
  devices.push_back(std::move(device));

  CreateCastDeviceSelectorView(/*show_devices=*/true);
  view()->OnDevicesUpdated(std::move(devices));
  EXPECT_TRUE(view()->GetHasDeviceIssueForTesting());
  EXPECT_NE(view()->GetDeviceContainerViewForTesting(), nullptr);
  for (views::View* child :
       view()->GetDeviceContainerViewForTesting()->children()) {
    auto* device_button = static_cast<IssueHoverButton*>(child);
    EXPECT_EQ(base::UTF16ToUTF8(device_button->device_name_label()->GetText()),
              kTestDeviceName);
    EXPECT_EQ(base::UTF16ToUTF8(device_button->status_text_label()->GetText()),
              kTestDeviceStatusText);
  }
}

TEST_F(CastDeviceSelectorViewTest, ShowPermissionRejectedError) {
  CreateCastDeviceSelectorView(/*show_devices=*/true);

  // Show the permission rejected error.
  view()->OnPermissionRejected();
  EXPECT_TRUE(view()->GetCloseButtonForTesting()->GetVisible());
  EXPECT_TRUE(view()->GetPermissionRejectedViewForTesting()->GetVisible());
  EXPECT_FALSE(view()->GetDeviceContainerViewForTesting()->GetVisible());

  // Dismiss the error.
  views::test::ButtonTestApi(view()->GetCloseButtonForTesting())
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(), 0, 0));
  EXPECT_FALSE(view()->GetPermissionRejectedViewForTesting()->GetVisible());

  // The permission rejected error is not showing.
  view()->OnPermissionRejected();
  EXPECT_FALSE(view()->GetPermissionRejectedViewForTesting()->GetVisible());
}

TEST_F(CastDeviceSelectorViewTest, ShowDevicesFocusesFirstDevice) {
  CreateCastDeviceSelectorView(/*show_devices=*/false);
  view()->OnDevicesUpdated(CreateTwoDevices(/*reversed=*/false));
  EXPECT_FALSE(view()->GetVisible());

  view()->ShowDevices();
  EXPECT_TRUE(view()->GetVisible());
  EXPECT_TRUE(
      view()->GetDeviceContainerViewForTesting()->children()[0]->HasFocus());
}

TEST_F(CastDeviceSelectorViewTest,
       ShowDevicesFocusesCloseButtonForPermissionRejectedError) {
  CreateCastDeviceSelectorView(/*show_devices=*/false);
  view()->OnPermissionRejected();
  EXPECT_FALSE(view()->GetVisible());

  view()->ShowDevices();
  EXPECT_TRUE(view()->GetVisible());
  EXPECT_TRUE(view()->GetCloseButtonForTesting()->HasFocus());
}

TEST_F(CastDeviceSelectorViewTest, DevicesUpdatedKeepsFocusOnSameDevice) {
  CreateCastDeviceSelectorView(/*show_devices=*/false);
  view()->OnDevicesUpdated(CreateTwoDevices(/*reversed=*/false));
  view()->ShowDevices();

  // Focus the second device, then update the list with the device order
  // reversed.
  views::View* container = view()->GetDeviceContainerViewForTesting();
  container->children()[1]->RequestFocus();
  ASSERT_TRUE(container->children()[1]->HasFocus());
  view()->OnDevicesUpdated(CreateTwoDevices(/*reversed=*/true));

  // Focus should stay on the same device, which is now the first entry.
  EXPECT_TRUE(container->children()[0]->HasFocus());
}

TEST_F(CastDeviceSelectorViewTest, DevicesUpdatedFocusesFirstDeviceIfRemoved) {
  CreateCastDeviceSelectorView(/*show_devices=*/false);
  view()->OnDevicesUpdated(CreateTwoDevices(/*reversed=*/false));
  view()->ShowDevices();

  // Focus the second device, then update the list without it.
  views::View* container = view()->GetDeviceContainerViewForTesting();
  container->children()[1]->RequestFocus();
  ASSERT_TRUE(container->children()[1]->HasFocus());
  view()->OnDevicesUpdated(CreateDevices());

  ASSERT_EQ(container->children().size(), 1u);
  EXPECT_TRUE(container->children()[0]->HasFocus());
}

TEST_F(CastDeviceSelectorViewTest, DevicesUpdatedDoesNotStealFocus) {
  CreateCastDeviceSelectorView(/*show_devices=*/false);
  view()->OnDevicesUpdated(CreateTwoDevices(/*reversed=*/false));
  view()->ShowDevices();

  // Focus the close button, which is not a device entry.
  view()->GetCloseButtonForTesting()->RequestFocus();
  ASSERT_TRUE(view()->GetCloseButtonForTesting()->HasFocus());
  view()->OnDevicesUpdated(CreateTwoDevices(/*reversed=*/true));

  EXPECT_TRUE(view()->GetCloseButtonForTesting()->HasFocus());
}

TEST_F(CastDeviceSelectorViewTest, FocusMovesWithStartCastingButtonAndClose) {
  testing::NiceMock<media_message_center::test::MockMediaNotificationItem> item;
  global_media_controls::MediaItemUIUpdatedView* media_item_ui_updated_view =
      CreateMediaItemUIUpdatedView(&item);
  view()->OnDevicesUpdated(CreateTwoDevices(/*reversed=*/false));

  views::View* start_casting_button =
      media_item_ui_updated_view->GetStartCastingButtonForTesting();
  ASSERT_TRUE(start_casting_button->GetVisible());
  start_casting_button->RequestFocus();
  ASSERT_TRUE(start_casting_button->HasFocus());

  // Pressing the start casting button shows the device list and moves focus to
  // the first device.
  ClickButton(start_casting_button);
  EXPECT_TRUE(view()->GetVisible());
  EXPECT_TRUE(
      view()->GetDeviceContainerViewForTesting()->children()[0]->HasFocus());

  // Pressing the close button hides the device list and returns focus to the
  // start casting button.
  view()->GetCloseButtonForTesting()->RequestFocus();
  ASSERT_TRUE(view()->GetCloseButtonForTesting()->HasFocus());
  ClickButton(view()->GetCloseButtonForTesting());
  EXPECT_FALSE(view()->GetVisible());
  EXPECT_TRUE(start_casting_button->HasFocus());

  // Pressing the start casting button again moves focus back to the list.
  // Pressing it again, e.g. with a mouse which does not move focus, hides the
  // list and returns focus to the start casting button.
  ClickButton(start_casting_button);
  EXPECT_TRUE(
      view()->GetDeviceContainerViewForTesting()->children()[0]->HasFocus());
  ClickButton(start_casting_button);
  EXPECT_FALSE(view()->GetVisible());
  EXPECT_TRUE(start_casting_button->HasFocus());
}

TEST_F(CastDeviceSelectorViewTest, PermissionRejectedMovesFocusToCloseButton) {
  CreateCastDeviceSelectorView(/*show_devices=*/false);
  view()->OnDevicesUpdated(CreateTwoDevices(/*reversed=*/false));
  view()->ShowDevices();
  ASSERT_TRUE(
      view()->GetDeviceContainerViewForTesting()->children()[0]->HasFocus());

  // The device entries are hidden by the permission rejected error, so focus
  // should move to a visible view.
  view()->OnPermissionRejected();
  EXPECT_FALSE(view()->GetDeviceContainerViewForTesting()->GetVisible());
  EXPECT_TRUE(view()->GetCloseButtonForTesting()->HasFocus());
}

TEST_F(CastDeviceSelectorViewTest,
       DevicesUpdatedMovesFocusFromPermissionRejectedErrorToFirstDevice) {
  CreateCastDeviceSelectorView(/*show_devices=*/false);
  view()->OnPermissionRejected();
  view()->ShowDevices();

  // The permission rejected view is only focusable with full keyboard access.
  widget()->GetFocusManager()->SetKeyboardAccessible(true);
  views::View* permission_rejected_view =
      view()->GetPermissionRejectedViewForTesting();
  permission_rejected_view->RequestFocus();
  ASSERT_TRUE(permission_rejected_view->HasFocus());

  // The permission rejected error is replaced by the device entries.
  view()->OnDevicesUpdated(CreateTwoDevices(/*reversed=*/false));
  EXPECT_FALSE(permission_rejected_view->GetVisible());
  EXPECT_TRUE(
      view()->GetDeviceContainerViewForTesting()->children()[0]->HasFocus());
}

TEST_F(CastDeviceSelectorViewTest,
       StartCastingButtonHasUpdatedNameWhenFocusReturns) {
  testing::NiceMock<media_message_center::test::MockMediaNotificationItem> item;
  global_media_controls::MediaItemUIUpdatedView* media_item_ui_updated_view =
      CreateMediaItemUIUpdatedView(&item);
  view()->OnDevicesUpdated(CreateTwoDevices(/*reversed=*/false));
  views::View* start_casting_button =
      media_item_ui_updated_view->GetStartCastingButtonForTesting();
  ClickButton(start_casting_button);
  view()->GetCloseButtonForTesting()->RequestFocus();

  FocusedNameRecorder recorder(start_casting_button);
  widget()->GetFocusManager()->AddFocusChangeListener(&recorder);
  ClickButton(view()->GetCloseButtonForTesting());
  widget()->GetFocusManager()->RemoveFocusChangeListener(&recorder);

  EXPECT_TRUE(start_casting_button->HasFocus());
  EXPECT_EQ(recorder.name(),
            start_casting_button->GetViewAccessibility().GetCachedName());
}

TEST_F(CastDeviceSelectorViewTest, EmptyDevicesUpdatedMovesFocusOffHiddenView) {
  testing::NiceMock<media_message_center::test::MockMediaNotificationItem> item;
  CreateMediaItemUIUpdatedView(&item);
  view()->OnDevicesUpdated(CreateTwoDevices(/*reversed=*/false));
  view()->ShowDevices();
  view()->GetCloseButtonForTesting()->RequestFocus();
  ASSERT_TRUE(view()->GetCloseButtonForTesting()->HasFocus());

  // There are no devices left, so the device selector view is hidden.
  view()->OnDevicesUpdated({});
  EXPECT_FALSE(view()->GetVisible());
  views::View* focused_view = widget()->GetFocusManager()->GetFocusedView();
  ASSERT_TRUE(focused_view);
  EXPECT_FALSE(view()->Contains(focused_view));
  EXPECT_TRUE(focused_view->IsDrawn());
}

TEST_F(CastDeviceSelectorViewTest,
       ShowDevicesBeforeDevicesUpdatedFocusesFirst) {
  // This happens when the dialog is opened from the presentation entry point.
  CreateCastDeviceSelectorView(/*show_devices=*/true);
  view()->OnDevicesUpdated(CreateTwoDevices(/*reversed=*/false));
  EXPECT_TRUE(
      view()->GetDeviceContainerViewForTesting()->children()[0]->HasFocus());

  // Later updates do not move focus again.
  view()->GetCloseButtonForTesting()->RequestFocus();
  view()->OnDevicesUpdated(CreateTwoDevices(/*reversed=*/true));
  EXPECT_TRUE(view()->GetCloseButtonForTesting()->HasFocus());
}

TEST_F(CastDeviceSelectorViewTest,
       ShowDevicesBeforeDevicesUpdatedDoesNotStealFocus) {
  CreateCastDeviceSelectorView(/*show_devices=*/true);
  views::View* focus_target =
      widget()->GetRootView()->AddChildView(std::make_unique<views::View>());
  focus_target->SetBounds(0, 0, 20, 20);
  focus_target->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  focus_target->RequestFocus();
  ASSERT_TRUE(focus_target->HasFocus());

  view()->OnDevicesUpdated(CreateTwoDevices(/*reversed=*/false));
  EXPECT_TRUE(focus_target->HasFocus());
}

TEST_F(CastDeviceSelectorViewTest,
       ShowDevicesDoesNotStealFocusFromViewFocusedBeforeCreation) {
  // This happens when the media item is refreshed, e.g. when remote playback
  // starts, and a new device selector view is created while focus is on
  // another view.
  views::View* contents =
      widget()->SetContentsView(std::make_unique<views::View>());
  widget()->Show();
  views::View* focus_target =
      contents->AddChildView(std::make_unique<views::View>());
  focus_target->SetBounds(0, 0, 20, 20);
  focus_target->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  focus_target->RequestFocus();
  ASSERT_TRUE(focus_target->HasFocus());

  AddCastDeviceSelectorViewTo(contents, /*show_devices=*/true);
  view()->OnDevicesUpdated(CreateTwoDevices(/*reversed=*/false));
  EXPECT_TRUE(focus_target->HasFocus());
}

TEST_F(CastDeviceSelectorViewTest,
       ShowDevicesBeforePermissionRejectedFocusesCloseButton) {
  CreateCastDeviceSelectorView(/*show_devices=*/true);
  view()->OnPermissionRejected();
  EXPECT_TRUE(view()->GetCloseButtonForTesting()->HasFocus());
}
