// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/device_signals/core/browser/pref_names.h"
#include "components/policy/core/common/features.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

class ManagedUserProfileNoticeDeviceSignalsDisclaimerPixelTest
    : public ProfilesPixelTestBaseT<DialogBrowserTest>,
      public testing::WithParamInterface<PixelTestParam> {
 public:
  ManagedUserProfileNoticeDeviceSignalsDisclaimerPixelTest()
      : ProfilesPixelTestBaseT<DialogBrowserTest>(GetParam()) {}

  ~ManagedUserProfileNoticeDeviceSignalsDisclaimerPixelTest() override =
      default;

  void ShowUi(const std::string& name) override {
    gfx::ScopedAnimationDurationScaleMode disable_animation(
        gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    CHECK(browser());

    AccountInfo account_info =
        SignInWithAccount(AccountManagementStatus::kManaged);

    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{},
        "SigninViewControllerDelegateViews");

    browser()
        ->GetFeatures()
        .signin_view_controller()
        ->ShowModalManagedUserNoticeDialog(
            signin::EnterpriseProfileCreationDialogParams::
                CreateForDeviceSignalsDisclaimer(
                    account_info, signin::DeviceSignalsDisclaimerCallback(
                                      base::DoNothing())));

    widget_waiter.WaitIfNeededAndGet();
  }
};

IN_PROC_BROWSER_TEST_P(ManagedUserProfileNoticeDeviceSignalsDisclaimerPixelTest,
                       InvokeUi_default) {
#if BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(features::kInitialWebUI)) {
    GTEST_SKIP() << "Skipping test because it fails with InitialWebUI enabled. "
                    "See b/477426026.";
  }
#endif

  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ManagedUserProfileNoticeDeviceSignalsDisclaimerPixelTest,
    testing::ValuesIn(std::vector<PixelTestParam>{
        {.test_suffix = "Regular"},
        {.test_suffix = "DarkTheme", .use_dark_theme = true},
        {.test_suffix = "Rtl", .use_right_to_left_language = true},
    }),
    [](const testing::TestParamInfo<PixelTestParam>& info) {
      return info.param.test_suffix;
    });

class ProfileBrowsersClosedWaiter : public BrowserCollectionObserver {
 public:
  explicit ProfileBrowsersClosedWaiter(Profile* profile) : profile_(profile) {}

  void Wait() {
    auto* collection = ProfileBrowserCollection::GetForProfile(profile_);
    if (!collection->IsEmpty()) {
      observation_.Observe(collection);
    } else {
      return;
    }
    run_loop_.Run();
  }

 private:
  // BrowserCollectionObserver:
  void OnBrowserClosed(BrowserWindowInterface* browser) override {
    if (ProfileBrowserCollection::GetForProfile(profile_)->IsEmpty()) {
      run_loop_.Quit();
    }
  }

  const raw_ptr<Profile> profile_;
  base::RunLoop run_loop_;
  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      observation_{this};
};

