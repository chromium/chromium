// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service.h"
#include "chrome/browser/enterprise/signin/profile_management_disclaimer_service_factory.h"
#include "chrome/browser/enterprise/signin/signals_disclaimer_metrics.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_active_state_manager/browser_active_state_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/device_signals/core/browser/pref_names.h"
#include "components/policy/core/common/features.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {

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

::testing::AssertionResult WaitForAndClickLearnMoreLink(
    content::WebContents* web_contents) {
  content::WaitForLoadStop(web_contents);
  std::string script = R"(
    new Promise((resolve) => {
      const interval = setInterval(() => {
        const link = document.querySelector('managed-user-profile-notice-app')
                         ?.shadowRoot?.querySelector('signals-disclaimer')
                         ?.shadowRoot?.querySelector('#learnMoreLink');
        if (link && !link.hidden) {
          clearInterval(interval);
          link.click();
          resolve(true);
        }
      }, 50);
    });
  )";

  ::testing::AssertionResult result = content::ExecJs(web_contents, script);
  if (!result) {
    LOG(ERROR) << "WaitForAndClickLearnMoreLink failed: " << result.message();
  }
  return result;
}

void WaitForWebContentsLoaded(content::WebContents* web_contents) {
  CHECK(web_contents);
  content::WaitForLoadStop(web_contents);
  content::WaitForCopyableViewInWebContents(web_contents);

  std::string script = R"(
    new Promise((resolve) => {
      const interval = setInterval(() => {
        const app = document.querySelector('managed-user-profile-notice-app');
        if (app && app.shadowRoot) {
          const disclaimer = app.shadowRoot.querySelector('signals-disclaimer');
          if (disclaimer && disclaimer.shadowRoot) {
            clearInterval(interval);
            Promise.all([
              app.updateComplete,
              disclaimer.updateComplete
            ]).then(() => resolve(true));
          }
        }
      }, 50);
    });
  )";
  ASSERT_TRUE(content::EvalJs(web_contents, script).is_ok());
}

}  // namespace

class DeviceSignalsDisclaimerModalPixelTest
    : public ProfilesPixelTestBaseT<DialogBrowserTest>,
      public testing::WithParamInterface<PixelTestParam> {
 public:
  DeviceSignalsDisclaimerModalPixelTest()
      : ProfilesPixelTestBaseT<DialogBrowserTest>(GetParam()) {}

  ~DeviceSignalsDisclaimerModalPixelTest() override = default;

  void ShowUi(const std::string& name) override {
    gfx::ScopedAnimationDurationScaleMode disable_animation(
        gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    CHECK(browser());

    AccountInfo account_info =
        SignInWithAccount(AccountManagementStatus::kManaged);

    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{},
        "SigninViewControllerDelegateViews");

    SigninViewController::From(browser())->ShowModalManagedUserNoticeDialog(
        signin::EnterpriseProfileCreationDialogParams::
            CreateForDeviceSignalsDisclaimer(
                account_info,
                signin::DeviceSignalsDisclaimerCallback(base::DoNothing()),
                /*is_modal_dialog=*/true));

    widget_waiter.WaitIfNeededAndGet();

    content::WebContents* web_contents =
        SigninViewController::From(browser())
            ->GetModalDialogWebContentsForTesting();
    WaitForWebContentsLoaded(web_contents);
  }
};

