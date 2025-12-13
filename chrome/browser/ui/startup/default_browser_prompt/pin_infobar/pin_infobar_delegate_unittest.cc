// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_delegate.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace default_browser {

class PinInfoBarDelegateTest : public testing::Test {
 protected:
  PinInfoBarDelegateTest() {
    feature_list_.InitAndEnableFeature(features::kOfferPinToTaskbarInfoBar);
  }

  void SetUp() override {
    infobar_manager_ =
        std::make_unique<infobars::ContentInfoBarManager>(web_contents_.get());
  }

  infobars::ContentInfoBarManager* infobar_manager() {
    return infobar_manager_.get();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  // Must be the first member.
  content::BrowserTaskEnvironment task_environment_;

  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<infobars::ContentInfoBarManager> infobar_manager_;
  ChromeLayoutProvider layout_provider_;
  TestingProfile profile_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  std::unique_ptr<content::WebContents> web_contents_{
      content::WebContentsTester::CreateTestWebContents(
          content::WebContents::CreateParams(&profile_))};
};

// Executes the code to ensure that creating the infobar doesn't crash.
TEST_F(PinInfoBarDelegateTest, Create) {
  ASSERT_TRUE(infobar_manager()->infobars().empty());
  EXPECT_TRUE(PinInfoBarDelegate::Create(infobar_manager()));
  EXPECT_EQ(1u, infobar_manager()->infobars().size());
}

// When the infobar is accepted, the "accepted" histogram should be recorded.
TEST_F(PinInfoBarDelegateTest, AcceptHistogram) {
  infobars::InfoBar* infobar = PinInfoBarDelegate::Create(infobar_manager());
  ASSERT_TRUE(static_cast<PinInfoBarDelegate*>(infobar->delegate())->Accept());
  histogram_tester().ExpectUniqueSample(
      "DefaultBrowser.PinInfoBar.UserInteraction",
      PinInfoBarUserInteraction::kAccepted, 1);
}

// When the infobar is dismissed, the "dismissed" histogram should be recorded.
TEST_F(PinInfoBarDelegateTest, DismissedHistogram) {
  infobars::InfoBar* infobar = PinInfoBarDelegate::Create(infobar_manager());
  static_cast<PinInfoBarDelegate*>(infobar->delegate())->InfoBarDismissed();
  histogram_tester().ExpectUniqueSample(
      "DefaultBrowser.PinInfoBar.UserInteraction",
      PinInfoBarUserInteraction::kDismissed, 1);
}

// When the infobar is destroyed without being accepted or dismissed, the
// "ignored" histogram should be recorded.
TEST_F(PinInfoBarDelegateTest, IgnoredHistogram) {
  infobars::InfoBar* infobar = PinInfoBarDelegate::Create(infobar_manager());
  infobar_manager()->RemoveInfoBar(infobar);
  histogram_tester().ExpectUniqueSample(
      "DefaultBrowser.PinInfoBar.UserInteraction",
      PinInfoBarUserInteraction::kIgnored, 1);
}

// When the infobar is destroyed after being dismissed, the "dismissed"
// histogram (not the "ignored" histogram) should be recorded.
TEST_F(PinInfoBarDelegateTest, DismissedHistogramInfoBarDestroyed) {
  infobars::InfoBar* infobar = PinInfoBarDelegate::Create(infobar_manager());
  static_cast<PinInfoBarDelegate*>(infobar->delegate())->InfoBarDismissed();
  infobar_manager()->RemoveInfoBar(infobar);
  histogram_tester().ExpectUniqueSample(
      "DefaultBrowser.PinInfoBar.UserInteraction",
      PinInfoBarUserInteraction::kDismissed, 1);
}

}  // namespace default_browser
