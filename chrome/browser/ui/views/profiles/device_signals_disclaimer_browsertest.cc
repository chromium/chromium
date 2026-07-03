// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/profiles/profiles_pixel_test_utils.h"
#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
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
                    account_info,
                    signin::SigninChoiceCallback(base::DoNothing())));

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

class ManagedUserProfileNoticeDeviceSignalsDisclaimerInteractiveTest
    : public InProcessBrowserTest {
 public:
  ManagedUserProfileNoticeDeviceSignalsDisclaimerInteractiveTest() = default;
  ~ManagedUserProfileNoticeDeviceSignalsDisclaimerInteractiveTest() override =
      default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    AccountInfo account_info =
        AccountInfo::Builder(GaiaId("12345"), "email@example.com")
            .SetAccountId(CoreAccountId::FromGaiaId(GaiaId("12345")))
            .SetHostedDomain("example.com")
            .Build();
    account_info_ = account_info;
  }

 protected:
  void WaitForAndClickButton(content::WebContents* web_contents,
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

    ASSERT_TRUE(content::ExecJs(web_contents, script));
  }

  content::WebContents* GetModalDialogWebContents() {
    return browser()
        ->GetFeatures()
        .signin_view_controller()
        ->GetModalDialogWebContentsForTesting();
  }

  AccountInfo account_info_;
};

IN_PROC_BROWSER_TEST_F(
    ManagedUserProfileNoticeDeviceSignalsDisclaimerInteractiveTest,
    ClickProceed) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");

  base::MockCallback<signin::SigninChoiceCallback>
      mock_process_user_choice_callback;
  base::MockCallback<base::OnceClosure> mock_done_callback;

  browser()
      ->GetFeatures()
      .signin_view_controller()
      ->ShowModalManagedUserNoticeDialog(
          signin::EnterpriseProfileCreationDialogParams::
              CreateForDeviceSignalsDisclaimer(
                  account_info_, mock_process_user_choice_callback.Get(),
                  mock_done_callback.Get()));

  widget_waiter.WaitIfNeededAndGet();

  content::WebContents* dialog_contents = GetModalDialogWebContents();
  ASSERT_TRUE(dialog_contents);

  base::RunLoop run_loop;
  EXPECT_CALL(mock_process_user_choice_callback,
              Run(signin::SIGNIN_CHOICE_NEW_PROFILE))
      .Times(1);
  EXPECT_CALL(mock_done_callback, Run()).WillOnce([&]() { run_loop.Quit(); });

  WaitForAndClickButton(dialog_contents, "managed-user-profile-notice-app",
                        "proceed-button");
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    ManagedUserProfileNoticeDeviceSignalsDisclaimerInteractiveTest,
    ClickCancel) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "SigninViewControllerDelegateViews");

  base::MockCallback<signin::SigninChoiceCallback>
      mock_process_user_choice_callback;
  base::MockCallback<base::OnceClosure> mock_done_callback;

  browser()
      ->GetFeatures()
      .signin_view_controller()
      ->ShowModalManagedUserNoticeDialog(
          signin::EnterpriseProfileCreationDialogParams::
              CreateForDeviceSignalsDisclaimer(
                  account_info_, mock_process_user_choice_callback.Get(),
                  mock_done_callback.Get()));

  widget_waiter.WaitIfNeededAndGet();

  content::WebContents* dialog_contents = GetModalDialogWebContents();
  ASSERT_TRUE(dialog_contents);

  base::RunLoop run_loop;
  EXPECT_CALL(mock_process_user_choice_callback,
              Run(signin::SIGNIN_CHOICE_CANCEL))
      .Times(1);
  EXPECT_CALL(mock_done_callback, Run()).WillOnce([&]() { run_loop.Quit(); });

  WaitForAndClickButton(dialog_contents, "managed-user-profile-notice-app",
                        "cancel-button");
  run_loop.Run();
}
