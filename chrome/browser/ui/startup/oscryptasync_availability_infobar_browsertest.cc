// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/browser_infobar_manager.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/startup/oscryptasync_availability_infobar_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

// Runs against the legacy infobar and the centralized framework; behavior
// must match. The availability check runs long before this infobar shows, so
// the test drives the show path directly.
class OSCryptAsyncAvailabilityInfoBarBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  OSCryptAsyncAvailabilityInfoBarBrowserTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeatureWithParameters(
          infobars::kCentralizedInfoBarFramework,
          {{"MigratedOSCryptAsyncAvailability", "true"}});
    } else {
      feature_list_.InitAndDisableFeature(
          infobars::kCentralizedInfoBarFramework);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         OSCryptAsyncAvailabilityInfoBarBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "MigratedInfobar"
                                             : "LegacyInfobar";
                         });

IN_PROC_BROWSER_TEST_P(OSCryptAsyncAvailabilityInfoBarBrowserTest,
                       ShowsTheRelaunchPromptOnTheActiveTab) {
  auto* infobar_manager = infobars::ContentInfoBarManager::FromWebContents(
      GetBrowserWindowInterface()->GetTabStripModel()->GetActiveWebContents());
  if (GetParam()) {
    auto* browser_infobar_manager =
        infobars::BrowserInfoBarManager::From(g_browser_process);
    ASSERT_TRUE(browser_infobar_manager);
    ASSERT_TRUE(browser_infobar_manager->Show(
        GetBrowserWindowInterface()->GetTabStripModel()->GetActiveTab(),
        infobars::InfoBarDelegate::OSCRYPTASYNC_AVAILABILITY_INFOBAR_DELEGATE));
  } else {
    OSCryptAsyncAvailabilityInfoBarDelegate::Create(infobar_manager);
  }
  ASSERT_EQ(1u, infobar_manager->infobars().size());
  auto* delegate =
      infobar_manager->infobars()[0]->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(delegate);
  EXPECT_EQ(
      infobars::InfoBarDelegate::OSCRYPTASYNC_AVAILABILITY_INFOBAR_DELEGATE,
      delegate->GetIdentifier());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_OSCRYPTASYNC_AVAILABILITY_INFOBAR_MESSAGE),
      delegate->GetMessageText());
  EXPECT_EQ(ConfirmInfoBarDelegate::BUTTON_OK, delegate->GetButtons());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_OSCRYPTASYNC_AVAILABILITY_INFOBAR_BUTTON),
      delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));
  EXPECT_FALSE(delegate->IsCloseable());
}
