// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class ExtendedUpdatesBrowserTest : public WebUIMochaBrowserTest {
 protected:
  ExtendedUpdatesBrowserTest() {
    set_test_loader_host(chrome::kChromeUIExtendedUpdatesDialogHost);
  }

  void SetUpOnMainThread() override {
    WebUIMochaBrowserTest::SetUpOnMainThread();

    // The ExtendedUpdates webui checks that the user is the owner before
    // allowing the page to open. That ownership check depends on encryption
    // keys being loaded, which happens asynchronously, so we need to
    // wait for it to finish loading before trying to open the page.
    WaitForIsOwner();
  }

  void WaitForIsOwner() {
    base::RunLoop run_loop;
    auto* owner_settings =
        ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
            browser()->profile());
    ASSERT_TRUE(owner_settings);
    owner_settings->IsOwnerAsync(base::BindLambdaForTesting(
        [&run_loop](bool is_owner) { run_loop.Quit(); }));
    run_loop.Run();
  }

  void RunTestFile(const std::string& test_file) {
    RunTest(base::StrCat({"chromeos/extended_updates/", test_file}),
            "mocha.run()");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      ash::features::kExtendedUpdatesOptInFeature};
};

IN_PROC_BROWSER_TEST_F(ExtendedUpdatesBrowserTest, AppTest) {
  RunTestFile("extended_updates_app_test.js");
}
