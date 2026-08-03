// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_controller.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/test/browser_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/view_class_properties.h"

namespace send_tab_to_self {

namespace {

class StubSendTabToSelfBubbleController : public SendTabToSelfBubbleController {
 public:
  explicit StubSendTabToSelfBubbleController(content::WebContents* web_contents)
      : SendTabToSelfBubbleController(web_contents) {}
  ~StubSendTabToSelfBubbleController() override = default;

  std::optional<send_tab_to_self::EntryPointDisplayReason>
  GetEntryPointDisplayReason() override {
    return reason_;
  }

  void SetEntryPointDisplayReason(
      std::optional<send_tab_to_self::EntryPointDisplayReason> reason) {
    reason_ = reason;
  }

  std::vector<TargetDeviceInfo> GetValidDevices() override {
    const auto now = base::Time::Now();
    return {{"Device_1", "device_guid_1",
             syncer::DeviceInfo::FormFactor::kDesktop, now - base::Days(0)},
            {"Device_2", "device_guid_2",
             syncer::DeviceInfo::FormFactor::kPhone, now - base::Days(0)},
            {"Device_3", "device_guid_3",
             syncer::DeviceInfo::FormFactor::kDesktop, now - base::Days(1)},
            {"Device_4", "device_guid_4",
             syncer::DeviceInfo::FormFactor::kPhone, now - base::Days(1)},
            {"Device_5", "device_guid_5",
             syncer::DeviceInfo::FormFactor::kDesktop, now - base::Days(5)},
            {"Device_6", "device_guid_6",
             syncer::DeviceInfo::FormFactor::kPhone, now - base::Days(5)}};
  }

  AccountInfo GetSharingAccountInfo() override {
    AccountInfo info;
    info.email = "user@host.com";
    info.account_image = gfx::Image(gfx::test::CreateImageSkia(96, 96));
    return info;
  }

 private:
  std::optional<send_tab_to_self::EntryPointDisplayReason> reason_ =
      send_tab_to_self::EntryPointDisplayReason::kOfferFeature;
};

}  // namespace

class SendTabToSelfBubbleTest : public DialogBrowserTest {
 public:
  SendTabToSelfBubbleTest() = default;

  SendTabToSelfBubbleTest(const SendTabToSelfBubbleTest&) = delete;
  SendTabToSelfBubbleTest& operator=(const SendTabToSelfBubbleTest&) = delete;

  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::unique_ptr<StubSendTabToSelfBubbleController> controller =
        std::make_unique<StubSendTabToSelfBubbleController>(web_contents);
    controller_ = controller.get();
    web_contents->SetUserData(StubSendTabToSelfBubbleController::UserDataKey(),
                              std::move(controller));
  }

  void TearDownOnMainThread() override {
    controller_ = nullptr;
    DialogBrowserTest::TearDownOnMainThread();
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    if (name == "ShowDeviceList") {
      controller_->SetEntryPointDisplayReason(
          send_tab_to_self::EntryPointDisplayReason::kOfferFeature);
    } else if (name == "ShowSigninPromo") {
      controller_->SetEntryPointDisplayReason(
          send_tab_to_self::EntryPointDisplayReason::kOfferSignIn);
    } else if (name == "ShowVerifyPromo") {
      controller_->SetEntryPointDisplayReason(
          send_tab_to_self::EntryPointDisplayReason::kOfferReauth);
    } else {
      CHECK_EQ(name, "ShowNoTargetDevicePromo");
      controller_->SetEntryPointDisplayReason(
          send_tab_to_self::EntryPointDisplayReason::kInformNoTargetDevice);
    }
    controller_->ShowBubble(ShareEntryPoint::kToolbarIcon);
  }

 protected:
  raw_ptr<StubSendTabToSelfBubbleController> controller_ = nullptr;
};

