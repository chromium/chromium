// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cookie_controls/roll_back_mode_b_infobar_delegate.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/privacy_sandbox/roll_back_3pcd_notice_action.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class TestWebContentsDelegate : public content::WebContentsDelegate {
 public:
  TestWebContentsDelegate() = default;
  ~TestWebContentsDelegate() override = default;

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    last_url_ = params.url;
    return source;
  }
  GURL last_url() { return last_url_; }

 private:
  GURL last_url_;
};

class RollBackModeBInfoBarDelegateTest : public testing::Test {
 protected:
  void SetUp() override {
    web_contents()->SetDelegate(&delegate_);
    infobar_manager_ =
        std::make_unique<infobars::ContentInfoBarManager>(web_contents());
  }

  infobars::ContentInfoBarManager* infobar_manager() {
    return infobar_manager_.get();
  }
  content::WebContents* web_contents() { return web_contents_.get(); }
  GURL last_url() { return delegate_.last_url(); }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<infobars::ContentInfoBarManager> infobar_manager_;
  ChromeLayoutProvider layout_provider_;
  TestingProfile profile_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestWebContentsDelegate delegate_;
  std::unique_ptr<content::WebContents> web_contents_{
      content::WebContentsTester::CreateTestWebContents(
          content::WebContents::CreateParams(&profile_))};
};

TEST_F(RollBackModeBInfoBarDelegateTest, Properties) {
  RollBackModeBInfoBarDelegate::Create(infobar_manager());
  ASSERT_EQ(infobar_manager()->infobars().size(), 1u);
  auto* delegate =
      infobar_manager()->infobars()[0]->delegate()->AsConfirmInfoBarDelegate();
  EXPECT_EQ(delegate->GetIdentifier(),
            infobars::InfoBarDelegate::InfoBarIdentifier::
                ROLL_BACK_MODE_B_INFOBAR_DELEGATE);
  EXPECT_EQ(&delegate->GetVectorIcon(),
            &vector_icons::kCookieChromeRefreshIcon);
  EXPECT_EQ(delegate->GetMessageText(),
            l10n_util::GetStringUTF16(IDS_MODE_B_ROLLBACK_DESCRIPTION));
  EXPECT_EQ(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK),
            l10n_util::GetStringUTF16(IDS_MODE_B_ROLLBACK_GOT_IT));
  EXPECT_EQ(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL),
            l10n_util::GetStringUTF16(IDS_MODE_B_ROLLBACK_SETTINGS));
}

TEST_F(RollBackModeBInfoBarDelegateTest,
       SettingsButtonRecordsHistogramAndNavigatesToCookieSettings) {
  RollBackModeBInfoBarDelegate::Create(infobar_manager());
  ASSERT_EQ(infobar_manager()->infobars().size(), 1u);
  auto* infobar = infobar_manager()->infobars()[0].get();
  EXPECT_FALSE(infobar->delegate()->AsConfirmInfoBarDelegate()->Cancel());
  EXPECT_EQ(last_url(), GURL(chrome::kChromeUICookieSettingsURL));
  histogram_tester().ExpectUniqueSample("Privacy.3PCD.RollbackNotice.Action",
                                        RollBack3pcdNoticeAction::kSettings, 1);
  infobar->RemoveSelf();
  histogram_tester().ExpectUniqueSample(
      "Privacy.3PCD.RollbackNotice.AutomaticallyDismissed", false, 1);
}

TEST_F(RollBackModeBInfoBarDelegateTest, RecordsHistogramWhenAccepted) {
  RollBackModeBInfoBarDelegate::Create(infobar_manager());
  ASSERT_EQ(infobar_manager()->infobars().size(), 1u);
  auto* infobar = infobar_manager()->infobars()[0].get();
  EXPECT_TRUE(infobar->delegate()->AsConfirmInfoBarDelegate()->Accept());
  histogram_tester().ExpectUniqueSample("Privacy.3PCD.RollbackNotice.Action",
                                        RollBack3pcdNoticeAction::kGotIt, 1);
  infobar->RemoveSelf();
  histogram_tester().ExpectUniqueSample(
      "Privacy.3PCD.RollbackNotice.AutomaticallyDismissed", false, 1);
}

TEST_F(RollBackModeBInfoBarDelegateTest, RecordsHistogramWhenNoActionTaken) {
  RollBackModeBInfoBarDelegate::Create(infobar_manager());
  ASSERT_EQ(infobar_manager()->infobars().size(), 1u);
  infobar_manager()->infobars()[0]->RemoveSelf();
  histogram_tester().ExpectUniqueSample(
      "Privacy.3PCD.RollbackNotice.AutomaticallyDismissed", true, 1);
}

}  // namespace
