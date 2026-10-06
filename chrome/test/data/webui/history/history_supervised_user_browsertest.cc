// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/android_buildflags.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/common/pref_names.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_DESKTOP_ANDROID)
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)

class HistorySupervisedUserTest : public WebUIMochaBrowserTest {
 public:
  HistorySupervisedUserTest() : history_(nullptr) {
    set_test_loader_host(chrome::kChromeUIHistoryHost);
  }

  void SetUpOnMainThread() override {
    WebUIMochaBrowserTest::SetUpOnMainThread();

    history_ = HistoryServiceFactory::GetForProfile(
        chrome_test_utils::GetProfile(this),
        ServiceAccessType::EXPLICIT_ACCESS);
    history::BlockUntilHistoryProcessesPendingRequests(history_);
  }

  void TearDownOnMainThread() override {
    history_ = nullptr;
    WebUIMochaBrowserTest::TearDownOnMainThread();
  }

 protected:
  // Sets the pref to allow or prohibit deleting history entries.
  void SetDeleteAllowed(bool allowed) {
    chrome_test_utils::GetProfile(this)->GetPrefs()->SetBoolean(
        prefs::kAllowDeletingBrowserHistory, allowed);
  }

 private:
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  // On Desktop Android the History WebUI is still behind a flag, so the
  // tests have to turn it on themselves.
  base::test::ScopedFeatureList scoped_feature_list_{
      chrome::android::kAndroidDesktopWebUiHistory};
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)

  // The HistoryService is owned by the profile.
  raw_ptr<history::HistoryService> history_ = nullptr;
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
