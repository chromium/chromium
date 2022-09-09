// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/safe_browsing/tailored_security_desktop_dialog.h"
#include "chrome/common/chrome_features.h"
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

views::Widget* ShowTailoredSecurityEnabledDialog(Browser* browser) {
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      safe_browsing::kTailoredSecurityNoticeDialog);
  safe_browsing::ShowEnabledDialogForBrowser(browser);

  return waiter.WaitIfNeededAndGet();
}

views::Widget* ShowTailoredSecurityDisabledDialog(Browser* browser) {
  views::NamedWidgetShownWaiter waiter(
      views::test::AnyWidgetTestPasskey{},
      safe_browsing::kTailoredSecurityNoticeDialog);
  safe_browsing::ShowDisabledDialogForBrowser(browser);

  return waiter.WaitIfNeededAndGet();
}

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
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, center, center,
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  button->OnMousePressed(event);
  button->OnMouseReleased(event);
}

}  // namespace

class TailoredSecurityDesktopDialogTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<TestParam> {
 public:
  TailoredSecurityDesktopDialogTest() {
    if (GetParam().use_dark_theme) {
      features_.InitAndEnableFeature(features::kWebUIDarkMode);
    } else {
      features_.Init();
    }
  }

  TailoredSecurityDesktopDialogTest(const TailoredSecurityDesktopDialogTest&) =
      delete;
  TailoredSecurityDesktopDialogTest& operator=(
      const TailoredSecurityDesktopDialogTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Reduce flakes by ensuring that animation is disabled.
    ui::ScopedAnimationDurationScaleMode disable_animation(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    const std::string& actual_name = name.substr(0, name.find("/"));
    if (actual_name == "enabledDialog") {
      safe_browsing::ShowEnabledDialogForBrowser(browser());
    } else if (actual_name == "disabledDialog") {
      safe_browsing::ShowDisabledDialogForBrowser(browser());
    } else {
      FAIL() << "No dialog case defined for this string: " << name;
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam().use_dark_theme) {
      command_line->AppendSwitch(switches::kForceDarkMode);
    }
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogTest,
                       InvokeUi_enabledDialog) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogTest,
                       InvokeUi_disabledDialog) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogTest,
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

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogTest,
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

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogTest,
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

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogTest,
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

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogTest,
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

IN_PROC_BROWSER_TEST_P(TailoredSecurityDesktopDialogTest,
                       DisabledDialogCancelButtonNavigatesToSettings) {
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

INSTANTIATE_TEST_SUITE_P(All,
                         TailoredSecurityDesktopDialogTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);
