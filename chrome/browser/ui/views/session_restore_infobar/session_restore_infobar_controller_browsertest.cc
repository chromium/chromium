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
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

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
  void CreateInfobar() {
    auto controller = std::make_unique<SessionRestoreInfobarController>(
        *browser()->profile(), false, false);
    controller->CreateOrDestroySessionRestoreInfobar(*web_contents_);
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
  CreateInfobar();
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

  CreateInfobar();
  EXPECT_EQ(1u, infobar_manager_->infobars().size());
}

// Test that the session restore infobar is not shown when the user's
// preferences are set to open the new tab page.
IN_PROC_BROWSER_TEST_F(SessionRestoreInfobarControllerBrowserTest,
                       InfobarNotShownForOtherSettings) {
  browser()->profile()->GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 4);

  CreateInfobar();
  EXPECT_EQ(0u, infobar_manager_->infobars().size());
}

// Test that the session restore infobar has the correct message value when the
// browser session is restored.
IN_PROC_BROWSER_TEST_F(SessionRestoreInfobarControllerBrowserTest,
                       InfobarMessageValueForRestart) {
  browser()->profile()->GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 1);
  CreateInfobar();

  EXPECT_EQ(1u, infobar_manager_->infobars().size());
  SessionRestoreInfoBarDelegate* delegate = GetDelegate();
  ASSERT_TRUE(delegate);
  EXPECT_EQ(delegate->GetMessageText(),
            l10n_util::GetStringUTF16(
                IDS_SESSION_RESTORE_TURN_OFF_RESTORE_FROM_SESSION));
}

}  // namespace session_restore_infobar
