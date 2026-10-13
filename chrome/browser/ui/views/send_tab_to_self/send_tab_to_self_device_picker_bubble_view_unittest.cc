// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_device_picker_bubble_view.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_device_button.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/sync_device_info/remote_device_info.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_utils.h"

namespace send_tab_to_self {

namespace {

using FormFactor = syncer::DeviceInfo::FormFactor;
using OsType = syncer::DeviceInfo::OsType;
using syncer::RemoteDeviceInfo;

constexpr char kDeviceGuid3[] = "device_guid_3";
constexpr char kDeviceName3[] = "Device_3";

class SendTabToSelfBubbleControllerMock : public SendTabToSelfBubbleController {
 public:
  explicit SendTabToSelfBubbleControllerMock(content::WebContents* web_contents)
      : SendTabToSelfBubbleController(web_contents) {}

  ~SendTabToSelfBubbleControllerMock() override = default;

  std::vector<RemoteDeviceInfo> GetValidDevices() override {
    base::SimpleTestClock clock;
    return {{"Device_1", "device_guid_1", FormFactor::kDesktop, OsType::kLinux,
             clock.Now() - base::Days(0),
             /*has_high_precision_timestamp=*/false},
            {"Device_2", "device_guid_2", FormFactor::kDesktop, OsType::kLinux,
             clock.Now() - base::Days(1),
             /*has_high_precision_timestamp=*/false},
            {kDeviceName3, kDeviceGuid3, FormFactor::kPhone, OsType::kAndroid,
             clock.Now() - base::Days(5),
             /*has_high_precision_timestamp=*/false}};
  }

  MOCK_METHOD(void,
              OnDeviceSelected,
              (const std::string& target_device_guid,
               std::string_view device_name),
              (override));
};

bool IsAccessibleNodeSelected(const views::View* view) {
  ui::AXNodeData node_data;
  view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  return node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
}

ax::mojom::Role GetAccessibleNodeRole(const views::View* view) {
  ui::AXNodeData node_data;
  view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  return node_data.role;
}

}  // namespace

class SendTabToSelfDevicePickerBubbleViewTest : public ChromeViewsTestBase {
 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // Create an anchor for the bubble.
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    // Owned by WebContents.
    auto controller = std::make_unique<SendTabToSelfBubbleControllerMock>(
        web_contents_.get());
    controller_ = controller.get();
    web_contents_->SetUserData(SendTabToSelfBubbleControllerMock::UserDataKey(),
                               std::move(controller));