IN_PROC_BROWSER_TEST_P(DeviceSignalsDisclaimerModalPixelTest,
                       InvokeUi_default) {
  set_baseline("8231223");
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    DeviceSignalsDisclaimerModalPixelTest,
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

// Sole purpose of this wrapper is capturing the WebContents. The step
// implementation controlling the disclaimer owns its own WebContents so getting
// it via available getters is not possible.
class TestStepTestView : public ProfileManagementStepTestView {
 public:
  using ProfileManagementStepTestView::ProfileManagementStepTestView;

  void ShowScreen(content::WebContents* contents,
                  const GURL& url,
                  base::OnceClosure navigation_finished_closure) override {
    active_contents_ = contents;
    ProfileManagementStepTestView::ShowScreen(
        contents, url, std::move(navigation_finished_closure));
  }

  content::WebContents* active_contents() const { return active_contents_; }

 private:
  raw_ptr<content::WebContents> active_contents_ = nullptr;
};

class DeviceSignalsDisclaimerProfilePickerPixelTest
    : public ProfilesPixelTestBaseT<UiBrowserTest>,
      public testing::WithParamInterface<PixelTestParam> {
 public:
  DeviceSignalsDisclaimerProfilePickerPixelTest()
      : ProfilesPixelTestBaseT<UiBrowserTest>(GetParam()) {
    scoped_feature_list_.InitWithFeatures(
        {policy::features::kDeviceSignalsBackfillDisclaimer,
         switches::kEnforceManagementDisclaimer},
        {});
  }

  void ShowUi(const std::string& name) override {
    gfx::ScopedAnimationDurationScaleMode disable_animation(
        gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    CHECK(browser());

    SignInWithAccount(AccountManagementStatus::kManaged);

    auto* view = new TestStepTestView(
        ProfilePicker::Params::ForTesting(
            ProfilePicker::EntryPoint::kOnStartupNoProfile,
            browser()->GetProfile()->GetPath()),
        ProfileManagementFlowController::Step::kDeviceSignalsDisclaimer,
        /*step_controller_factory=*/
        base::BindRepeating(
            [](Profile* profile, ProfilePickerWebContentsHost* host) {
              return ProfileManagementStepController::
                  CreateForDeviceSignalsDisclaimer(host, profile,
                                                   base::DoNothing());
            },
            browser()->GetProfile()));
    profile_picker_view_tracker_.SetView(view);
    view->ShowAndWait(GetParam().window_size);

    WaitForWebContentsLoaded(view->active_contents());
  }

  bool VerifyUi() override {
    views::Widget* widget = CHECK_DEREF(profile_picker_view()).GetWidget();

    const testing::TestInfo* test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    const std::string baseline = "8231223";
    const std::string screenshot_name =
        base::StrCat({"_", test_info->name(), "_", baseline});

    return VerifyPixelUi(widget,
                         "DeviceSignalsDisclaimerProfilePickerPixelTest",
                         screenshot_name) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {
    if (ProfileManagementStepTestView* view = profile_picker_view()) {
      ViewDeletedWaiter(view).Wait();
    }
  }

 private:
  ProfileManagementStepTestView* profile_picker_view() {
    return static_cast<ProfileManagementStepTestView*>(
        profile_picker_view_tracker_.view());
  }

  views::ViewTracker profile_picker_view_tracker_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(DeviceSignalsDisclaimerProfilePickerPixelTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    ,
    DeviceSignalsDisclaimerProfilePickerPixelTest,
    testing::ValuesIn(std::vector<PixelTestParam>{
        {.test_suffix = "Regular"},
        {.test_suffix = "DarkTheme", .use_dark_theme = true},
        {.test_suffix = "Rtl", .use_right_to_left_language = true},
    }),
    [](const testing::TestParamInfo<PixelTestParam>& info) {
      return info.param.test_suffix;
    });

class DeviceSignalsDisclaimerInteractiveTest : public SigninBrowserTestBase {
 public:
  DeviceSignalsDisclaimerInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {policy::features::kDeviceSignalsBackfillDisclaimer,
         switches::kEnforceManagementDisclaimer},
        {});
  }

 protected:
  content::WebContents* GetModalDialogWebContents(
      BrowserWindowInterface* browser) {
    return SigninViewController::From(browser)
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
  SigninViewController::From(browser())->ShowModalManagedUserNoticeDialog(
      signin::EnterpriseProfileCreationDialogParams::
          CreateForDeviceSignalsDisclaimer(account_info,
                                           result_future.GetCallback(),
                                           /*is_modal_dialog=*/true));

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
  SigninViewController::From(browser())->ShowModalManagedUserNoticeDialog(
      signin::EnterpriseProfileCreationDialogParams::
          CreateForDeviceSignalsDisclaimer(account_info,
                                           result_future.GetCallback(),
                                           /*is_modal_dialog=*/true));

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
  SigninViewController::From(browser())->ShowModalManagedUserNoticeDialog(
      signin::EnterpriseProfileCreationDialogParams::
          CreateForDeviceSignalsDisclaimer(account_info,
                                           result_future.GetCallback(),
                                           /*is_modal_dialog=*/true));

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
    browser()->GetProfile()->GetPrefs()->SetBoolean(
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
    return SigninViewController::From(browser)->ShowsModalDialog();
  }

  void WaitForModalDialog(BrowserWindowInterface* browser) {
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return ShowsModalDialog(browser); }));
  }

  void SimulateBrowserFocus(BrowserWindowInterface* browser) {
    BrowserActiveStateManager::From(browser)->DidBecomeInactive();
    BrowserActiveStateManager::From(browser)->DidBecomeActive();
  }

  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
  std::optional<views::NamedWidgetShownWaiter> widget_waiter_;
  base::HistogramTester histogram_tester_;

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
  histogram_tester_.ExpectBucketCount(kEnterpriseSignalsDisclaimerModalShown,
                                      true, 1);
  histogram_tester_.ExpectUniqueSample(
      kEnterpriseSignalsDisclaimerModalResult,
      EnterpriseSignalsDisclaimerModalResult::kAccepted, 1);
}

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerStartupInteractiveTest,
                       TriggerCancel) {
  SimulateBrowserFocus(browser());
  views::Widget* widget = widget_waiter_->WaitIfNeededAndGet();
  ASSERT_TRUE(widget);
  content::WebContents* dialog_contents = GetModalDialogWebContents(browser());
  ASSERT_TRUE(dialog_contents);

  ProfileBrowsersClosedWaiter browsers_closed_waiter(
      profile_keep_alive_->profile());

  // Click cancel.
  // The result can be an error since the browser will get destroyed upon
  // clicking.
  std::ignore = WaitForAndClickButton(
      dialog_contents, "managed-user-profile-notice-app", "cancel-button");

  // Wait for all browsers to close and for the profile picker to be shown.
  browsers_closed_waiter.Wait();
  WaitForPickerWidgetCreated();
  histogram_tester_.ExpectBucketCount(kEnterpriseSignalsDisclaimerModalShown,
                                      true, 1);
  histogram_tester_.ExpectUniqueSample(
      kEnterpriseSignalsDisclaimerModalResult,
      EnterpriseSignalsDisclaimerModalResult::kDeclined, 1);
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
  SigninViewController::From(browser())->ShowModalSigninErrorDialog();
  error_dialog_waiter.WaitIfNeededAndGet();

  EXPECT_TRUE(ShowsModalDialog(browser()));
  // Verify consent was not granted since the disclaimer was interrupted.
  EXPECT_FALSE(browser()->GetProfile()->GetPrefs()->GetBoolean(
      device_signals::prefs::kDeviceSignalsPermanentConsentReceived));

  views::NamedWidgetShownWaiter next_dialog_waiter(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");

  // Now handle and close the new dialog.
  SigninViewController::From(browser())->CloseModalSignin();
  EXPECT_FALSE(ShowsModalDialog(browser()));

  // Simulate Browser Activated event when the user clicks/focuses the browser.
  SimulateBrowserFocus(browser());

  views::Widget* final_widget = next_dialog_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(final_widget);
  EXPECT_TRUE(ShowsModalDialog(browser()));
  histogram_tester_.ExpectBucketCount(kEnterpriseSignalsDisclaimerModalShown,
                                      true, 2);
  histogram_tester_.ExpectUniqueSample(kEnterpriseSignalsDisclaimerModalResult,
                                       EnterpriseSignalsDisclaimerModalResult::
                                           kDismissedWithoutExplicitUserAction,
                                       1);
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
  BrowserWindowInterface* new_browser = CreateBrowser(browser()->GetProfile());
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
  histogram_tester_.ExpectBucketCount(kEnterpriseSignalsDisclaimerModalShown,
                                      true, 2);
  histogram_tester_.ExpectBucketCount(
      kEnterpriseSignalsDisclaimerModalResult,
      EnterpriseSignalsDisclaimerModalResult::kAccepted, 1);
  histogram_tester_.ExpectBucketCount(
      kEnterpriseSignalsDisclaimerModalResult,
      EnterpriseSignalsDisclaimerModalResult::kDismissedByAnotherWindow, 1);
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
  histogram_tester_.ExpectBucketCount(kEnterpriseSignalsDisclaimerModalShown,
                                      true, 2);
  histogram_tester_.ExpectBucketCount(
      kEnterpriseSignalsDisclaimerModalResult,
      EnterpriseSignalsDisclaimerModalResult::kDeclined, 1);
  histogram_tester_.ExpectBucketCount(
      kEnterpriseSignalsDisclaimerModalResult,
      EnterpriseSignalsDisclaimerModalResult::kDismissedByAnotherWindow, 1);
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
  BrowserWindowInterface* new_browser = CreateBrowser(browser()->GetProfile());
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
  histogram_tester_.ExpectBucketCount(kEnterpriseSignalsDisclaimerModalShown,
                                      true, 2);
  histogram_tester_.ExpectBucketCount(kEnterpriseSignalsDisclaimerModalResult,
                                      EnterpriseSignalsDisclaimerModalResult::
                                          kDismissedWithoutExplicitUserAction,
                                      1);
  histogram_tester_.ExpectBucketCount(
      kEnterpriseSignalsDisclaimerModalResult,
      EnterpriseSignalsDisclaimerModalResult::kAccepted, 1);
}

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerStartupInteractiveTest,
                       LearnMore_OpenAndClose) {
  // The dialog opens in the current browser.
  SimulateBrowserFocus(browser());
  views::Widget* widget = widget_waiter_->WaitIfNeededAndGet();
  ASSERT_TRUE(widget);
  content::WebContents* dialog_contents = GetModalDialogWebContents(browser());
  ASSERT_TRUE(dialog_contents);

  // Click `Learn More` and wait for the popup browser to open.
  ui_test_utils::BrowserCreatedObserver browser_creation_observer;
  ASSERT_TRUE(WaitForAndClickLearnMoreLink(dialog_contents));
  BrowserWindowInterface* popup_browser = browser_creation_observer.Wait();
  ASSERT_TRUE(popup_browser);

  auto* browser_collection =
      ProfileBrowserCollection::GetForProfile(browser()->GetProfile());
  EXPECT_EQ(browser_collection->GetSize(), 2);

  // Simulate the user clicking Close button and wait for destruction.
  ui_test_utils::BrowserDestroyedObserver browser_destroyed_observer(
      popup_browser);
  BrowserView::GetBrowserViewForBrowser(popup_browser)
      ->GetWidget()
      ->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  browser_destroyed_observer.Wait();

  EXPECT_EQ(browser_collection->GetSize(), 1);
  histogram_tester_.ExpectBucketCount(
      kEnterpriseSignalsDisclaimerModalLearnMoreClicked, true, 1);
}

