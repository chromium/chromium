// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_device_picker_bubble_view.h"

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_device_button.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace send_tab_to_self {

namespace {

class SendTabToSelfBubbleControllerMock : public SendTabToSelfBubbleController {
 public:
  explicit SendTabToSelfBubbleControllerMock(content::WebContents* web_contents)
      : SendTabToSelfBubbleController(web_contents) {}

  ~SendTabToSelfBubbleControllerMock() override = default;

  std::vector<TargetDeviceInfo> GetValidDevices() override {
    base::SimpleTestClock clock;
    return {
        {"Device_1", "Device_1", "device_guid_1",
         syncer::DeviceInfo::FormFactor::kDesktop, clock.Now() - base::Days(0)},
        {"Device_2", "Device_2", "device_guid_2",
         syncer::DeviceInfo::FormFactor::kDesktop, clock.Now() - base::Days(1)},
        {"Device_3", "Device_3", "device_guid_3",
         syncer::DeviceInfo::FormFactor::kPhone, clock.Now() - base::Days(5)}};
  }

  AccountInfo GetSharingAccountInfo() override {
    AccountInfo info;
    info.email = "user@host.com";
    info.account_image = gfx::Image(gfx::test::CreateImageSkia(96, 96));
    return info;
  }

  MOCK_METHOD(void,
              OnDeviceSelected,
              (const std::string& target_device_guid),
              (override));
};

}  // namespace

class SendTabToSelfDevicePickerBubbleViewTest : public ChromeViewsTestBase {
 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // Create an anchor for the bubble.
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    // Owned by WebContents.
    controller_ = new SendTabToSelfBubbleControllerMock(web_contents_.get());
    web_contents_->SetUserData(SendTabToSelfBubbleControllerMock::UserDataKey(),
                               base::WrapUnique(controller_.get()));

    bubble_ = new SendTabToSelfDevicePickerBubbleView(
        anchor_widget_->GetContentsView(), web_contents_.get());
    views::BubbleDialogDelegateView::CreateBubble(bubble_);
  }

  void TearDown() override {
    bubble_->GetWidget()->CloseNow();
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  TestingProfile profile_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<SendTabToSelfDevicePickerBubbleView, DanglingUntriaged> bubble_;
  // Owned by WebContents.
  raw_ptr<SendTabToSelfBubbleControllerMock> controller_;
};

TEST_F(SendTabToSelfDevicePickerBubbleViewTest,
       KeyboardAccessibilityConfigured) {
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

TEST_F(SendTabToSelfDevicePickerBubbleViewTest, ButtonPressed) {
  EXPECT_CALL(*controller_, OnDeviceSelected("device_guid_3"));
  const views::View* button_container = bubble_->GetButtonContainerForTesting();
  ASSERT_EQ(3U, button_container->children().size());
  bubble_->DeviceButtonPressed(static_cast<SendTabToSelfBubbleDeviceButton*>(
      button_container->children()[2]));
}

}  // namespace send_tab_to_self
