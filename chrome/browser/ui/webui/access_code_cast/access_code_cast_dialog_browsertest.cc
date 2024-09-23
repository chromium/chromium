// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_dialog.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class AccessCodeCastDialogBrowserTest : public DialogBrowserTest {
 public:
  AccessCodeCastDialogBrowserTest() = default;
  AccessCodeCastDialogBrowserTest(const AccessCodeCastDialogBrowserTest&) =
      delete;
  AccessCodeCastDialogBrowserTest& operator=(
      const AccessCodeCastDialogBrowserTest&) = delete;
  ~AccessCodeCastDialogBrowserTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContentsAddedObserver observer;

    // Enable AccessCodeCast.
    browser()->profile()->GetPrefs()->SetBoolean(
        media_router::prefs::kAccessCodeCastEnabled, true);

    // Show the dialog.
    CastModeSet tab_mode = {MediaCastMode::TAB_MIRROR};
    content::WebContents* web_contents =
        chrome_test_utils::GetActiveWebContents(this);
    std::unique_ptr<MediaRouteStarter> starter =
        std::make_unique<MediaRouteStarter>(
            MediaRouterUIParameters(tab_mode, web_contents));
    AccessCodeCastDialog::Show(
        tab_mode, std::move(starter),
        AccessCodeCastDialogOpenLocation::kBrowserCastMenu);
    content::WebContents* dialog_contents = observer.GetWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(dialog_contents));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(b/40261456): Test is consistently failing.
IN_PROC_BROWSER_TEST_F(AccessCodeCastDialogBrowserTest,
                       DISABLED_InvokeUi_default) {
  ShowAndVerifyUi();
}

}  // namespace media_router