class DeviceSignalsDisclaimerInteractiveTest : public SigninBrowserTestBase {
 public:
  DeviceSignalsDisclaimerInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {policy::features::kDeviceSignalsBackfillDisclaimer,
         switches::kEnforceManagementDisclaimer},
        {});
  }

 protected:
  ::testing::AssertionResult WaitForAndClickButton(
      content::WebContents* web_contents,
      const std::string& app,
      const std::string& button_id,
      bool log_on_failure = false) {
    content::WaitForLoadStop(web_contents);
    std::string script = base::StringPrintf(R"(
      new Promise((resolve) => {
        const interval = setInterval(() => {
          const button = document.querySelector('%s')?.shadowRoot?.querySelector('#%s');
          if (button && !button.hidden) {
            clearInterval(interval);
            button.click();
            resolve(true);
          }
        }, 50);
      });
    )",
                                            app.c_str(), button_id.c_str());

    ::testing::AssertionResult result = content::ExecJs(web_contents, script);
    if (!result && log_on_failure) {
      LOG(ERROR) << "WaitForAndClickButton failed for " << app << " -> "
                 << button_id << ": " << result.message();
    }
    return result;
  }

  content::WebContents* GetModalDialogWebContents(Browser* browser) {
    return browser->GetFeatures()
        .signin_view_controller()
        ->GetModalDialogWebContentsForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerInteractiveTest, ClickProceed) {
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager(), "email@example.com", signin::ConsentLevel::kSignin);

  base::test::TestFuture<signin::DeviceSignalsDisclaimerResult> result_future;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");
  browser()
      ->GetFeatures()
      .signin_view_controller()
      ->ShowModalManagedUserNoticeDialog(
          signin::EnterpriseProfileCreationDialogParams::
              CreateForDeviceSignalsDisclaimer(account_info,
                                               result_future.GetCallback()));

  std::ignore = widget_waiter.WaitIfNeededAndGet();
  content::WebContents* dialog_contents = GetModalDialogWebContents(browser());
  ASSERT_TRUE(dialog_contents);
  ASSERT_TRUE(WaitForAndClickButton(dialog_contents,
                                    "managed-user-profile-notice-app",
                                    "proceed-button", /*log_on_failure=*/true));

  EXPECT_EQ(result_future.Get(),
            signin::DeviceSignalsDisclaimerResult::kAccepted);
}

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerInteractiveTest, ClickCancel) {
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager(), "email@example.com", signin::ConsentLevel::kSignin);

  base::test::TestFuture<signin::DeviceSignalsDisclaimerResult> result_future;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");
  browser()
      ->GetFeatures()
      .signin_view_controller()
      ->ShowModalManagedUserNoticeDialog(
          signin::EnterpriseProfileCreationDialogParams::
              CreateForDeviceSignalsDisclaimer(account_info,
                                               result_future.GetCallback()));

  std::ignore = widget_waiter.WaitIfNeededAndGet();
  content::WebContents* dialog_contents = GetModalDialogWebContents(browser());
  ASSERT_TRUE(dialog_contents);
  ASSERT_TRUE(WaitForAndClickButton(dialog_contents,
                                    "managed-user-profile-notice-app",
                                    "cancel-button", /*log_on_failure=*/true));

  EXPECT_EQ(result_future.Get(),
            signin::DeviceSignalsDisclaimerResult::kCanceled);
}

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerInteractiveTest, CloseBrowser) {
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager(), "email@example.com", signin::ConsentLevel::kSignin);

  base::test::TestFuture<signin::DeviceSignalsDisclaimerResult> result_future;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");
  browser()
      ->GetFeatures()
      .signin_view_controller()
      ->ShowModalManagedUserNoticeDialog(
          signin::EnterpriseProfileCreationDialogParams::
              CreateForDeviceSignalsDisclaimer(account_info,
                                               result_future.GetCallback()));

  std::ignore = widget_waiter.WaitIfNeededAndGet();

  // Close the browser.
  CloseBrowserSynchronously(browser());

  EXPECT_EQ(result_future.Get(),
            signin::DeviceSignalsDisclaimerResult::kDismissed);
}

