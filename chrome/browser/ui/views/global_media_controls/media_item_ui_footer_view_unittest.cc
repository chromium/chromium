// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_item_ui_footer_view.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_device_entry_ui.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/button_test_api.h"

using testing::NiceMock;

namespace {

// A mock class for handling stop casting button.
class StopCastingHandler {
 public:
  StopCastingHandler() = default;
  ~StopCastingHandler() = default;

  MOCK_METHOD(void, StopCasting, (), ());
};

// A mock class for delegating media notification footer view.
class MockFooterViewDelegate : public MediaItemUIFooterView::Delegate {
 public:
  MockFooterViewDelegate() = default;
  ~MockFooterViewDelegate() override = default;

  // MediaNotificationfooterview::Delegate.
  MOCK_METHOD(void, OnDropdownButtonClicked, (), (override));
  MOCK_METHOD(bool, IsDeviceSelectorExpanded, (), (override));
  MOCK_METHOD(void, OnDeviceSelected, (int), (override));
};

}  // namespace

class MediaItemUIFooterViewTest : public ChromeViewsTestBase {
 public:
  MediaItemUIFooterViewTest() = default;
  ~MediaItemUIFooterViewTest() override = default;

  // ChromeViewsTestBase
  void SetUp() override { ChromeViewsTestBase::SetUp(); }

  void TearDown() override {
    widget_.reset();
    handler_.reset();
    delegate_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void CreateView(bool is_cast_session) {
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    handler_ = std::make_unique<StopCastingHandler>();
    delegate_ = std::make_unique<NiceMock<MockFooterViewDelegate>>();

    base::RepeatingClosure stop_casting_callback =
        is_cast_session ? base::BindRepeating(&StopCastingHandler::StopCasting,
                                              base::Unretained(handler_.get()))
                        : base::NullCallback();

    view_ = widget_->SetContentsView(
        std::make_unique<MediaItemUIFooterView>(stop_casting_callback));
    view_->SetDelegate(delegate_.get());

    widget_->Show();
  }

  void SimulateButtonClicked(views::View* view) {
    views::test::ButtonTestApi(static_cast<views::Button*>(view))
        .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                    gfx::Point(), ui::EventTimeForNow(), 0, 0));
  }

  std::vector<views::View*> GetVisibleItems() {
    std::vector<views::View*> item;
    for (views::View* view : get_view()->children()) {
      if (view->GetVisible() && view->width() > 0)
        item.push_back(view);
    }
    return item;
  }

  views::Widget* get_widget() { return widget_.get(); }

  MediaItemUIFooterView* get_view() { return view_; }

  MockFooterViewDelegate* delegate() { return delegate_.get(); }

  StopCastingHandler* stop_casting_handler() { return handler_.get(); }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<StopCastingHandler> handler_;
  std::unique_ptr<MockFooterViewDelegate> delegate_;
  raw_ptr<MediaItemUIFooterView, DanglingUntriaged> view_ = nullptr;
};

TEST_F(MediaItemUIFooterViewTest, ViewDuringCast) {
  CreateView(true);
  EXPECT_EQ(get_view()->children().size(), 1u);

  EXPECT_CALL(*stop_casting_handler(), StopCasting());
  SimulateButtonClicked(get_view()->children()[0]);
}

