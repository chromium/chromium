// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/safe_browsing/tailored_security_desktop_dialog_manager.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_outcome.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

const char kEnhancedProtectionSettingsUrl[] =
    "chrome://settings/security?q=enhanced";

// A struct of test parameters that can be used by parameterized tests.
struct TestParam {
  // The suffix for the test name.
  std::string test_suffix;
  // Whether to use a dark theme or not.
  bool use_dark_theme = false;
};

// To be passed as 4th argument to `INSTANTIATE_TEST_SUITE_P()`, allows the test
// to be named like `All/<TestClassName>.InvokeUi_default/<TestSuffix>` instead
// of using the index of the param in `kTestParam` as suffix.
std::string ParamToTestSuffix(const ::testing::TestParamInfo<TestParam>& info) {
  return info.param.test_suffix;
}

const TestParam kTestParams[] = {{"LightTheme", /*use_dark_theme=*/false},
                                 {"DarkTheme", /*use_dark_theme=*/true}};

void ClickButton(views::BubbleDialogDelegate* bubble_delegate,
                 views::View* button) {
  // Reset the timer to make sure that test click isn't discarded as possibly
  // unintended.
  bubble_delegate->ResetViewShownTimeStampForTesting();
  gfx::Point center(button->width() / 2, button->height() / 2);
  const ui::MouseEvent event(ui::EventType::kMousePressed, center, center,
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  button->OnMousePressed(event);
  button->OnMouseReleased(event);
}

}  // namespace

class TailoredSecurityDesktopDialogManagerTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<TestParam> {
 public:
  TailoredSecurityDesktopDialogManagerTest() {
    dialog_manager_ =
        std::make_unique<safe_browsing::TailoredSecurityDesktopDialogManager>();
  }

  TailoredSecurityDesktopDialogManagerTest(
      const TailoredSecurityDesktopDialogManagerTest&) = delete;
  TailoredSecurityDesktopDialogManagerTest& operator=(
      const TailoredSecurityDesktopDialogManagerTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Reduce flakes by ensuring that animation is disabled.
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    const std::string& actual_name = name.substr(0, name.find("/"));
    if (actual_name == "enabledDialog") {
      dialog_manager_->ShowEnabledDialogForBrowser(browser());
    } else if (actual_name == "disabledDialog") {
      dialog_manager_->ShowDisabledDialogForBrowser(browser());
    } else {
      FAIL() << "No dialog case defined for this string: " << name;
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam().use_dark_theme) {
      command_line->AppendSwitch(switches::kForceDarkMode);
    }
  }

  views::Widget* ShowTailoredSecurityEnabledDialog(Browser* browser) {
    views::NamedWidgetShownWaiter waiter(
        views::test::AnyWidgetTestPasskey{},
        safe_browsing::kTailoredSecurityNoticeDialog);
    dialog_manager_->ShowEnabledDialogForBrowser(browser);

    return waiter.WaitIfNeededAndGet();
  }

  views::Widget* ShowTailoredSecurityDisabledDialog(Browser* browser) {
    views::NamedWidgetShownWaiter waiter(
        views::test::AnyWidgetTestPasskey{},
        safe_browsing::kTailoredSecurityNoticeDialog);
    dialog_manager_->ShowDisabledDialogForBrowser(browser);

    return waiter.WaitIfNeededAndGet();
  }

