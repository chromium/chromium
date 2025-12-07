// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_delegate.h"

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace session_restore_infobar {

namespace {

struct SessionRestoreInfoBarTestParams {
  SessionRestoreInfoBarDelegate::InfobarMessageType message_type;
  const char* histogram_name;
};

}  // namespace

class SessionRestoreInfoBarDelegateTest
    : public testing::TestWithParam<SessionRestoreInfoBarTestParams> {
 protected:
  SessionRestoreInfoBarDelegateTest()
      : web_contents_(content::WebContentsTester::CreateTestWebContents(
            content::WebContents::CreateParams(profile()))) {}

  void SetUp() override {
    infobars::ContentInfoBarManager::CreateForWebContents(web_contents_.get());
  }

  infobars::InfoBar* CreateDelegate() {
    return SessionRestoreInfoBarDelegate::Show(infobar_manager(), *profile(),
                                               base::DoNothing(),
                                               GetParam().message_type);
  }

  infobars::ContentInfoBarManager* infobar_manager() {
    return infobars::ContentInfoBarManager::FromWebContents(
        web_contents_.get());
  }

  TestingProfile* profile() { return &profile_; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  // Must be the first member.
  content::BrowserTaskEnvironment task_environment_;
  ChromeLayoutProvider layout_provider_;
  base::HistogramTester histogram_tester_;
  TestingProfile profile_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  std::unique_ptr<content::WebContents> web_contents_;
};

// Executes the code to ensure that creating the infobar doesn't crash. When the
// infobar is created, the "shown" histogram should be recorded.
TEST_P(SessionRestoreInfoBarDelegateTest, Create) {
  CreateDelegate();
  histogram_tester().ExpectUniqueSample(
      GetParam().histogram_name,
      SessionRestoreInfoBarDelegate::InfobarAction::kShown, 1);
}

// When the infobar is dismissed, the "dismissed" histogram should be recorded.
TEST_P(SessionRestoreInfoBarDelegateTest, DismissedHistogram) {
  infobars::InfoBar* infobar = CreateDelegate();
  infobar->delegate()->InfoBarDismissed();
  histogram_tester().ExpectBucketCount(
      GetParam().histogram_name,
      SessionRestoreInfoBarDelegate::InfobarAction::kDismissed, 1);
}

// When the link is clicked, the "link clicked" histogram should be recorded.
TEST_P(SessionRestoreInfoBarDelegateTest, LinkClickedHistogram) {
  infobars::InfoBar* infobar = CreateDelegate();
  infobar->delegate()->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
  histogram_tester().ExpectBucketCount(
      GetParam().histogram_name,
      SessionRestoreInfoBarDelegate::InfobarAction::kLinkClicked, 1);
}

// When the infobar is destroyed without being accepted or dismissed, the
// "ignored" histogram should be recorded.
TEST_P(SessionRestoreInfoBarDelegateTest, IgnoredHistogram) {
  infobars::InfoBar* infobar = CreateDelegate();
  infobar_manager()->RemoveInfoBar(infobar);
  histogram_tester().ExpectBucketCount(
      GetParam().histogram_name,
      SessionRestoreInfoBarDelegate::InfobarAction::kIgnored, 1);
}

// When the infobar is destroyed after being dismissed, the "dismissed"
// histogram (not the "ignored" histogram) should be recorded.
TEST_P(SessionRestoreInfoBarDelegateTest, DismissedHistogramInfoBarDestroyed) {
  infobars::InfoBar* infobar = CreateDelegate();
  infobar->delegate()->InfoBarDismissed();
  infobar_manager()->RemoveInfoBar(infobar);
  histogram_tester().ExpectBucketCount(
      GetParam().histogram_name,
      SessionRestoreInfoBarDelegate::InfobarAction::kDismissed, 1);
  histogram_tester().ExpectBucketCount(
      GetParam().histogram_name,
      SessionRestoreInfoBarDelegate::InfobarAction::kIgnored, 0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SessionRestoreInfoBarDelegateTest,
    testing::Values(
        SessionRestoreInfoBarTestParams{
            SessionRestoreInfoBarDelegate::InfobarMessageType::
                kTurnOffFromRestart,
            "SessionRestore.InfoBar.TurnOffFromRestart"},
        SessionRestoreInfoBarTestParams{
            SessionRestoreInfoBarDelegate::InfobarMessageType::
                kTurnOnSessionRestore,
            "SessionRestore.InfoBar.TurnOnSessionRestore"}));

}  // namespace session_restore_infobar
