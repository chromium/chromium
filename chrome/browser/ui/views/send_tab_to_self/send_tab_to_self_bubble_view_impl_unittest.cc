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

  std::vector<TargetDeviceInfo> GetValidDevices() const override {
    base::SimpleTestClock clock;
    return {{"Device_1", "Device_1", "device_guid_1",
             sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
             clock.Now() - base::TimeDelta::FromDays(0)},
            {"Device_2", "Device_2", "device_guid_2",
             sync_pb::SyncEnums_DeviceType_TYPE_WIN,
             clock.Now() - base::TimeDelta::FromDays(1)},
            {"Device_3", "Device_3", "device_guid_3",
             sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
             clock.Now() - base::TimeDelta::FromDays(5)}};
  }

  MOCK_METHOD2(OnDeviceSelected,
               void(const std::string& target_device_name,
                    const std::string& target_device_guid));
};

}  // namespace

class SendTabToSelfBubbleViewImplTest : public ChromeViewsTestBase {
 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // Create an anchor for the bubble.
    anchor_widget_ = CreateTestWidget(views::Widget::InitParams::TYPE_WINDOW);

    profile_ = std::make_unique<TestingProfile>();
    controller_ = std::make_unique<SendTabToSelfBubbleControllerMock>();
    bubble_ = new SendTabToSelfBubbleViewImpl(anchor_widget_->GetContentsView(),
                                              nullptr, controller_.get());
    views::BubbleDialogDelegateView::CreateBubble(bubble_);
  }

  void TearDown() override {
    bubble_->GetWidget()->CloseNow();
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<views::Widget> anchor_widget_;
  std::unique_ptr<SendTabToSelfBubbleControllerMock> controller_;
  SendTabToSelfBubbleViewImpl* bubble_;
};

TEST_F(SendTabToSelfBubbleViewImplTest, KeyboardAccessibilityConfigured) {
  auto* container = bubble_->GetButtonContainerForTesting();

  ASSERT_EQ(3U, container->children().size());

  // All three device entries should be grouped together, and the first one
  // should receive initial keyboard focus.
  EXPECT_EQ(container->children()[0], bubble_->GetInitiallyFocusedView());
  EXPECT_NE(-1, container->children()[0]->GetGroup());
  EXPECT_EQ(container->children()[0]->GetGroup(),
            container->children()[1]->GetGroup());
  EXPECT_EQ(container->children()[0]->GetGroup(),
            container->children()[2]->GetGroup());
}

TEST_F(SendTabToSelfBubbleViewImplTest, ButtonPressed) {
  EXPECT_CALL(*controller_.get(),
              OnDeviceSelected("Device_3", "device_guid_3"));
  const views::View* button_container = bubble_->GetButtonContainerForTesting();
  ASSERT_EQ(3U, button_container->children().size());
  bubble_->DeviceButtonPressed(static_cast<SendTabToSelfBubbleDeviceButton*>(
      button_container->children()[2]));
}

}  // namespace send_tab_to_self
