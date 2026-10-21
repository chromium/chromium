// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_test_accessor.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync_device_info/remote_device_info.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_id.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/view_class_properties.h"

namespace send_tab_to_self {

class SendTabToSelfBubbleTest;

namespace {

using FormFactor = syncer::DeviceInfo::FormFactor;
using OsType = syncer::DeviceInfo::OsType;
// Test suite for dialog tests that do not have Skia Gold baselines enabled via
// `*SendTabToSelfBubbleTest.*` in `pixel_tests.filter`.
using SendTabToSelfBubbleDialogTest = SendTabToSelfBubbleTest;
using syncer::RemoteDeviceInfo;
using ::testing::Values;
using ::testing::WithParamInterface;

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

  std::vector<RemoteDeviceInfo> GetValidDevices() override {
    const auto now = base::Time::Now();
    return {{"Device_1", "device_guid_1", FormFactor::kDesktop, OsType::kLinux,
             now - base::Days(0), /*has_high_precision_timestamp=*/false},
            {"Device_2", "device_guid_2", FormFactor::kPhone, OsType::kAndroid,
             now - base::Days(0), /*has_high_precision_timestamp=*/false},
            {"Device_3", "device_guid_3", FormFactor::kDesktop, OsType::kLinux,
             now - base::Days(1), /*has_high_precision_timestamp=*/false},
            {"Device_4", "device_guid_4", FormFactor::kPhone, OsType::kAndroid,
             now - base::Days(1), /*has_high_precision_timestamp=*/false},
            {"Device_5", "device_guid_5", FormFactor::kDesktop, OsType::kLinux,
             now - base::Days(5), /*has_high_precision_timestamp=*/false},
            {"Device_6", "device_guid_6", FormFactor::kPhone, OsType::kAndroid,
             now - base::Days(5), /*has_high_precision_timestamp=*/false}};
  }

  AccountInfo GetSharingAccountInfo() override {
    return AccountInfo::Builder(GaiaId("test_gaia"), "user@host.com")
        .SetAvatarImage(gfx::Image(gfx::test::CreateImageSkia(96, 96)))
        .Build();
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

  void SetUpInProcessBrowserTestFixture() override {
    DialogBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &SendTabToSelfBubbleTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<StubSendTabToSelfSyncService>();
        }));
  }

  void SetUpOnMainThread() override {
    DialogBrowserTest::SetUpOnMainThread();

    content::WebContents* web_contents =
        browser()->GetTabStripModel()->GetActiveWebContents();
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
    ShowToolbarBubble();
  }

  void ShowToolbarBubble() {
    controller_->ShowBubble(ShareEntryPoint::kToolbarIcon);
    if (!controller_->IsBubbleShown()) {
      PinnedActionTestAccessor::WaitForAnimation(browser());
    }
  }

 protected:
  raw_ptr<StubSendTabToSelfBubbleController> controller_ = nullptr;
  base::CallbackListSubscription create_services_subscription_;
};

// Verifies that the no-target-device promo bubble renders and matches the
// baseline screenshot.
IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleTest,
                       InvokeUi_ShowNoTargetDevicePromo) {
  // Last updated in crrev.com/c/3832669.
  set_baseline("3832669");
  ShowAndVerifyUi();
}

// Verifies that the device picker bubble opens and closes cleanly.
IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleDialogTest, InvokeUi_ShowDeviceList) {
  ShowAndVerifyUi();
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Verifies that the sign-in promo bubble opens and closes cleanly.
IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleDialogTest,
                       InvokeUi_ShowSigninPromo) {
  ShowAndVerifyUi();
}

// Verifies that the reauth promo bubble opens and closes cleanly.
IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleDialogTest,
                       InvokeUi_ShowVerifyPromo) {
  ShowAndVerifyUi();
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

class SendTabToSelfBubbleParameterizedTest
    : public SendTabToSelfBubbleTest,
      public WithParamInterface<EntryPointDisplayReason> {
 public:
  SendTabToSelfBubbleParameterizedTest() = default;

  void SetUpOnMainThread() override {
    SendTabToSelfBubbleTest::SetUpOnMainThread();
    controller_->SetEntryPointDisplayReason(GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SendTabToSelfBubbleParameterizedTest,
    Values(EntryPointDisplayReason::kOfferFeature,
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
           EntryPointDisplayReason::kOfferSignIn,
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
  PinnedToolbarActions* container =
      browser_view->toolbar_button_provider()->GetPinnedToolbarActions();
  container->UpdateActionState(kActionSendTabToSelf, true);
  PinnedActionTestAccessor::WaitForAnimation(browser());

  // Find the send tab to self button.
  PinnedActionTestAccessor accessor(browser(), kActionSendTabToSelf);
  EXPECT_TRUE(accessor.GetVisible());
  EXPECT_TRUE(accessor.GetEnabled());
  ASSERT_TRUE(accessor.GetElement());
  EXPECT_EQ(accessor.GetElement()->identifier(),
            kPinnedToolbarActionSendTabToSelfElementId);

  // Clicking the button triggers the bubble and confirm it is shown.
  EXPECT_EQ(action_item->GetInvokeCount(), 0);
  accessor.Click();
  ASSERT_TRUE(controller_->IsBubbleShown());
  controller_->send_tab_to_self_bubble_view()->set_close_on_deactivate(false);
  EXPECT_TRUE(action_item->GetIsShowingBubble());
  EXPECT_EQ(action_item->GetInvokeCount(), 1);

  // Clicking the button while the bubble is shown skips executing the action
  // rather than re-triggering the bubble.
  accessor.Click();
  EXPECT_EQ(action_item->GetInvokeCount(), 1);
  EXPECT_TRUE(controller_->IsBubbleShown());
}

IN_PROC_BROWSER_TEST_P(SendTabToSelfBubbleParameterizedTest,
                       BubbleTriggersCorrectlyWhenNotPinned) {
  PinnedActionTestAccessor accessor(browser(), kActionSendTabToSelf);

  // Ensure it is not pinned or popped out.
  ASSERT_FALSE(accessor.IsPinnedOrPoppedOut());

  // Trigger the bubble.
  ShowToolbarBubble();
  EXPECT_TRUE(controller_->IsBubbleShown());

  // Confirm it is now popped out.
  EXPECT_TRUE(accessor.IsPoppedOut());
  EXPECT_TRUE(accessor.GetVisible());

  // Confirm the bubble is anchored to the button.
  auto* bubble_delegate = controller_->send_tab_to_self_bubble_view();
  ASSERT_TRUE(bubble_delegate);
  EXPECT_FALSE(accessor.GetBubbleAnchor().IsNull());
  EXPECT_TRUE(bubble_delegate->IsSameAnchor(accessor.GetBubbleAnchor()));

  // Close the bubble.
  bubble_delegate->GetWidget()->CloseNow();
  EXPECT_FALSE(controller_->IsBubbleShown());

  // Confirm it is no longer popped out.
  EXPECT_FALSE(accessor.IsPoppedOut());
}

IN_PROC_BROWSER_TEST_P(SendTabToSelfBubbleParameterizedTest,
                       ShowBubbleMultipleTimes) {
  // Call ShowBubble multiple times. The early return prevents re-creation.
  ShowToolbarBubble();
  SendTabToSelfBubbleView* first_view =
      controller_->send_tab_to_self_bubble_view();
  EXPECT_TRUE(first_view);

  controller_->ShowBubble(ShareEntryPoint::kToolbarIcon);
  EXPECT_EQ(first_view, controller_->send_tab_to_self_bubble_view());
}

}  // namespace send_tab_to_self
