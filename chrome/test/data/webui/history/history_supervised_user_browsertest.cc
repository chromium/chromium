// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

class HistorySupervisedUserTest : public WebUIMochaBrowserTest {
 public:
  HistorySupervisedUserTest() : history_(nullptr) {
    set_test_loader_host(chrome::kChromeUIHistoryHost);
  }

  void SetUpOnMainThread() override {
    WebUIMochaBrowserTest::SetUpOnMainThread();

    history_ = HistoryServiceFactory::GetForProfile(
        browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);
    ui_test_utils::WaitForHistoryToLoad(history_);
  }

 protected:
  // Sets the pref to allow or prohibit deleting history entries.
  void SetDeleteAllowed(bool allowed) {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kAllowDeletingBrowserHistory, allowed);
  }

 private:
  // The HistoryService is owned by the profile.
  raw_ptr<history::HistoryService, DanglingUntriaged> history_;
};

#if BUILDFLAG(IS_MAC)
#define MAYBE_AllSupervised DISABLED_All
#else
#define MAYBE_AllSupervised All
#endif
IN_PROC_BROWSER_TEST_F(HistorySupervisedUserTest, MAYBE_AllSupervised) {
  SetDeleteAllowed(false);
  RunTest("history/history_supervised_user_test.js", "mocha.run()");
}
