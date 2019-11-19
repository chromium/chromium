// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view_impl.h"

#include <string>
#include <vector>

#include "base/test/simple_test_clock.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_device_button.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace send_tab_to_self {

namespace {

class SendTabToSelfBubbleControllerMock : public SendTabToSelfBubbleController {
 public:
  SendTabToSelfBubbleControllerMock() = default;
  ~SendTabToSelfBubbleControllerMock() override = default;

  MOCK_METHOD2(OnDeviceSelected,
               void(const std::string& target_device_name,
                    const std::string& target_device_guid));
};

class SendTabToSelfBubbleViewImplMock : public SendTabToSelfBubbleViewImpl {
 public:
  SendTabToSelfBubbleViewImplMock(views::View* anchor_view,
                                  content::WebContents* web_contents,
                                  SendTabToSelfBubbleController* controller)
      : SendTabToSelfBubbleViewImpl(anchor_view,
                                    web_contents,
                                    controller) {}
  ~SendTabToSelfBubbleViewImplMock() override = default;

  // The delegate cannot find widget since it is created from a null profile.
  // This method will be called inside DevicePressed(). Unit tests will
  // chrash without mocking.
  MOCK_METHOD0(CloseBubble, void());
};

}  // namespace

class SendTabToSelfBubbleViewImplTest : public ChromeViewsTestBase {
 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // Create an anchor for the bubble.
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    anchor_widget_ = std::make_unique<views::Widget>();
    anchor_widget_->Init(std::move(params));

    profile_ = std::make_unique<TestingProfile>();
    controller_ = std::make_unique<SendTabToSelfBubbleControllerMock>();
    bubble_ = std::make_unique<SendTabToSelfBubbleViewImplMock>(
        anchor_widget_->GetContentsView(), nullptr, controller_.get());
  }

  void TearDown() override {
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  const std::vector<TargetDeviceInfo> SetUpDeviceList() {
    base::SimpleTestClock clock;
    std::vector<TargetDeviceInfo> list;
    TargetDeviceInfo valid_device_1(
        "Device_1", "device_guid_1", sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
        /*last_updated_timestamp=*/clock.Now() - base::TimeDelta::FromDays(0));
    TargetDeviceInfo valid_device_2(
        "Device_2", "device_guid_2", sync_pb::SyncEnums_DeviceType_TYPE_WIN,
        /*last_updated_timestamp=*/clock.Now() - base::TimeDelta::FromDays(1));
    TargetDeviceInfo valid_device_3(
        "Device_3", "device_guid_3", sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
        /*last_updated_timestamp=*/clock.Now() - base::TimeDelta::FromDays(5));
    list.push_back(valid_device_1);
    list.push_back(valid_device_2);
    list.push_back(valid_device_3);
    return list;
  }

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<views::Widget> anchor_widget_;
  std::unique_ptr<SendTabToSelfBubbleControllerMock> controller_;
  std::unique_ptr<SendTabToSelfBubbleViewImpl> bubble_;
};

TEST_F(SendTabToSelfBubbleViewImplTest, PopulateScrollView) {
  bubble_->CreateScrollView();
  bubble_->PopulateScrollView(SetUpDeviceList());
  EXPECT_EQ(3UL, bubble_->GetDeviceButtonsForTest().size());
}

TEST_F(SendTabToSelfBubbleViewImplTest, DevicePressed) {
  bubble_->Init();
  bubble_->PopulateScrollView(SetUpDeviceList());
  EXPECT_CALL(*controller_.get(),
              OnDeviceSelected("Device_3", "device_guid_3"));
  bubble_->DevicePressed(2);
}

}  // namespace send_tab_to_self