// Test suite for pixel/dialog tests that assert the old bubble UI.
// These tests must run with SendTabToSelfEnhancedDesktopUI disabled to match
// the existing pixel baselines.
class SendTabToSelfBubbleOldUITest : public SendTabToSelfBubbleTest {
 public:
  SendTabToSelfBubbleOldUITest() {
    feature_list_.InitAndDisableFeature(kSendTabToSelfEnhancedDesktopUI);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleOldUITest, InvokeUi_ShowDeviceList) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleOldUITest, InvokeUi_ShowSigninPromo) {
  // Last updated in crrev.com/c/3776623.
  set_baseline("3776623");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleOldUITest,
                       InvokeUi_ShowNoTargetDevicePromo) {
  // Last updated in crrev.com/c/3832669.
  set_baseline("3832669");
  ShowAndVerifyUi();
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class SendTabToSelfBubbleEnhancedUITest : public SendTabToSelfBubbleTest {
 public:
  SendTabToSelfBubbleEnhancedUITest() {
    feature_list_.InitAndEnableFeature(kSendTabToSelfEnhancedDesktopUI);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleEnhancedUITest,
                       InvokeUi_ShowSigninPromo) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleEnhancedUITest,
                       InvokeUi_ShowVerifyPromo) {
  ShowAndVerifyUi();
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

class SendTabToSelfBubbleParameterizedTest
    : public SendTabToSelfBubbleTest,
      public testing::WithParamInterface<EntryPointDisplayReason> {
 public:
  SendTabToSelfBubbleParameterizedTest() {
    if (GetParam() == EntryPointDisplayReason::kOfferReauth) {
      feature_list_.InitAndEnableFeature(kSendTabToSelfEnhancedDesktopUI);
    }
  }

  void SetUpOnMainThread() override {
    SendTabToSelfBubbleTest::SetUpOnMainThread();
    controller_->SetEntryPointDisplayReason(GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SendTabToSelfBubbleParameterizedTest,
    testing::Values(EntryPointDisplayReason::kOfferFeature,
                    EntryPointDisplayReason::kOfferSignIn,
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
                    EntryPointDisplayReason::kOfferReauth,
#endif
                    EntryPointDisplayReason::kInformNoTargetDevice));

IN_PROC_BROWSER_TEST_P(SendTabToSelfBubbleParameterizedTest,
                       BubbleTriggersCorrectlyWhenPinned) {
  // Pin send tab to self to the toolbar.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  actions::ActionItem* browser_action_item =
      BrowserActions::From(browser())->root_action_item();
  auto* action_item = actions::ActionManager::Get().FindAction(
      kActionSendTabToSelf, browser_action_item);
  action_item->SetEnabled(true);
  action_item->SetVisible(true);
  PinnedToolbarActionsContainer* container =
      static_cast<PinnedToolbarActionsContainer*>(
          browser_view->toolbar_button_provider()->GetPinnedToolbarActions());
  container->UpdateActionState(kActionSendTabToSelf, true);
  views::test::WaitForAnimatingLayoutManager(container);

  // Find the send tab to self button.
  PinnedActionToolbarButton* send_tab_to_self_button = nullptr;
  for (views::View* child : container->children()) {
    if (views::Button::AsButton(child)) {
      PinnedActionToolbarButton* button =
          static_cast<PinnedActionToolbarButton*>(child);
      if (button->GetActionId() == kActionSendTabToSelf) {
        send_tab_to_self_button = button;
        break;
      }
    }
  }
  EXPECT_NE(send_tab_to_self_button, nullptr);
  EXPECT_EQ(send_tab_to_self_button->GetProperty(views::kElementIdentifierKey),
            kPinnedToolbarActionSendTabToSelfElementId);

  // Simulate triggering the bubble and confirm it is shown.
  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  ui::MouseEvent release_event(ui::EventType::kMouseReleased, gfx::Point(),
                               gfx::Point(), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  controller_->ShowBubble(ShareEntryPoint::kToolbarIcon);
  EXPECT_TRUE(controller_->IsBubbleShown());

  // Confirm execution is skipped for the next mouse release.
  send_tab_to_self_button->OnMousePressed(press_event);
  EXPECT_TRUE(send_tab_to_self_button->ShouldSkipExecutionForTesting());
  send_tab_to_self_button->OnMouseReleased(release_event);
  EXPECT_FALSE(send_tab_to_self_button->ShouldSkipExecutionForTesting());
}

IN_PROC_BROWSER_TEST_P(SendTabToSelfBubbleParameterizedTest,
                       BubbleTriggersCorrectlyWhenNotPinned) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  PinnedToolbarActionsContainer* container =
      static_cast<PinnedToolbarActionsContainer*>(
          browser_view->toolbar_button_provider()->GetPinnedToolbarActions());

  // Ensure it is not pinned or popped out.
  ASSERT_FALSE(container->IsActionPinnedOrPoppedOut(kActionSendTabToSelf));

  // Trigger the bubble.
  controller_->ShowBubble(ShareEntryPoint::kToolbarIcon);
  EXPECT_TRUE(controller_->IsBubbleShown());

  // Confirm it is now popped out.
  EXPECT_TRUE(container->IsActionPoppedOut(kActionSendTabToSelf));

  // Find the send tab to self button.
  PinnedActionToolbarButton* send_tab_to_self_button =
      container->GetButtonFor(kActionSendTabToSelf);
  ASSERT_TRUE(send_tab_to_self_button);
  EXPECT_TRUE(send_tab_to_self_button->GetVisible());

  // Confirm the bubble is anchored to the button.
  auto* bubble_delegate = controller_->send_tab_to_self_bubble_view();
  EXPECT_EQ(bubble_delegate->GetAnchorView(), send_tab_to_self_button);
  EXPECT_EQ(send_tab_to_self_button->GetProperty(views::kElementIdentifierKey),
            kPinnedToolbarActionSendTabToSelfElementId);

  // Close the bubble.
  bubble_delegate->GetWidget()->CloseNow();
  EXPECT_FALSE(controller_->IsBubbleShown());

  // Confirm it is no longer popped out.
  EXPECT_FALSE(container->IsActionPoppedOut(kActionSendTabToSelf));
}

IN_PROC_BROWSER_TEST_P(SendTabToSelfBubbleParameterizedTest,
                       ShowBubbleMultipleTimes) {
  // Call ShowBubble multiple times. The early return prevents re-creation.
  controller_->ShowBubble(ShareEntryPoint::kToolbarIcon);
  SendTabToSelfBubbleView* first_view =
      controller_->send_tab_to_self_bubble_view();
  EXPECT_TRUE(first_view);

  controller_->ShowBubble(ShareEntryPoint::kToolbarIcon);
  EXPECT_EQ(first_view, controller_->send_tab_to_self_bubble_view());
}

}  // namespace send_tab_to_self
