// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"

#include "base/test/bind.h"
#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/components/external_app_install_features.h"
#include "chrome/browser/web_applications/components/web_app_id_constants.h"
#include "chrome/browser/web_applications/external_web_app_manager.h"
#include "chrome/browser/web_applications/test/test_os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace web_app {

class PreinstalledWebAppsBrowserTest : public InProcessBrowserTest {
 public:
  PreinstalledWebAppsBrowserTest() {
    ExternalWebAppManager::SkipStartupForTesting();
    // Ignore any default app configs on disk.
    ExternalWebAppManager::SetConfigDirForTesting(&empty_path_);
    ForceUsePreinstalledWebAppsForTesting();
    WebAppProvider::SetOsIntegrationManagerFactoryForTesting(
        [](Profile* profile) -> std::unique_ptr<OsIntegrationManager> {
          return std::make_unique<TestOsIntegrationManager>(
              profile, nullptr, nullptr, nullptr, nullptr);
        });
  }

  base::FilePath empty_path_;
};

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppsBrowserTest, CheckInstalledFields) {
  base::AutoReset<bool> scope =
      SetExternalAppInstallFeatureAlwaysEnabledForTesting();

  auto& provider = *WebAppProvider::Get(browser()->profile());

  struct Expectation {
    const char* app_id;
    const char* install_url;
    const char* launch_url;
  } kExpectations[] = {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
    {
        kGoogleCalendarAppId,
        "https://calendar.google.com/calendar/installwebapp?usp=chrome_default",
        "https://calendar.google.com/calendar/r?usp=installed_webapp",
    },
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
    {
        kGoogleDocsAppId,
        "https://docs.google.com/document/installwebapp?usp=chrome_default",
        "https://docs.google.com/document/?usp=installed_webapp",
    },
    {
        kGoogleSlidesAppId,
        "https://docs.google.com/presentation/installwebapp?usp=chrome_default",
        "https://docs.google.com/presentation/?usp=installed_webapp",
    },
    {
        kGoogleSheetsAppId,
        "https://docs.google.com/spreadsheets/installwebapp?usp=chrome_default",
        "https://docs.google.com/spreadsheets/?usp=installed_webapp",
    },
    {
        kGoogleDriveAppId,
        "https://drive.google.com/drive/installwebapp?usp=chrome_default",
        "https://drive.google.com/?lfhs=2&usp=installed_webapp",
    },
    {
        kGmailAppId,
        "https://mail.google.com/mail/installwebapp?usp=chrome_default",
        "https://mail.google.com/mail/?usp=installed_webapp",
    },
    {
        kYoutubeAppId,
        "https://www.youtube.com/s/notifications/manifest/cr_install.html",
        "https://www.youtube.com/?feature=ytca",
    },
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  };
  size_t kExpectedCount = sizeof(kExpectations) / sizeof(kExpectations[0]);

  base::RunLoop run_loop;
  provider.external_web_app_manager().LoadAndSynchronizeForTesting(
      base::BindLambdaForTesting(
          [&](std::map<GURL, PendingAppManager::InstallResult> install_results,
              std::map<GURL, bool> uninstall_results) {
            EXPECT_EQ(install_results.size(), kExpectedCount);

            for (const Expectation& expectation : kExpectations) {
              EXPECT_EQ(install_results[GURL(expectation.install_url)].code,
                        InstallResultCode::kSuccessOfflineOnlyInstall);
            }

            EXPECT_EQ(uninstall_results.size(), 0u);

            run_loop.Quit();
          }));
  run_loop.Run();

  for (const Expectation& expectation : kExpectations) {
    EXPECT_EQ(provider.registrar().GetAppLaunchUrl(expectation.app_id),
              GURL(expectation.launch_url));
  }

  // Note that default web apps *DO* show app icons on Chrome OS however it
  // is done via the |WebAppsChromeOs| publishing live our current app state to
  // the app service rather than writing shortcut files as the case on all other
  // desktop platforms.
  auto* test_os_integration_manager =
      provider.os_integration_manager().AsTestOsIntegrationManager();
  EXPECT_EQ(test_os_integration_manager->num_create_shortcuts_calls(), 0u);
  EXPECT_EQ(test_os_integration_manager->num_create_file_handlers_calls(), 0u);
  EXPECT_EQ(test_os_integration_manager->num_register_run_on_os_login_calls(),
            0u);
  EXPECT_EQ(
      test_os_integration_manager->num_add_app_to_quick_launch_bar_calls(), 0u);
  EXPECT_FALSE(test_os_integration_manager->did_add_to_desktop());
}

}  // namespace web_app