 private:
  std::unique_ptr<safe_browsing::TailoredSecurityDesktopDialogManager>
      dialog_manager_;
};

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogManagerTest,
                       InvokeUi_enabledDialog) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogManagerTest,
                       InvokeUi_disabledDialog) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogManagerTest,
                       EnabledDialogOkButtonIncrementsAcknowledgedHistogram) {
  base::HistogramTester histograms;
  auto* dialog = ShowTailoredSecurityEnabledDialog(browser());
  auto* bubble_delegate = dialog->widget_delegate()->AsBubbleDialogDelegate();
  histograms.ExpectBucketCount(safe_browsing::kEnabledDialogOutcome,
                               TailoredSecurityOutcome::kAccepted, 0);
  ClickButton(bubble_delegate, bubble_delegate->GetOkButton());
  histograms.ExpectBucketCount(safe_browsing::kEnabledDialogOutcome,
                               TailoredSecurityOutcome::kAccepted, 1);
}

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogManagerTest,
                       EnabledDialogCancelButtonIncrementsSettingsHistogram) {
  base::HistogramTester histograms;
  auto* dialog = ShowTailoredSecurityEnabledDialog(browser());
  auto* bubble_delegate = dialog->widget_delegate()->AsBubbleDialogDelegate();
  histograms.ExpectBucketCount(safe_browsing::kEnabledDialogOutcome,
                               TailoredSecurityOutcome::kSettings, 0);

  ClickButton(bubble_delegate, bubble_delegate->GetCancelButton());
  histograms.ExpectBucketCount(safe_browsing::kEnabledDialogOutcome,
                               TailoredSecurityOutcome::kSettings, 1);
}

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogManagerTest,
                       EnabledDialogCancelButtonNavigatesToSettings) {
  base::HistogramTester histograms;
  auto* dialog = ShowTailoredSecurityEnabledDialog(browser());
  auto* bubble_delegate = dialog->widget_delegate()->AsBubbleDialogDelegate();

  ClickButton(bubble_delegate, bubble_delegate->GetCancelButton());
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL(),
            GURL(kEnhancedProtectionSettingsUrl));
}

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogManagerTest,
                       EnabledDialogRecordsUserActionOnShow) {
  base::UserActionTester uat;
  EXPECT_EQ(
      uat.GetActionCount("SafeBrowsing.AccountIntegration.EnabledDialog.Shown"),
      0);

  ShowTailoredSecurityEnabledDialog(browser());

  EXPECT_EQ(
      uat.GetActionCount("SafeBrowsing.AccountIntegration.EnabledDialog.Shown"),
      1);
}

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogManagerTest,
                       EnabledDialogOkButtonRecordsUserAction) {
  base::UserActionTester uat;
  auto* dialog = ShowTailoredSecurityEnabledDialog(browser());
  auto* bubble_delegate = dialog->widget_delegate()->AsBubbleDialogDelegate();
  EXPECT_EQ(
      uat.GetActionCount(
          "SafeBrowsing.AccountIntegration.EnabledDialog.OkButtonClicked"),
      0);

  ClickButton(bubble_delegate, bubble_delegate->GetOkButton());
  EXPECT_EQ(
      uat.GetActionCount(
          "SafeBrowsing.AccountIntegration.EnabledDialog.OkButtonClicked"),
      1);
}

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogManagerTest,
                       EnabledDialogCancelButtonRecordsUserAction) {
  base::UserActionTester uat;
  auto* dialog = ShowTailoredSecurityEnabledDialog(browser());
  auto* bubble_delegate = dialog->widget_delegate()->AsBubbleDialogDelegate();
  EXPECT_EQ(uat.GetActionCount("SafeBrowsing.AccountIntegration.EnabledDialog."
                               "SettingsButtonClicked"),
            0);

  ClickButton(bubble_delegate, bubble_delegate->GetCancelButton());
  EXPECT_EQ(uat.GetActionCount("SafeBrowsing.AccountIntegration.EnabledDialog."
                               "SettingsButtonClicked"),
            1);
}

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogManagerTest,
                       DisabledDialogOkButtonIncrementsAcknowledgedHistogram) {
  base::HistogramTester histograms;
  auto* dialog = ShowTailoredSecurityDisabledDialog(browser());
  auto* bubble_delegate = dialog->widget_delegate()->AsBubbleDialogDelegate();
  histograms.ExpectBucketCount(safe_browsing::kDisabledDialogOutcome,
                               TailoredSecurityOutcome::kAccepted, 0);
  ClickButton(bubble_delegate, bubble_delegate->GetOkButton());
  histograms.ExpectBucketCount(safe_browsing::kDisabledDialogOutcome,
                               TailoredSecurityOutcome::kAccepted, 1);
}

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogManagerTest,
                       DisabledDialogCancelButtonIncrementsSettingsHistogram) {
  base::HistogramTester histograms;
  auto* dialog = ShowTailoredSecurityDisabledDialog(browser());
  auto* bubble_delegate = dialog->widget_delegate()->AsBubbleDialogDelegate();
  histograms.ExpectBucketCount(safe_browsing::kDisabledDialogOutcome,
                               TailoredSecurityOutcome::kSettings, 0);

  ClickButton(bubble_delegate, bubble_delegate->GetCancelButton());
  histograms.ExpectBucketCount(safe_browsing::kDisabledDialogOutcome,
                               TailoredSecurityOutcome::kSettings, 1);
}

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogManagerTest,
                       DisabledDialogCancelButtonNavigatesToSettings) {
  base::HistogramTester histograms;
  auto* dialog = ShowTailoredSecurityDisabledDialog(browser());
  auto* bubble_delegate = dialog->widget_delegate()->AsBubbleDialogDelegate();

  ClickButton(bubble_delegate, bubble_delegate->GetCancelButton());
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL(),
            GURL(kEnhancedProtectionSettingsUrl));
}

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogManagerTest,
                       DisabledDialogRecordsUserActionOnShow) {
  base::UserActionTester uat;
  EXPECT_EQ(uat.GetActionCount(
                "SafeBrowsing.AccountIntegration.DisabledDialog.Shown"),
            0);

  ShowTailoredSecurityDisabledDialog(browser());

  EXPECT_EQ(uat.GetActionCount(
                "SafeBrowsing.AccountIntegration.DisabledDialog.Shown"),
            1);
}

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogManagerTest,
                       DisabledDialogOkButtonRecordsUserAction) {
  base::UserActionTester uat;
  auto* dialog = ShowTailoredSecurityDisabledDialog(browser());
  auto* bubble_delegate = dialog->widget_delegate()->AsBubbleDialogDelegate();
  EXPECT_EQ(uat.GetActionCount("SafeBrowsing.AccountIntegration.DisabledDialog."
                               "OkButtonClicked"),
            0);

  ClickButton(bubble_delegate, bubble_delegate->GetOkButton());
  EXPECT_EQ(uat.GetActionCount("SafeBrowsing.AccountIntegration.DisabledDialog."
                               "OkButtonClicked"),
            1);
}

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogManagerTest,
                       DisabledDialogCancelButtonRecordsUserAction) {
  base::UserActionTester uat;
  auto* dialog = ShowTailoredSecurityDisabledDialog(browser());
  auto* bubble_delegate = dialog->widget_delegate()->AsBubbleDialogDelegate();
  EXPECT_EQ(uat.GetActionCount("SafeBrowsing.AccountIntegration.DisabledDialog."
                               "SettingsButtonClicked"),
            0);

  ClickButton(bubble_delegate, bubble_delegate->GetCancelButton());
  EXPECT_EQ(uat.GetActionCount("SafeBrowsing.AccountIntegration.DisabledDialog."
                               "SettingsButtonClicked"),
            1);
}

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogManagerTest,
                       OpeningANewEnableDialogWillCloseAnyOpenDisableDialogs) {
  auto* disabled_dialog = ShowTailoredSecurityDisabledDialog(browser());
  auto* enabled_dialog = ShowTailoredSecurityEnabledDialog(browser());
  EXPECT_TRUE(disabled_dialog->IsClosed());
  EXPECT_FALSE(enabled_dialog->IsClosed());
}

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogManagerTest,
                       ClosingDisabledByOpeningEnabledLogsCloseReason) {
  base::HistogramTester histograms;
  ShowTailoredSecurityDisabledDialog(browser());
  histograms.ExpectBucketCount(safe_browsing::kDisabledDialogOutcome,
                               TailoredSecurityOutcome::kSettings, 0);
  histograms.ExpectBucketCount(safe_browsing::kDisabledDialogOutcome,
                               TailoredSecurityOutcome::kAccepted, 0);

  ShowTailoredSecurityEnabledDialog(browser());

  histograms.ExpectBucketCount(safe_browsing::kDisabledDialogOutcome,
                               TailoredSecurityOutcome::kSettings, 0);
  histograms.ExpectBucketCount(safe_browsing::kDisabledDialogOutcome,
                               TailoredSecurityOutcome::kAccepted, 0);
  histograms.ExpectBucketCount(safe_browsing::kDisabledDialogOutcome,
                               TailoredSecurityOutcome::kClosedByAnotherDialog,
                               1);
}

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogManagerTest,
                       OpeningANewDisableDialogWillCloseAnyOpenEnableDialogs) {
  auto* enabled_dialog = ShowTailoredSecurityEnabledDialog(browser());
  auto* disabled_dialog = ShowTailoredSecurityDisabledDialog(browser());
  EXPECT_TRUE(enabled_dialog->IsClosed());
  EXPECT_FALSE(disabled_dialog->IsClosed());
}

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogManagerTest,
                       ClosingEnabledByOpeningDisabledLogsCloseReason) {
  base::HistogramTester histograms;
  ShowTailoredSecurityEnabledDialog(browser());
  histograms.ExpectBucketCount(safe_browsing::kEnabledDialogOutcome,
                               TailoredSecurityOutcome::kSettings, 0);
  histograms.ExpectBucketCount(safe_browsing::kEnabledDialogOutcome,
                               TailoredSecurityOutcome::kAccepted, 0);

  ShowTailoredSecurityDisabledDialog(browser());

  histograms.ExpectBucketCount(safe_browsing::kEnabledDialogOutcome,
                               TailoredSecurityOutcome::kSettings, 0);
  histograms.ExpectBucketCount(safe_browsing::kEnabledDialogOutcome,
                               TailoredSecurityOutcome::kAccepted, 0);
  histograms.ExpectBucketCount(safe_browsing::kEnabledDialogOutcome,
                               TailoredSecurityOutcome::kClosedByAnotherDialog,
                               1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         TailoredSecurityDesktopDialogManagerTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);
