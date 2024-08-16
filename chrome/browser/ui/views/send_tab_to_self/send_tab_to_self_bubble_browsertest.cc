// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/test/browser_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"

namespace send_tab_to_self {

namespace {

class TestSendTabToSelfBubbleController : public SendTabToSelfBubbleController {
 public:
  explicit TestSendTabToSelfBubbleController(content::WebContents* web_contents)
      : SendTabToSelfBubbleController(web_contents) {}
  ~TestSendTabToSelfBubbleController() override = default;

  std::optional<send_tab_to_self::EntryPointDisplayReason>
  GetEntryPointDisplayReason() override {
    return send_tab_to_self::EntryPointDisplayReason::kOfferFeature;
  }

  std::vector<TargetDeviceInfo> GetValidDevices() override {
    const auto now = base::Time::Now();
    return {{"Device_1", "Device_1", "device_guid_1",
             syncer::DeviceInfo::FormFactor::kDesktop, now - base::Days(0)},
            {"Device_2", "Device_2", "device_guid_2",
             syncer::DeviceInfo::FormFactor::kPhone, now - base::Days(0)},
            {"Device_3", "Device_3", "device_guid_3",
             syncer::DeviceInfo::FormFactor::kDesktop, now - base::Days(1)},
            {"Device_4", "Device_4", "device_guid_4",
             syncer::DeviceInfo::FormFactor::kPhone, now - base::Days(1)},
            {"Device_5", "Device_5", "device_guid_5",
             syncer::DeviceInfo::FormFactor::kDesktop, now - base::Days(5)},
            {"Device_6", "Device_6", "device_guid_6",
             syncer::DeviceInfo::FormFactor::kPhone, now - base::Days(5)}};
  }

  AccountInfo GetSharingAccountInfo() override {
    AccountInfo info;
    info.email = "user@host.com";
    info.account_image = gfx::Image(gfx::test::CreateImageSkia(96, 96));
    return info;
  }
};

}  // namespace

class SendTabToSelfBubbleTest : public DialogBrowserTest {
 public:
  SendTabToSelfBubbleTest() = default;

  SendTabToSelfBubbleTest(const SendTabToSelfBubbleTest&) = delete;
  SendTabToSelfBubbleTest& operator=(const SendTabToSelfBubbleTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    web_contents->SetUserData(
        TestSendTabToSelfBubbleController::UserDataKey(),
        std::make_unique<TestSendTabToSelfBubbleController>(web_contents));

    if (name == "ShowDeviceList") {
      BrowserView::GetBrowserViewForBrowser(browser())
          ->ShowSendTabToSelfDevicePickerBubble(web_contents);
    } else if (name == "ShowSigninPromo") {
      BrowserView::GetBrowserViewForBrowser(browser())
          ->ShowSendTabToSelfPromoBubble(web_contents,
                                         /*show_signin_button=*/true);
    } else {
      DCHECK_EQ(name, "ShowNoTargetDevicePromo");
      BrowserView::GetBrowserViewForBrowser(browser())
          ->ShowSendTabToSelfPromoBubble(web_contents,
                                         /*show_signin_button=*/false);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_{features::kToolbarPinning};
};

// TODO(crbug.com/40927205): Flakily fails on some Windows builders.
#if BUILDFLAG(IS_WIN)
#define MAYBE_InvokeUi_ShowDeviceList DISABLED_InvokeUi_ShowDeviceList
#else
#define MAYBE_InvokeUi_ShowDeviceList InvokeUi_ShowDeviceList
#endif
IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleTest, MAYBE_InvokeUi_ShowDeviceList) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleTest, InvokeUi_ShowSigninPromo) {
  // Last updated in crrev.com/c/3776623.
  set_baseline("3776623");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleTest,
                       InvokeUi_ShowNoTargetDevicePromo) {
  // Last updated in crrev.com/c/3832669.
  set_baseline("3832669");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleTest,
                       BubbleTriggersCorrectlyWhenPinned) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<TestSendTabToSelfBubbleController> unique_controller =
      std::make_unique<TestSendTabToSelfBubbleController>(web_contents);
  TestSendTabToSelfBubbleController* controller = unique_controller.get();
  web_contents->SetUserData(TestSendTabToSelfBubbleController::UserDataKey(),
                            std::move(unique_controller));

  // Pin send tab to self to the toolbar.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  actions::ActionItem* browser_action_item =
      browser()->browser_actions()->root_action_item();
  auto* action_item = actions::ActionManager::Get().FindAction(
      kActionSendTabToSelf, browser_action_item);
  action_item->SetEnabled(true);
  action_item->SetVisible(true);
  PinnedToolbarActionsContainer* container =
      browser_view->toolbar()->pinned_toolbar_actions_container();
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

  // Simulate triggering the bubble and confirm it is shown.
  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  ui::MouseEvent release_event(ui::EventType::kMouseReleased, gfx::Point(),
                               gfx::Point(), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  controller->ShowBubble();
  EXPECT_TRUE(controller->IsBubbleShown());

  // Confirm execution is skipped for the next mouse release.
  send_tab_to_self_button->OnMousePressed(press_event);
  EXPECT_TRUE(send_tab_to_self_button->ShouldSkipExecutionForTesting());
  send_tab_to_self_button->OnMouseReleased(release_event);
  EXPECT_FALSE(send_tab_to_self_button->ShouldSkipExecutionForTesting());
}

}  // namespace send_tab_to_self
