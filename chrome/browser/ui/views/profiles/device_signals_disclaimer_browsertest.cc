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

class DeviceSignalsDisclaimerInteractiveTest : public InProcessBrowserTest {
 public:
  DeviceSignalsDisclaimerInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {policy::features::kDeviceSignalsBackfillDisclaimer,
         switches::kEnforceManagementDisclaimer},
        {});
  }

 protected:
  bool WaitForAndClickButton(content::WebContents* web_contents,
                             const std::string& app,
                             const std::string& button_id) {
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

    return content::ExecJs(web_contents, script);
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
  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "email@example.com", signin::ConsentLevel::kSignin);

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
  ASSERT_TRUE(WaitForAndClickButton(
      dialog_contents, "managed-user-profile-notice-app", "proceed-button"));

  EXPECT_EQ(result_future.Get(),
            signin::DeviceSignalsDisclaimerResult::kAccepted);
}

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerInteractiveTest, ClickCancel) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "email@example.com", signin::ConsentLevel::kSignin);

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
  ASSERT_TRUE(WaitForAndClickButton(
      dialog_contents, "managed-user-profile-notice-app", "cancel-button"));

  EXPECT_EQ(result_future.Get(),
            signin::DeviceSignalsDisclaimerResult::kCanceled);
}

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerInteractiveTest, CloseBrowser) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "email@example.com", signin::ConsentLevel::kSignin);

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

    // Keep the profile alive so it does not get destroyed when all windows get
    // closed.
    profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
        browser()->profile(), ProfileKeepAliveOrigin::kBrowserWindow);

    // Set up primary account as managed.
    auto* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    signin::MakePrimaryAccountAvailable(identity_manager, "email@example.com",
                                        signin::ConsentLevel::kSignin);

    // Accept account management.
    enterprise_util::SetUserAcceptedAccountManagement(browser()->profile(),
                                                      true);

    // Reset permanent consent preference to false.
    browser()->profile()->GetPrefs()->SetBoolean(
        device_signals::prefs::kDeviceSignalsPermanentConsentReceived, false);

    scoped_command_line_.GetProcessCommandLine()->RemoveSwitch(
        switches::kNoFirstRun);
  }

 protected:
  void TearDownOnMainThread() override {
    profile_keep_alive_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
};

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerStartupInteractiveTest,
                       TriggerProceed) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");

  // Open a new browser window for the same profile to trigger the dialog
  // naturally.
  Browser* new_browser = CreateBrowser(browser()->profile());
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  content::WebContents* dialog_contents =
      GetModalDialogWebContents(new_browser);
  ASSERT_TRUE(dialog_contents);

  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);

  // Click proceed.
  ASSERT_TRUE(WaitForAndClickButton(
      dialog_contents, "managed-user-profile-notice-app", "proceed-button"));

  // Verify the dialog closed and the preference is true.
  destroyed_waiter.Wait();
  EXPECT_TRUE(new_browser->profile()->GetPrefs()->GetBoolean(
      device_signals::prefs::kDeviceSignalsPermanentConsentReceived));
}

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerStartupInteractiveTest,
                       TriggerCancel) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");

  // Open a new browser window for the same profile to trigger the dialog
  // naturally.
  Browser* new_browser = CreateBrowser(browser()->profile());

  widget_waiter.WaitIfNeededAndGet();

  content::WebContents* dialog_contents =
      GetModalDialogWebContents(new_browser);
  ASSERT_TRUE(dialog_contents);

  ProfileBrowsersClosedWaiter waiter(profile_keep_alive_->profile());

  // Click cancel.
  // The result can be an error since the browser will get destroyed upon
  // clicking.
  std::ignore = WaitForAndClickButton(
      dialog_contents, "managed-user-profile-notice-app", "cancel-button");

  // Wait for all browsers to close and for the profile picker to be shown.
  waiter.Wait();
  WaitForPickerWidgetCreated();
}

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerStartupInteractiveTest,
                       TriggerInterruptedByAnotherDialog) {
  views::NamedWidgetShownWaiter disclaimer_waiter(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");
  Browser* new_browser = CreateBrowser(browser()->profile());
  disclaimer_waiter.WaitIfNeededAndGet();

  EXPECT_TRUE(
      new_browser->GetFeatures().signin_view_controller()->ShowsModalDialog());

  // Interrupt by showing another signin modal dialog (e.g. signin error
  // dialog). This should close the existing disclaimer dialog.
  views::NamedWidgetShownWaiter error_dialog_waiter(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");
  new_browser->GetFeatures()
      .signin_view_controller()
      ->ShowModalSigninErrorDialog();
  error_dialog_waiter.WaitIfNeededAndGet();

  EXPECT_TRUE(
      new_browser->GetFeatures().signin_view_controller()->ShowsModalDialog());
  // Verify consent was not granted since the disclaimer was interrupted.
  EXPECT_FALSE(new_browser->profile()->GetPrefs()->GetBoolean(
      device_signals::prefs::kDeviceSignalsPermanentConsentReceived));

  // Now handle and close the new dialog.
  new_browser->GetFeatures().signin_view_controller()->CloseModalSignin();
  EXPECT_FALSE(
      new_browser->GetFeatures().signin_view_controller()->ShowsModalDialog());

  // Simulate Browser Activated event when the user clicks/focuses the browser.
  new_browser->DidBecomeInactive();
  new_browser->DidBecomeActive();
  views::NamedWidgetShownWaiter reactivated_disclaimer_waiter(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");
  reactivated_disclaimer_waiter.WaitIfNeededAndGet();

  EXPECT_TRUE(
      new_browser->GetFeatures().signin_view_controller()->ShowsModalDialog());
}

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerStartupInteractiveTest,
                       TriggerProceed_MultipleWindows) {
  views::NamedWidgetShownWaiter widget_waiter1(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");
  Browser* new_browser1 = CreateBrowser(browser()->profile());
  views::Widget* widget1 = widget_waiter1.WaitIfNeededAndGet();

  views::NamedWidgetShownWaiter widget_waiter2(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");
  Browser* new_browser2 = CreateBrowser(browser()->profile());
  views::Widget* widget2 = widget_waiter2.WaitIfNeededAndGet();

  // Ensure both browsers are displaying the dialog.
  content::WebContents* dialog_contents1 =
      GetModalDialogWebContents(new_browser1);
  ASSERT_TRUE(dialog_contents1);
  EXPECT_TRUE(
      new_browser1->GetFeatures().signin_view_controller()->ShowsModalDialog());
  EXPECT_TRUE(
      new_browser2->GetFeatures().signin_view_controller()->ShowsModalDialog());

  views::test::WidgetDestroyedWaiter destroyed_waiter1(widget1);
  views::test::WidgetDestroyedWaiter destroyed_waiter2(widget2);

  // Click proceed on the first window.
  ASSERT_TRUE(WaitForAndClickButton(
      dialog_contents1, "managed-user-profile-notice-app", "proceed-button"));

  // Verify both dialogs closed and the preference is true.
  destroyed_waiter1.Wait();
  destroyed_waiter2.Wait();
  EXPECT_TRUE(new_browser1->profile()->GetPrefs()->GetBoolean(
      device_signals::prefs::kDeviceSignalsPermanentConsentReceived));
}

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerStartupInteractiveTest,
                       TriggerCancel_MultipleWindows) {
  views::NamedWidgetShownWaiter widget_waiter1(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");
  Browser* new_browser1 = CreateBrowser(browser()->profile());
  widget_waiter1.WaitIfNeededAndGet();

  views::NamedWidgetShownWaiter widget_waiter2(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");
  std::ignore = CreateBrowser(browser()->profile());
  widget_waiter2.WaitIfNeededAndGet();

  content::WebContents* dialog_contents1 =
      GetModalDialogWebContents(new_browser1);
  ASSERT_TRUE(dialog_contents1);

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
  views::NamedWidgetShownWaiter widget_waiter1(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");
  Browser* new_browser1 = CreateBrowser(browser()->profile());
  widget_waiter1.WaitIfNeededAndGet();

  views::NamedWidgetShownWaiter widget_waiter2(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");
  Browser* new_browser2 = CreateBrowser(browser()->profile());
  views::Widget* widget2 = widget_waiter2.WaitIfNeededAndGet();

  EXPECT_TRUE(
      new_browser1->GetFeatures().signin_view_controller()->ShowsModalDialog());
  EXPECT_TRUE(
      new_browser2->GetFeatures().signin_view_controller()->ShowsModalDialog());

  // Close the first browser window. This dismisses the disclaimer on that
  // window.
  CloseBrowserSynchronously(new_browser1);

  content::WebContents* dialog_contents2 =
      GetModalDialogWebContents(new_browser2);
  ASSERT_TRUE(dialog_contents2);

  views::test::WidgetDestroyedWaiter destroyed_waiter2(widget2);

  // Click proceed on the second window.
  ASSERT_TRUE(WaitForAndClickButton(
      dialog_contents2, "managed-user-profile-notice-app", "proceed-button"));

  // Verify the second dialog closed and the preference is true.
  destroyed_waiter2.Wait();
  EXPECT_TRUE(new_browser2->profile()->GetPrefs()->GetBoolean(
      device_signals::prefs::kDeviceSignalsPermanentConsentReceived));
}
