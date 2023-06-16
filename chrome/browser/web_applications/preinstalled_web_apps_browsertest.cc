// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"

#include "base/test/bind.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#include "chrome/browser/web_applications/preinstalled_web_app_config_utils.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/with_crosapi_param.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using web_app::test::CrosapiParam;
using web_app::test::WithCrosapiParam;

namespace web_app {

class PreinstalledWebAppsBrowserTest : public WebAppControllerBrowserTest,
                                       public WithCrosapiParam {
 public:
  PreinstalledWebAppsBrowserTest() {
    PreinstalledWebAppManager::SkipStartupForTesting();
    // Ignore any default app configs on disk.
    SetPreinstalledWebAppConfigDirForTesting(&empty_path_);
    WebAppProvider::SetOsIntegrationManagerFactoryForTesting(
        [](Profile* profile) -> std::unique_ptr<OsIntegrationManager> {
          return std::make_unique<FakeOsIntegrationManager>(
              profile, nullptr, nullptr, nullptr, nullptr);
        });
  }

  ~PreinstalledWebAppsBrowserTest() override {
    SetPreinstalledWebAppConfigDirForTesting(nullptr);
  }

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    WebAppControllerBrowserTest::SetUpDefaultCommandLine(command_line);

    // This was added by PrepareBrowserCommandLineForTests(), re-enable default
    // apps as we wish to test that they get installed.
    command_line->RemoveSwitch(switches::kDisableDefaultApps);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUpOnMainThread() override {
    if (browser() == nullptr) {
      // Create a new Ash browser window so test code using browser() can work
      // even when Lacros is the only browser.
      // TODO(crbug.com/1450158): Remove uses of browser() from such tests.
      chrome::NewEmptyWindow(ProfileManager::GetActiveUserProfile());
      SelectFirstBrowser();
    }
    WebAppControllerBrowserTest::SetUpOnMainThread();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  base::FilePath empty_path_;
};

IN_PROC_BROWSER_TEST_P(PreinstalledWebAppsBrowserTest, CheckInstalledFields) {
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
        "https://calendar.google.com/calendar/installwebapp?usp=chrome_default",
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
              std::map<GURL, bool> uninstall_results) {
            if (GetParam() == test::CrosapiParam::kDisabled) {
              EXPECT_EQ(install_results.size(),
                        kOfflineOnlyExpectedCount + kOnlineOnlyExpectedCount);

              for (const auto& expectation : kOfflineOnlyExpectations) {
                EXPECT_EQ(
                    install_results[GURL(expectation.install_url)].code,
                    webapps::InstallResultCode::kSuccessOfflineOnlyInstall);
              }

              for (const auto& expectation : kOnlineOnlyExpectations) {
                EXPECT_EQ(install_results[GURL(expectation.install_url)].code,
                          webapps::InstallResultCode::kInstallURLLoadFailed);
              }
            } else {
              EXPECT_EQ(install_results.size(), 0u);
            }

            EXPECT_EQ(uninstall_results.size(), 0u);

            run_loop.Quit();
          }));
  run_loop.Run();

  for (const auto& expectation : kOfflineOnlyExpectations) {
    if (GetParam() == test::CrosapiParam::kDisabled) {
      EXPECT_EQ(provider.registrar_unsafe().GetAppLaunchUrl(expectation.app_id),
                GURL(expectation.launch_url));
    } else {
      EXPECT_FALSE(provider.registrar_unsafe().GetAppById(expectation.app_id));
    }
  }

  // Note that default web apps *DO* show app icons on Chrome OS however it is
  // done via the |WebApps| publishing live our current app state to the app
  // service rather than writing shortcut files as the case on all other desktop
  // platforms.
  auto* fake_os_integration_manager =
      provider.os_integration_manager().AsTestOsIntegrationManager();
  EXPECT_EQ(fake_os_integration_manager->num_create_shortcuts_calls(), 0u);
  EXPECT_EQ(fake_os_integration_manager->num_create_file_handlers_calls(), 0u);
  EXPECT_EQ(fake_os_integration_manager->num_register_run_on_os_login_calls(),
            0u);
  EXPECT_EQ(
      fake_os_integration_manager->num_add_app_to_quick_launch_bar_calls(), 0u);
  EXPECT_FALSE(fake_os_integration_manager->did_add_to_desktop());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PreinstalledWebAppsBrowserTest,
                         ::testing::Values(
#if BUILDFLAG(IS_CHROMEOS_ASH)
                             CrosapiParam::kEnabled,
#endif
                             CrosapiParam::kDisabled),
                         WithCrosapiParam::ParamToString);

}  // namespace web_app