IN_PROC_BROWSER_TEST_F(DeviceSignalsDisclaimerStartupInteractiveTest,
                       LearnMore_OpenAndOpen) {
  // The dialog opens in the current browser.
  SimulateBrowserFocus(browser());
  views::Widget* widget = widget_waiter_->WaitIfNeededAndGet();
  ASSERT_TRUE(widget);
  content::WebContents* dialog_contents = GetModalDialogWebContents(browser());
  ASSERT_TRUE(dialog_contents);

  // Click `Learn More` and wait for the popup browser to open.
  ui_test_utils::BrowserCreatedObserver browser_creation_observer;
  ASSERT_TRUE(WaitForAndClickLearnMoreLink(dialog_contents));
  BrowserWindowInterface* popup_browser = browser_creation_observer.Wait();
  ASSERT_TRUE(popup_browser);
  auto* browser_collection =
      ProfileBrowserCollection::GetForProfile(browser()->GetProfile());
  EXPECT_EQ(browser_collection->GetSize(), 2u);

  BrowserActiveStateManager::From(popup_browser)->DidBecomeInactive();
  SimulateBrowserFocus(browser());

  // Click `Learn More` again.
  ASSERT_FALSE(popup_browser->IsActive());
  ASSERT_TRUE(WaitForAndClickLearnMoreLink(dialog_contents));

  // New window should not be opened, instead the existing popup should be
  // focused.
  ui_test_utils::WaitUntilBrowserBecomeActive(popup_browser);
  EXPECT_EQ(browser_collection->GetSize(), 2u);
  histogram_tester_.ExpectBucketCount(
      kEnterpriseSignalsDisclaimerModalLearnMoreClicked, true, 2);
}

// Profile picker tests are located in
// chrome/browser/ui/views/profiles/profile_picker_view_browsertest.cc