    bubble_ = new SendTabToSelfDevicePickerBubbleView(
        views::BubbleAnchor(anchor_widget_->GetContentsView()),
        web_contents_.get());
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

// Verifies that the first target device button in the list of options receives
// initial keyboard focus when the dialog opens, and that all device buttons
// share a group for arrow-key navigation.
TEST_F(SendTabToSelfDevicePickerBubbleViewTest,
       InitiallyFocusedViewIsFirstDevice) {
  const views::View* container = bubble_->GetButtonContainerForTesting();
  ASSERT_EQ(3U, container->children().size());

  // The first device entry should be the initially focused view for screen
  // readers and keyboard navigation when the dialog opens, and all device
  // entries should be grouped together.
  EXPECT_EQ(container->children()[0], bubble_->GetInitiallyFocusedView());
  EXPECT_NE(-1, container->children()[0]->GetGroup());
  EXPECT_EQ(container->children()[0]->GetGroup(),
            container->children()[1]->GetGroup());
  EXPECT_EQ(container->children()[0]->GetGroup(),
            container->children()[2]->GetGroup());
}

// Verifies that clicking a target device button selects it and accepting the
// dialog notifies the controller with the selected device.
TEST_F(SendTabToSelfDevicePickerBubbleViewTest,
       SelectDeviceAndAcceptNotifiesController) {
  EXPECT_CALL(*controller_, OnDeviceSelected).Times(0);
  const views::View* button_container = bubble_->GetButtonContainerForTesting();
  ASSERT_EQ(3U, button_container->children().size());

  auto* third_button = views::AsViewClass<SendTabToSelfBubbleDeviceButton>(
      button_container->children()[2]);
  views::test::ButtonTestApi(third_button).NotifyDefaultMouseClick();
  EXPECT_TRUE(third_button->IsSelected());

  EXPECT_CALL(*controller_, OnDeviceSelected(kDeviceGuid3, kDeviceName3));
  bubble_->AcceptDialog();
}

// Verifies that the first target device button is automatically selected on
// open and its accessibility node data indicates that it is selected and has
// listbox option roles for Windows AT compatibility.
TEST_F(SendTabToSelfDevicePickerBubbleViewTest,
       FirstDeviceIsSelectedAndAccessibleOnOpen) {
  const views::View* container = bubble_->GetButtonContainerForTesting();
  ASSERT_EQ(3U, container->children().size());

  auto* first_button = views::AsViewClass<SendTabToSelfBubbleDeviceButton>(
      container->children()[0]);
  auto* second_button = views::AsViewClass<SendTabToSelfBubbleDeviceButton>(
      container->children()[1]);

  EXPECT_TRUE(first_button->IsSelected());
  EXPECT_FALSE(second_button->IsSelected());

  // Verify that the accessible roles and selected state are exposed correctly
  // for screen readers on all platforms.
  EXPECT_EQ(ax::mojom::Role::kListBox, GetAccessibleNodeRole(container));
  EXPECT_EQ(ax::mojom::Role::kListBoxOption,
            GetAccessibleNodeRole(first_button));
  EXPECT_EQ(ax::mojom::Role::kListBoxOption,
            GetAccessibleNodeRole(second_button));
  EXPECT_TRUE(IsAccessibleNodeSelected(first_button));
  EXPECT_FALSE(IsAccessibleNodeSelected(second_button));
}

// Verifies that selecting a different target device updates the selected state
// and accessibility node data for both the newly selected and previously
// selected target device buttons.
TEST_F(SendTabToSelfDevicePickerBubbleViewTest,
       SelectTargetDeviceUpdatesAccessibilitySelection) {
  const views::View* container = bubble_->GetButtonContainerForTesting();
  ASSERT_EQ(3U, container->children().size());

  auto* first_button = views::AsViewClass<SendTabToSelfBubbleDeviceButton>(
      container->children()[0]);
  auto* second_button = views::AsViewClass<SendTabToSelfBubbleDeviceButton>(
      container->children()[1]);

  EXPECT_TRUE(first_button->IsSelected());
  EXPECT_FALSE(second_button->IsSelected());
  EXPECT_EQ(ax::mojom::Role::kListBox, GetAccessibleNodeRole(container));
  EXPECT_EQ(ax::mojom::Role::kListBoxOption,
            GetAccessibleNodeRole(first_button));
  EXPECT_EQ(ax::mojom::Role::kListBoxOption,
            GetAccessibleNodeRole(second_button));
  EXPECT_TRUE(IsAccessibleNodeSelected(first_button));
  EXPECT_FALSE(IsAccessibleNodeSelected(second_button));

  // Select the second device button.
  bubble_->SelectTargetDevice(second_button);

  EXPECT_FALSE(first_button->IsSelected());
  EXPECT_TRUE(second_button->IsSelected());

  // Verify accessibility attributes after selection change.
  EXPECT_EQ(ax::mojom::Role::kListBoxOption,
            GetAccessibleNodeRole(first_button));
  EXPECT_EQ(ax::mojom::Role::kListBoxOption,
            GetAccessibleNodeRole(second_button));
  EXPECT_FALSE(IsAccessibleNodeSelected(first_button));
  EXPECT_TRUE(IsAccessibleNodeSelected(second_button));
}

}  // namespace send_tab_to_self
