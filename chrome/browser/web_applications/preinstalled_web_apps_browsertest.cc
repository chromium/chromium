// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"

#include "ash/constants/web_app_id_constants.h"
#include "base/test/bind.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#include "chrome/browser/web_applications/preinstalled_web_app_config_utils.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace web_app {

class PreinstalledWebAppsBrowserTest : public WebAppBrowserTestBase {
 public:
  PreinstalledWebAppsBrowserTest()
      : skip_preinstalled_web_app_startup_(
            PreinstalledWebAppManager::SkipStartupForTesting()) {
    // Ignore any default app configs on disk.
    SetPreinstalledWebAppConfigDirForTesting(&empty_path_);
  }

  ~PreinstalledWebAppsBrowserTest() override {
    SetPreinstalledWebAppConfigDirForTesting(nullptr);
  }

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    WebAppBrowserTestBase::SetUpDefaultCommandLine(command_line);

    // This was added by PrepareBrowserCommandLineForTests(), re-enable default
    // apps as we wish to test that they get installed.
    command_line->RemoveSwitch(switches::kDisableDefaultApps);
  }

  base::FilePath empty_path_;
  base::AutoReset<bool> skip_preinstalled_web_app_startup_;
};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && BUILDFLAG(IS_CHROMEOS)
// TODO(http://crbug.com/328691719): Test is flaky on CHROMEOS.
#define MAYBE_CheckInstalledFields DISABLED_CheckInstalledFields
#else
#define MAYBE_CheckInstalledFields CheckInstalledFields
#endif
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppsBrowserTest,
                       MAYBE_CheckInstalledFields) {
  base::AutoReset<bool> scope =
      SetPreinstalledAppInstallFeatureAlwaysEnabledForTesting();

  auto& provider = *WebAppProvider::GetForTest(browser()->profile());
  struct OfflineOnlyExpectation {
    const char* app_id;
    const char* install_url;
    const char* launch_url;
  } kOfflineOnlyExpectations[] = {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if BUILDFLAG(IS_CHROMEOS)
      {
          kGoogleCalendarAppId,
          "https://calendar.google.com/calendar/"
          "installwebapp?usp=chrome_default",
          "https://calendar.google.com/calendar/r?usp=installed_webapp",
      },
#endif  // BUILDFLAG(IS_CHROMEOS)
      {
          kGoogleDocsAppId,
          "https://docs.google.com/document/installwebapp?usp=chrome_default",
          "https://docs.google.com/document/?usp=installed_webapp",
      },
      {
          kGoogleSlidesAppId,
          "https://docs.google.com/presentation/"
          "installwebapp?usp=chrome_default",
          "https://docs.google.com/presentation/?usp=installed_webapp",
      },
      {
          kGoogleSheetsAppId,
          "https://docs.google.com/spreadsheets/"
          "installwebapp?usp=chrome_default",
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
  size_t kOfflineOnlyExpectedCount =
      sizeof(kOfflineOnlyExpectations) / sizeof(kOfflineOnlyExpectations[0]);

  struct OnlineOnlyExpectation {
    const char* install_url;
  } kOnlineOnlyExpectations[] = {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if BUILDFLAG(IS_CHROMEOS)
      {
          "https://mail.google.com/chat/download?usp=chrome_default",
      },
      {
          "https://meet.google.com/download/webapp?usp=chrome_default",
      },
      {
          "https://calculator.apps.chrome/install",
      },
      {
          "https://discover.apps.chrome/install/",
      },
#endif  // BUILDFLAG(IS_CHROMEOS)
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  };
  size_t kOnlineOnlyExpectedCount =
      sizeof(kOnlineOnlyExpectations) / sizeof(kOnlineOnlyExpectations[0]);

  base::RunLoop run_loop;
  provider.preinstalled_web_app_manager().LoadAndSynchronizeForTesting(
      base::BindLambdaForTesting(
          [&](std::map<GURL, ExternallyManagedAppManager::InstallResult>
                  install_results,
              std::map<GURL, webapps::UninstallResultCode> uninstall_results) {
            EXPECT_EQ(install_results.size(),
                      kOfflineOnlyExpectedCount + kOnlineOnlyExpectedCount);

            for (const auto& expectation : kOfflineOnlyExpectations) {
              EXPECT_EQ(install_results[GURL(expectation.install_url)].code,
                        webapps::InstallResultCode::kSuccessOfflineOnlyInstall);
            }

            for (const auto& expectation : kOnlineOnlyExpectations) {
              EXPECT_EQ(install_results[GURL(expectation.install_url)].code,
                        webapps::InstallResultCode::kInstallURLLoadFailed);
            }

            EXPECT_EQ(uninstall_results.size(), 0u);

            run_loop.Quit();
          }));
  run_loop.Run();

  for (const auto& expectation : kOfflineOnlyExpectations) {
    EXPECT_EQ(provider.registrar_unsafe().GetAppLaunchUrl(expectation.app_id),
              GURL(expectation.launch_url));
  }
}

}  // namespace web_app
