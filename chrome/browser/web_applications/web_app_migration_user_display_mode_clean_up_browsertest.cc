// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_migration_user_display_mode_clean_up.h"

#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"

namespace {

const char kAppId[] = "dofnemchnjfeendjmdhaldenaiabpiad";
const char kAppName[] = "Test App";
const char kStartUrl[] = "https://test.com";

size_t GetTestPreCount() {
  constexpr base::StringPiece kPreTestPrefix = "PRE_";
  base::StringPiece test_name =
      testing::UnitTest::GetInstance()->current_test_info()->name();
  size_t count = 0;
  while (test_name.find(kPreTestPrefix, kPreTestPrefix.size() * count) ==
         kPreTestPrefix.size() * count) {
    ++count;
  }
  return count;
}

}  // namespace

namespace web_app {

class WebAppMigrationUserDisplayModeCleanUpBrowserTest
    : public InProcessBrowserTest {
 public:
  WebAppMigrationUserDisplayModeCleanUpBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {
            features::kDesktopPWAsWithoutExtensions,
            features::kDesktopPWAsMigrationUserDisplayModeCleanUp,
        },
        {});
    switch (GetTestPreCount()) {
      case 2:
        WebAppMigrationUserDisplayModeCleanUp::DisableForTesting();
        break;
      case 1:
        WebAppMigrationUserDisplayModeCleanUp::SkipWaitForSyncForTesting();
        WebAppMigrationUserDisplayModeCleanUp::SetCompletedCallbackForTesting(
            base::BindLambdaForTesting([this]() {
              clean_up_completed_ = true;
              if (completed_callback_)
                std::move(completed_callback_).Run();
            }));
        break;
    }
  }

  ~WebAppMigrationUserDisplayModeCleanUpBrowserTest() override = default;

  Profile* profile() { return browser()->profile(); }
  WebAppProvider& provider() { return *WebAppProvider::Get(profile()); }

 protected:
  bool clean_up_completed_ = false;
  base::OnceClosure completed_callback_;
  base::HistogramTester histograms_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppMigrationUserDisplayModeCleanUpBrowserTest,
                       PRE_PRE_CleanUp) {
  // Clean up must be disabled for this stage.
  ASSERT_FALSE(WebAppMigrationUserDisplayModeCleanUp::CreateIfNeeded(profile(),
                                                                     nullptr));

  InstallFinalizer& web_app_finalizer = provider().install_finalizer();
  InstallFinalizer* bookmark_app_finalizer =
      web_app_finalizer.legacy_finalizer_for_testing();
  ASSERT_TRUE(bookmark_app_finalizer);

  InstallFinalizer::FinalizeOptions options;
  options.install_source = WebappInstallSource::OMNIBOX_INSTALL_ICON;

  // Install bookmark app set to open as window.
  {
    base::RunLoop run_loop;
    WebApplicationInfo info;
    info.start_url = GURL(kStartUrl);
    info.title = base::UTF8ToUTF16(kAppName);
    info.open_as_window = true;
    bookmark_app_finalizer->FinalizeInstall(
        info, options,
        base::BindLambdaForTesting(
            [&](const AppId& app_id, InstallResultCode code) {
              EXPECT_EQ(app_id, kAppId);
              EXPECT_EQ(code, InstallResultCode::kSuccessNewInstall);
              run_loop.Quit();
            }));
    run_loop.Run();

    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile())
            ->enabled_extensions()
            .GetByID(kAppId);
    ASSERT_TRUE(extension);
    EXPECT_EQ(extensions::GetLaunchContainer(
                  extensions::ExtensionPrefs::Get(profile()), extension),
              extensions::LaunchContainer::kLaunchContainerWindow);
  }

  // Install WebApp set to open as browser tab.
  {
    bookmark_app_finalizer = nullptr;
    web_app_finalizer.RemoveLegacyInstallFinalizerForTesting();

    base::RunLoop run_loop;
    WebApplicationInfo info;
    info.start_url = GURL(kStartUrl);
    info.title = base::UTF8ToUTF16(kAppName);
    info.open_as_window = false;
    web_app_finalizer.FinalizeInstall(
        info, options,
        base::BindLambdaForTesting(
            [&](const AppId& app_id, InstallResultCode code) {
              EXPECT_EQ(app_id, kAppId);
              EXPECT_EQ(code, InstallResultCode::kSuccessNewInstall);
              run_loop.Quit();
            }));
    run_loop.Run();

    EXPECT_EQ(provider().registrar().GetAppUserDisplayMode(kAppId),
              DisplayMode::kBrowser);
  }
}

IN_PROC_BROWSER_TEST_F(WebAppMigrationUserDisplayModeCleanUpBrowserTest,
                       PRE_CleanUp) {
  // Wait for clean up to complete (this will timeout if we don't run clean up).
  if (!clean_up_completed_) {
    base::RunLoop run_loop;
    completed_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Web app should now open in a window.
  EXPECT_EQ(provider().registrar().GetAppUserDisplayMode(kAppId),
            DisplayMode::kStandalone);
  histograms_.ExpectBucketCount("WebApp.Migration.UserDisplayModeCleanUp",
                                /*BooleanMigrated=*/true, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppMigrationUserDisplayModeCleanUpBrowserTest,
                       CleanUp) {
  // Check that clean up is not needed anymore.
  EXPECT_FALSE(WebAppMigrationUserDisplayModeCleanUp::CreateIfNeeded(profile(),
                                                                     nullptr));
}

}  // namespace web_app