class DeviceSignalsDisclaimerStartupInteractiveTest
    : public DeviceSignalsDisclaimerInteractiveTest,
      public WithProfilePickerTestHelpers {
 public:
  DeviceSignalsDisclaimerStartupInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {policy::features::kDeviceSignalsBackfillDisclaimer,
         switches::kEnforceManagementDisclaimer},
        {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    widget_waiter_.emplace(views::test::AnyWidgetTestPasskey{},
                           "SigninViewControllerDelegateViews");

    // Keep the profile alive so it does not get destroyed when all windows get
    // closed.
    profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
        browser()->GetProfile(), ProfileKeepAliveOrigin::kBrowserWindow);

    // Set up primary account as managed.
    auto* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->GetProfile());
    signin::MakePrimaryAccountAvailable(identity_manager, "email@example.com",
                                        signin::ConsentLevel::kSignin);

    // Accept account management.
    enterprise_util::SetUserAcceptedAccountManagement(browser()->GetProfile(),
                                                      true);

    // Reset permanent consent preference to false.
    browser()->profile()->GetPrefs()->SetBoolean(
        device_signals::prefs::kDeviceSignalsPermanentConsentReceived, false);

    ProfileManagementDisclaimerServiceFactory::GetForProfile(
        profile_keep_alive_->profile())
        ->SetBypassNoFirstRunForTesting(true);
  }

 protected:
  void TearDownOnMainThread() override {
    profile_keep_alive_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  bool ShowsModalDialog(BrowserWindowInterface* browser) {
    return browser->GetFeatures().signin_view_controller()->ShowsModalDialog();
  }

  void WaitForModalDialog(BrowserWindowInterface* browser) {
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return ShowsModalDialog(browser); }));
  }

  void SimulateBrowserFocus(Browser* browser) {
    browser->DidBecomeInactive();
    browser->DidBecomeActive();
  }

  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
  std::optional<views::NamedWidgetShownWaiter> widget_waiter_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerStartupInteractiveTest,
                       TriggerProceed) {
  SimulateBrowserFocus(browser());
  views::Widget* widget = widget_waiter_->WaitIfNeededAndGet();
  ASSERT_TRUE(widget);
  content::WebContents* dialog_contents = GetModalDialogWebContents(browser());
  ASSERT_TRUE(dialog_contents);
  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);

  // Click proceed.
  std::ignore = WaitForAndClickButton(
      dialog_contents, "managed-user-profile-notice-app", "proceed-button");

  // Verify the dialog closed and the preference is true.
  destroyed_waiter.Wait();
  EXPECT_TRUE(browser()->GetProfile()->GetPrefs()->GetBoolean(
      device_signals::prefs::kDeviceSignalsPermanentConsentReceived));
}

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerStartupInteractiveTest,
                       TriggerCancel) {
  SimulateBrowserFocus(browser());
  views::Widget* widget = widget_waiter_->WaitIfNeededAndGet();
  ASSERT_TRUE(widget);
  content::WebContents* dialog_contents = GetModalDialogWebContents(browser());
  ASSERT_TRUE(dialog_contents);

  ProfileBrowsersClosedWaiter browers_closed_waiter(
      profile_keep_alive_->profile());

  // Click cancel.
  // The result can be an error since the browser will get destroyed upon
  // clicking.
  std::ignore = WaitForAndClickButton(
      dialog_contents, "managed-user-profile-notice-app", "cancel-button");

  // Wait for all browsers to close and for the profile picker to be shown.
  browers_closed_waiter.Wait();
  WaitForPickerWidgetCreated();
}

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerStartupInteractiveTest,
                       TriggerInterruptedByAnotherDialog) {
  SimulateBrowserFocus(browser());
  views::Widget* widget = widget_waiter_->WaitIfNeededAndGet();
  ASSERT_TRUE(widget);

  // Interrupt by showing another signin modal dialog (e.g. signin error
  // dialog). This should close the existing disclaimer dialog.
  views::NamedWidgetShownWaiter error_dialog_waiter(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");
  browser()
      ->GetFeatures()
      .signin_view_controller()
      ->ShowModalSigninErrorDialog();
  error_dialog_waiter.WaitIfNeededAndGet();

  EXPECT_TRUE(ShowsModalDialog(browser()));
  // Verify consent was not granted since the disclaimer was interrupted.
  EXPECT_FALSE(browser()->GetProfile()->GetPrefs()->GetBoolean(
      device_signals::prefs::kDeviceSignalsPermanentConsentReceived));

  views::NamedWidgetShownWaiter next_dialog_waiter(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");

  // Now handle and close the new dialog.
  browser()->GetFeatures().signin_view_controller()->CloseModalSignin();
  EXPECT_FALSE(ShowsModalDialog(browser()));

  // Simulate Browser Activated event when the user clicks/focuses the browser.
  SimulateBrowserFocus(browser());

  views::Widget* final_widget = next_dialog_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(final_widget);
  EXPECT_TRUE(ShowsModalDialog(browser()));
}

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerStartupInteractiveTest,
                       TriggerProceed_MultipleWindows) {
  // The dialog opens in the current browser.
  SimulateBrowserFocus(browser());
  views::Widget* widget = widget_waiter_->WaitIfNeededAndGet();
  ASSERT_TRUE(widget);
  content::WebContents* dialog_contents1 = GetModalDialogWebContents(browser());
  ASSERT_TRUE(dialog_contents1);

  // Open a second browser and wait for the dialog there too.
  views::NamedWidgetShownWaiter new_widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");
  Browser* new_browser = CreateBrowser(browser()->GetProfile());
  views::Widget* new_widget = new_widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(new_widget);
  content::WebContents* dialog_contents2 =
      GetModalDialogWebContents(new_browser);
  ASSERT_TRUE(dialog_contents2);

  EXPECT_TRUE(ShowsModalDialog(browser()));
  EXPECT_TRUE(ShowsModalDialog(new_browser));

  views::test::WidgetDestroyedWaiter destroyed_waiter1(widget);
  views::test::WidgetDestroyedWaiter destroyed_waiter2(new_widget);

  // Click proceed on the first window.
  std::ignore = WaitForAndClickButton(
      dialog_contents1, "managed-user-profile-notice-app", "proceed-button");

  // Verify both dialogs closed and the preference is true.
  destroyed_waiter1.Wait();
  destroyed_waiter2.Wait();
  EXPECT_TRUE(new_browser->GetProfile()->GetPrefs()->GetBoolean(
      device_signals::prefs::kDeviceSignalsPermanentConsentReceived));
}

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerStartupInteractiveTest,
                       TriggerCancel_MultipleWindows) {
  // The dialog opens in the current browser.
  SimulateBrowserFocus(browser());
  views::Widget* widget = widget_waiter_->WaitIfNeededAndGet();
  ASSERT_TRUE(widget);
  content::WebContents* dialog_contents1 = GetModalDialogWebContents(browser());
  ASSERT_TRUE(dialog_contents1);

  // Open a second browser and wait for the dialog there too.
  views::NamedWidgetShownWaiter new_widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");
  std::ignore = CreateBrowser(browser()->GetProfile());
  views::Widget* new_widget = new_widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(new_widget);

  ProfileBrowsersClosedWaiter waiter(profile_keep_alive_->profile());

  // Click cancel on the first window.
  // Ignoring the result of ExecJS because the browser teardown can cause it to
  // fail.
  std::ignore = WaitForAndClickButton(
      dialog_contents1, "managed-user-profile-notice-app", "cancel-button");

  // Wait for all browsers to close and for the profile picker to be shown.
  waiter.Wait();
  WaitForPickerWidgetCreated();
}

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerStartupInteractiveTest,
                       TriggerProceed_MultipleWindows_OneWindowClosedFirst) {
  // The dialog opens in the current browser.
  SimulateBrowserFocus(browser());
  views::Widget* widget = widget_waiter_->WaitIfNeededAndGet();
  ASSERT_TRUE(widget);

  // Open a second browser and wait for the dialog there too.
  views::NamedWidgetShownWaiter new_widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");
  Browser* new_browser = CreateBrowser(browser()->GetProfile());
  views::Widget* new_widget = new_widget_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(new_widget);

  EXPECT_TRUE(ShowsModalDialog(browser()));
  EXPECT_TRUE(ShowsModalDialog(new_browser));

  // Close the first browser window. This dismisses the disclaimer on that
  // window.
  CloseBrowserSynchronously(browser());

  content::WebContents* dialog_contents2 =
      GetModalDialogWebContents(new_browser);
  ASSERT_TRUE(dialog_contents2);

  views::test::WidgetDestroyedWaiter destroyed_waiter2(new_widget);

  // Click proceed on the second window.
  std::ignore = WaitForAndClickButton(
      dialog_contents2, "managed-user-profile-notice-app", "proceed-button");

  // Verify the second dialog closed and the preference is true.
  destroyed_waiter2.Wait();
  EXPECT_TRUE(new_browser->GetProfile()->GetPrefs()->GetBoolean(
      device_signals::prefs::kDeviceSignalsPermanentConsentReceived));
}
