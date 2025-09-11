// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_controller.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_delegate.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace session_restore_infobar {

class SessionRestoreInfobarControllerBrowserTest : public InProcessBrowserTest {
 public:
  SessionRestoreInfobarControllerBrowserTest() = default;
  ~SessionRestoreInfobarControllerBrowserTest() override = default;

  SessionRestoreInfobarControllerBrowserTest(
      const SessionRestoreInfobarControllerBrowserTest&) = delete;
  SessionRestoreInfobarControllerBrowserTest& operator=(
      const SessionRestoreInfobarControllerBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    infobar_manager_ =
        infobars::ContentInfoBarManager::FromWebContents(web_contents_);
    ASSERT_TRUE(infobar_manager_);
    EXPECT_EQ(0u, infobar_manager_->infobars().size());
  }

  void TearDownOnMainThread() override {
    web_contents_ = nullptr;
    infobar_manager_ = nullptr;
  }

 protected:
  void CreateInfobar(bool was_restarted, bool is_post_crash_launch) {
    auto controller = std::make_unique<SessionRestoreInfobarController>();
    controller->MaybeShowInfoBar(*browser()->profile(), was_restarted,
                                 is_post_crash_launch);
  }

  content::WebContents* AddNewTab(int index) {
    browser()->tab_strip_model()->InsertWebContentsAt(
        index,
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile())),
        AddTabTypes::ADD_ACTIVE);
    return browser()->tab_strip_model()->GetWebContentsAt(index);
  }

  infobars::ContentInfoBarManager* GetInfoBarManager(
      content::WebContents* web_contents) {
    return infobars::ContentInfoBarManager::FromWebContents(web_contents);
  }

  SessionRestoreInfoBarDelegate* GetDelegate() {
    if (infobar_manager_->infobars().empty()) {
      return nullptr;
    }
    infobars::InfoBar* infobar = infobar_manager_->infobars()[0];
    return static_cast<SessionRestoreInfoBarDelegate*>(infobar->delegate());
  }

  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<infobars::ContentInfoBarManager> infobar_manager_;
};

// Test that the session restore infobar has no value set by the user and the
// untouched session restore preference shows the correct message.
IN_PROC_BROWSER_TEST_F(SessionRestoreInfobarControllerBrowserTest,
                       InfobarUntouchedSessionRestoreDefaultPref) {
  CreateInfobar(false, false);
  EXPECT_EQ(1u, infobar_manager_->infobars().size());

  SessionRestoreInfoBarDelegate* delegate = GetDelegate();
  ASSERT_TRUE(delegate);
  EXPECT_EQ(delegate->GetMessageText(),
            l10n_util::GetStringUTF16(IDS_SESSION_RESTORE_TURN_ON));
}

// Test that the session restore infobar is shown when the user's preferences
// are set to continue where they left off.
IN_PROC_BROWSER_TEST_F(SessionRestoreInfobarControllerBrowserTest,
                       InfobarShownForSessionRestore) {
  browser()->profile()->GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 1);

  CreateInfobar(false, false);
  EXPECT_EQ(1u, infobar_manager_->infobars().size());
}

// Test that the session restore infobar is not shown when the user's
// preferences are set to open the new tab page.
IN_PROC_BROWSER_TEST_F(SessionRestoreInfobarControllerBrowserTest,
                       InfobarNotShownForOtherSettings) {
  browser()->profile()->GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 4);

  CreateInfobar(false, false);
  EXPECT_EQ(0u, infobar_manager_->infobars().size());
}

// Test that the session restore infobar has the correct message value when the
// browser session is restored.
IN_PROC_BROWSER_TEST_F(SessionRestoreInfobarControllerBrowserTest,
                       InfobarMessageValueForRestart) {
  browser()->profile()->GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 1);
  CreateInfobar(true, false);

  EXPECT_EQ(1u, infobar_manager_->infobars().size());
  SessionRestoreInfoBarDelegate* delegate = GetDelegate();
  ASSERT_TRUE(delegate);
  EXPECT_EQ(delegate->GetMessageText(),
            l10n_util::GetStringUTF16(
                IDS_SESSION_RESTORE_TURN_OFF_RESTORE_FROM_RESTART));
}

// Test that the session restore infobar is global and appears on all tabs.
IN_PROC_BROWSER_TEST_F(SessionRestoreInfobarControllerBrowserTest,
                       InfobarIsGlobal) {
  CreateInfobar(false, false);
  EXPECT_EQ(1u, infobar_manager_->infobars().size());

  // Check that the infobar is also shown on new tabs.
  content::WebContents* web_contents2 = AddNewTab(1);
  EXPECT_EQ(1u, GetInfoBarManager(web_contents2)->infobars().size());

  content::WebContents* web_contents3 = AddNewTab(2);
  EXPECT_EQ(1u, GetInfoBarManager(web_contents3)->infobars().size());
}

// Test that dismissing one infobar dismisses all other infobars.
IN_PROC_BROWSER_TEST_F(SessionRestoreInfobarControllerBrowserTest,
                       DismissOneInfobarDismissesAll) {
  CreateInfobar(false, false);
  EXPECT_EQ(1u, infobar_manager_->infobars().size());

  content::WebContents* web_contents2 = AddNewTab(1);
  infobars::ContentInfoBarManager* infobar_manager2 =
      GetInfoBarManager(web_contents2);
  EXPECT_EQ(1u, infobar_manager2->infobars().size());

  content::WebContents* web_contents3 = AddNewTab(2);
  infobars::ContentInfoBarManager* infobar_manager3 =
      GetInfoBarManager(web_contents3);
  EXPECT_EQ(1u, infobar_manager3->infobars().size());

  // All infobars are removed.
  SessionRestoreInfoBarManager::GetInstance()->CloseAllInfoBars();

  EXPECT_EQ(0u, infobar_manager_->infobars().size());
  EXPECT_EQ(0u, infobar_manager2->infobars().size());
  EXPECT_EQ(0u, infobar_manager3->infobars().size());
}

}  // namespace session_restore_infobar