TEST_F(MediaItemUIFooterViewTest, DevicesCanFit) {
  CreateView(false);
  get_widget()->SetBounds(gfx::Rect(200, 20));
  EXPECT_EQ(get_view()->children().size(), 0u);

  const std::string device1_name = "device1";
  AudioDeviceEntryView device1(views::Button::PressedCallback(), SK_ColorRED,
                               SK_ColorRED, "device", device1_name);
  device1.set_tag(0);

  const std::string device2_name = "device2";
  AudioDeviceEntryView device2(views::Button::PressedCallback(), SK_ColorRED,
                               SK_ColorRED, "device", device2_name);
  device2.set_tag(1);

  std::map<int, raw_ptr<DeviceEntryUI, CtnExperimental>> devices;
  devices[0] = &device1;
  devices[1] = &device2;

  get_view()->OnMediaItemUIDeviceSelectorUpdated(devices);
  get_widget()->LayoutRootViewIfNecessary();
  auto visible_items = GetVisibleItems();
  EXPECT_EQ(visible_items.size(), 2u);

  // Both devices should be visible in footer view.
  EXPECT_EQ(static_cast<views::LabelButton*>(visible_items[0])->GetText(),
            base::UTF8ToUTF16(device1_name));
  EXPECT_EQ(static_cast<views::LabelButton*>(visible_items[1])->GetText(),
            base::UTF8ToUTF16(device2_name));

  EXPECT_CALL(*delegate(), OnDeviceSelected(0));
  EXPECT_CALL(*delegate(), OnDeviceSelected(1));
  for (auto* view : visible_items)
    SimulateButtonClicked(view);
}

TEST_F(MediaItemUIFooterViewTest, OverflowButton) {
  CreateView(false);
  get_widget()->SetBounds(gfx::Rect(200, 20));
  EXPECT_EQ(get_view()->children().size(), 0u);

  const std::string device_name = "a very very long device name";
  AudioDeviceEntryView device(views::Button::PressedCallback(), SK_ColorRED,
                              SK_ColorRED, "device", device_name);

  std::map<int, raw_ptr<DeviceEntryUI, CtnExperimental>> devices;
  devices[0] = &device;
  devices[1] = &device;

  get_view()->OnMediaItemUIDeviceSelectorUpdated(devices);
  get_widget()->LayoutRootViewIfNecessary();
  auto visible_items = GetVisibleItems();
  EXPECT_EQ(visible_items.size(), 2u);

  // Only one device button could fit in, so the overflow button should show.
  EXPECT_EQ(static_cast<views::LabelButton*>(visible_items[0])->GetText(),
            base::UTF8ToUTF16(device_name));
  EXPECT_EQ(static_cast<views::LabelButton*>(visible_items[1])->GetText(),
            std::u16string());

  EXPECT_CALL(*delegate(), OnDropdownButtonClicked());
  SimulateButtonClicked(visible_items[1]);
}

TEST_F(MediaItemUIFooterViewTest, OverflowButtonFallback) {
  CreateView(false);
  get_widget()->SetBounds(gfx::Rect(310, 20));
  EXPECT_EQ(get_view()->children().size(), 0u);

  const std::string device_name = "a very very long device name";
  AudioDeviceEntryView device(views::Button::PressedCallback(), SK_ColorRED,
                              SK_ColorRED, "device", device_name);

  std::map<int, raw_ptr<DeviceEntryUI, CtnExperimental>> devices;
  devices[0] = &device;
  devices[1] = &device;

  // Two devices with 130px width should fit in the footer view (296px).
  get_view()->OnMediaItemUIDeviceSelectorUpdated(devices);
  get_widget()->LayoutRootViewIfNecessary();
  auto visible_items = GetVisibleItems();
  EXPECT_EQ(visible_items.size(), 2u);

  EXPECT_EQ(static_cast<views::LabelButton*>(visible_items[0])->GetText(),
            base::UTF8ToUTF16(device_name));
  EXPECT_EQ(static_cast<views::LabelButton*>(visible_items[1])->GetText(),
            base::UTF8ToUTF16(device_name));

  // Add another device should overflow the view, but we should remove one of
  // the two present devices because the overflow button won't fit in.
  // (130px device button + 8px spacing + 130px device button + 8px spacing +
  // 30px overflow button > 296px maximum footer size)
  devices[2] = &device;
  get_view()->OnMediaItemUIDeviceSelectorUpdated(devices);
  get_widget()->LayoutRootViewIfNecessary();
  visible_items = GetVisibleItems();
  EXPECT_EQ(visible_items.size(), 2u);

  EXPECT_EQ(static_cast<views::LabelButton*>(visible_items[1])->GetText(),
            std::u16string());
}
