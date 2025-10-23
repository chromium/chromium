// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"

#include "ash/constants/web_app_id_constants.h"
#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#include "chrome/browser/web_applications/preinstalled_web_app_config_utils.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace web_app {

namespace {

web_app::proto::InstallState ExpectedPreinstalledAppInstallState() {
#if BUILDFLAG(IS_CHROMEOS)
  return web_app::proto::InstallState::INSTALLED_WITH_OS_INTEGRATION;
#else
  return web_app::proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION;
#endif
}
}  // namespace

class PreinstalledWebAppsBrowserTest : public WebAppBrowserTestBase {
 public:
  PreinstalledWebAppsBrowserTest()
      :  // Ignore any default app configs on disk.
        config_dir_auto_reset_(
            test::SetPreinstalledWebAppConfigDirForTesting(base::FilePath())),
        skip_preinstalled_web_app_startup_(
            PreinstalledWebAppManager::SkipStartupForTesting()) {}

  ~PreinstalledWebAppsBrowserTest() override = default;

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    WebAppBrowserTestBase::SetUpDefaultCommandLine(command_line);

    // This was added by PrepareBrowserCommandLineForTests(), re-enable default
    // apps as we wish to test that they get installed.
    command_line->RemoveSwitch(switches::kDisableDefaultApps);
  }

  test::ConfigDirAutoReset config_dir_auto_reset_;
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
          ash::kGoogleCalendarAppId,
          "https://calendar.google.com/calendar/"
          "installwebapp?usp=chrome_default",
          "https://calendar.google.com/calendar/r?usp=installed_webapp",
      },
#endif  // BUILDFLAG(IS_CHROMEOS)
      {
          ash::kGoogleDocsAppId,
          "https://docs.google.com/document/installwebapp?usp=chrome_default",
          "https://docs.google.com/document/?usp=installed_webapp",
      },
      {
          ash::kGoogleSlidesAppId,
          "https://docs.google.com/presentation/"
          "installwebapp?usp=chrome_default",
          "https://docs.google.com/presentation/?usp=installed_webapp",
      },
      {
          ash::kGoogleSheetsAppId,
          "https://docs.google.com/spreadsheets/"
          "installwebapp?usp=chrome_default",
          "https://docs.google.com/spreadsheets/?usp=installed_webapp",
      },
      {
          ash::kGoogleDriveAppId,
          "https://drive.google.com/drive/installwebapp?usp=chrome_default",
          "https://drive.google.com/?lfhs=2&usp=installed_webapp",
      },
      {
          ash::kGmailAppId,
          "https://mail.google.com/mail/installwebapp?usp=chrome_default",
          "https://mail.google.com/mail/?usp=installed_webapp",
      },
      {
          ash::kYoutubeAppId,
          "https://www.youtube.com/s/notifications/manifest/cr_install.html",
          "https://www.youtube.com/?feature=ytca",
      },
      {
          ash::kGoogleChatAppId,
          "https://mail.google.com/chat/download?usp=chrome_default",
          "https://mail.google.com/chat/",
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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// All edge cases around the functionality that the preinstalled chat app
// migration relies on are tested below in tests that don't rely on chrome
// branding. This test is more a sanity check to make sure the feature list
// correctly causes the old and/or new app to be installed.
class PreinstalledChatWebAppBrowserTest
    : public PreinstalledWebAppsBrowserTest {
 public:
  PreinstalledChatWebAppBrowserTest() {
    if (GetTestPreCount() == 0) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kWebAppMigratePreinstalledChat);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kWebAppMigratePreinstalledChat);
    }
  }

  void InstallDefaultApps() {
    base::test::TestFuture<
        std::map<GURL /*install_url*/,
                 ExternallyManagedAppManager::InstallResult>,
        std::map<GURL /*install_url*/, webapps::UninstallResultCode>>
        sync_result;
    provider().preinstalled_web_app_manager().LoadAndSynchronizeForTesting(
        sync_result.GetCallback());
    ASSERT_TRUE(sync_result.Wait());
  }

  WebAppProvider& provider() const {
    return *WebAppProvider::GetForTest(browser()->profile());
  }

  const WebAppRegistrar& registrar() const {
    return provider().registrar_unsafe();
  }

 protected:
  const webapps::AppId old_app_id_ =
      GenerateAppIdFromManifestId(GURL("https://mail.google.com/chat/"));
  const webapps::AppId new_app_id_ =
      GenerateAppIdFromManifestId(GURL("https://chat.google.com/"));

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PreinstalledChatWebAppBrowserTest,
                       PRE_PreinstalledOnlyMigrates) {
  InstallDefaultApps();

  ASSERT_TRUE(registrar().IsInstallState(
      old_app_id_, {ExpectedPreinstalledAppInstallState()}));
  EXPECT_TRUE(registrar().GetAppById(old_app_id_)->IsPreinstalledApp());
  EXPECT_FALSE(registrar().IsInRegistrar(new_app_id_));
}

IN_PROC_BROWSER_TEST_F(PreinstalledChatWebAppBrowserTest,
                       PreinstalledOnlyMigrates) {
  InstallDefaultApps();

  EXPECT_FALSE(registrar().IsInRegistrar(old_app_id_));
  ASSERT_TRUE(registrar().IsInstallState(
      new_app_id_, {ExpectedPreinstalledAppInstallState()}));
  EXPECT_TRUE(registrar().GetAppById(new_app_id_)->IsPreinstalledApp());
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

class PreinstalledWebAppMigrationTest : public PreinstalledWebAppsBrowserTest {
 public:
  void InstallDefaultApps(bool install_old_app, bool is_standalone = false) {
    ExternalInstallOptions app_options(
        install_old_app ? old_app_url_ : new_app_url_,
        is_standalone ? mojom::UserDisplayMode::kStandalone
                      : mojom::UserDisplayMode::kBrowser,
        ExternalInstallSource::kExternalDefault);
    app_options.expected_app_id = install_old_app ? old_app_id_ : new_app_id_;
    app_options.user_type_allowlist = {"unmanaged", "managed", "child"};
    app_options.app_info_factory = base::BindRepeating(
        [](webapps::ManifestId manifest_id, GURL start_url,
           bool is_standalone) {
          auto info =
              std::make_unique<WebAppInstallInfo>(manifest_id, start_url);
          info->title = u"Test App";
          info->scope = start_url.GetWithoutFilename();
          info->display_mode =
              is_standalone ? DisplayMode::kStandalone : DisplayMode::kBrowser;
          return info;
        },
        app_options.install_url, app_options.install_url, is_standalone);
    if (!install_old_app) {
      app_options.SetOnlyUninstallAndReplaceWhenCompatible(
          old_app_id_, ExternalInstallOptions::
                           SetOnlyUninstallAndReplaceWhenCompatiblePassKey());
    }

    std::vector<ExternalInstallOptions> preinstall_options;
    preinstall_options.push_back(app_options);
    preinstalled_app_data_.apps = std::move(preinstall_options);

    base::test::TestFuture<
        std::map<GURL /*install_url*/,
                 ExternallyManagedAppManager::InstallResult>,
        std::map<GURL /*install_url*/, webapps::UninstallResultCode>>
        sync_result;
    provider().preinstalled_web_app_manager().LoadAndSynchronizeForTesting(
        sync_result.GetCallback());
    ASSERT_TRUE(sync_result.Wait());
  }

  WebAppProvider& provider() const {
    return *WebAppProvider::GetForTest(browser()->profile());
  }

  const WebAppRegistrar& registrar() const {
    return provider().registrar_unsafe();
  }

 protected:
  GURL old_app_url_ = GURL("https://old.example.com/index.html");
  GURL new_app_url_ = GURL("https://new.example.com/index.html");
  webapps::AppId old_app_id_ = GenerateAppIdFromManifestId(old_app_url_);
  webapps::AppId new_app_id_ = GenerateAppIdFromManifestId(new_app_url_);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedTestingPreinstalledAppData preinstalled_app_data_;
};

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppMigrationTest,
                       PRE_PreinstalledOnlyMigrates) {
  InstallDefaultApps(/*install_old_app=*/true);

  EXPECT_TRUE(registrar().IsInRegistrar(old_app_id_));
  ASSERT_TRUE(registrar().IsInstallState(
      old_app_id_, {ExpectedPreinstalledAppInstallState()}));
  EXPECT_TRUE(registrar().GetAppById(old_app_id_)->IsPreinstalledApp());
  EXPECT_FALSE(registrar().IsInRegistrar(new_app_id_));
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppMigrationTest,
                       PreinstalledOnlyMigrates) {
  InstallDefaultApps(/*install_old_app=*/false);

  EXPECT_FALSE(registrar().IsInRegistrar(old_app_id_));
  ASSERT_TRUE(registrar().IsInstallState(
      new_app_id_, {ExpectedPreinstalledAppInstallState()}));
  EXPECT_TRUE(registrar().GetAppById(new_app_id_)->IsPreinstalledApp());
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppMigrationTest,
                       NewProfileGetsMigratedApp) {
  InstallDefaultApps(/*install_old_app=*/false);

  EXPECT_FALSE(registrar().IsInRegistrar(old_app_id_));
  ASSERT_TRUE(registrar().IsInstallState(
      new_app_id_, {ExpectedPreinstalledAppInstallState()}));
  EXPECT_TRUE(registrar().GetAppById(new_app_id_)->IsPreinstalledApp());
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppMigrationTest,
                       PRE_UserInstalledDoesntMigrate) {
  InstallDefaultApps(/*install_old_app=*/true);

  EXPECT_TRUE(registrar().IsInRegistrar(old_app_id_));
  EXPECT_TRUE(registrar().IsInstallState(
      old_app_id_, {ExpectedPreinstalledAppInstallState()}));

  // User install the same app.
  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(old_app_url_);
  EXPECT_EQ(old_app_id_, web_app::test::InstallWebApp(browser()->profile(),
                                                      std::move(web_app_info)));

  ASSERT_TRUE(registrar().IsInstallState(
      old_app_id_,
      {web_app::proto::InstallState::INSTALLED_WITH_OS_INTEGRATION}));
  EXPECT_TRUE(registrar().GetAppById(old_app_id_)->IsPreinstalledApp());
  EXPECT_FALSE(registrar().IsInRegistrar(new_app_id_));
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppMigrationTest,
                       UserInstalledDoesntMigrate) {
  InstallDefaultApps(/*install_old_app=*/false);

  ASSERT_TRUE(registrar().IsInstallState(
      old_app_id_,
      {web_app::proto::InstallState::INSTALLED_WITH_OS_INTEGRATION}));
  EXPECT_TRUE(registrar().GetAppById(old_app_id_)->IsPreinstalledApp());
  EXPECT_FALSE(registrar().IsInRegistrar(new_app_id_));
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppMigrationTest,
                       PRE_OpenStandaloneDoesntMigrate) {
  InstallDefaultApps(/*install_old_app=*/true);

  EXPECT_TRUE(registrar().IsInRegistrar(old_app_id_));
  EXPECT_TRUE(registrar().IsInstallState(
      old_app_id_, {ExpectedPreinstalledAppInstallState()}));

  // Change the web app's user display mode to kStandalone
  base::test::TestFuture<void> future;
  provider().scheduler().SetUserDisplayMode(
      old_app_id_, mojom::UserDisplayMode::kStandalone, future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_TRUE(registrar().IsInstallState(
      old_app_id_,
      {web_app::proto::InstallState::INSTALLED_WITH_OS_INTEGRATION}));
  EXPECT_TRUE(registrar().GetAppById(old_app_id_)->IsPreinstalledApp());
  EXPECT_FALSE(registrar().IsInRegistrar(new_app_id_));
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppMigrationTest,
                       OpenStandaloneDoesntMigrate) {
  InstallDefaultApps(/*install_old_app=*/false);

  ASSERT_TRUE(registrar().IsInstallState(
      old_app_id_,
      {web_app::proto::InstallState::INSTALLED_WITH_OS_INTEGRATION}));
  EXPECT_TRUE(registrar().GetAppById(old_app_id_)->IsPreinstalledApp());
  EXPECT_FALSE(registrar().IsInRegistrar(new_app_id_));
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppMigrationTest,
                       PRE_PreinstallAsStandaloneMigrates) {
  // On ChromeOS the default app is pre-installed as standalone. Make sure
  // migration works for that case as well.
  InstallDefaultApps(/*install_old_app=*/true, /*is_standalone=*/true);

  EXPECT_TRUE(registrar().IsInRegistrar(old_app_id_));
  EXPECT_TRUE(registrar().IsInstallState(
      old_app_id_, {ExpectedPreinstalledAppInstallState()}));
  EXPECT_FALSE(registrar().IsInRegistrar(new_app_id_));
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppMigrationTest,
                       PreinstallAsStandaloneMigrates) {
  InstallDefaultApps(/*install_old_app=*/false, /*is_standalone=*/true);

  EXPECT_FALSE(registrar().IsInRegistrar(old_app_id_));
  ASSERT_TRUE(registrar().IsInstallState(
      new_app_id_, {ExpectedPreinstalledAppInstallState()}));
  EXPECT_TRUE(registrar().GetAppById(new_app_id_)->IsPreinstalledApp());
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppMigrationTest,
                       PRE_PRE_NoMigrationIfUninstalled) {
  InstallDefaultApps(/*install_old_app=*/true);

  ASSERT_TRUE(registrar().IsInstallState(
      old_app_id_, {ExpectedPreinstalledAppInstallState()}));
  EXPECT_TRUE(registrar().GetAppById(old_app_id_)->IsPreinstalledApp());
  EXPECT_FALSE(registrar().IsInRegistrar(new_app_id_));

  // Have the user uninstall the pre-installed app.
  base::test::TestFuture<webapps::UninstallResultCode> uninstall_result;
  provider().scheduler().RemoveUserUninstallableManagements(
      old_app_id_, webapps::WebappUninstallSource::kAppsPage,
      uninstall_result.GetCallback());
  EXPECT_EQ(webapps::UninstallResultCode::kAppRemoved, uninstall_result.Get());
  EXPECT_FALSE(registrar().IsInRegistrar(old_app_id_));
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppMigrationTest,
                       PRE_NoMigrationIfUninstalled) {
  InstallDefaultApps(/*install_old_app=*/false);

  EXPECT_FALSE(registrar().IsInRegistrar(old_app_id_));
  EXPECT_FALSE(registrar().IsInRegistrar(new_app_id_));
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppMigrationTest,
                       NoMigrationIfUninstalled) {
  InstallDefaultApps(/*install_old_app=*/false);

  EXPECT_FALSE(registrar().IsInRegistrar(old_app_id_));
  EXPECT_FALSE(registrar().IsInRegistrar(new_app_id_));
}

// This next test is mostly a sanity check to make sure things aren't entirely
// broken if we were to roll back the migration.
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppMigrationTest, PRE_ReverseMigration) {
  InstallDefaultApps(/*install_old_app=*/false);

  EXPECT_TRUE(registrar().IsInRegistrar(new_app_id_));
  ASSERT_TRUE(registrar().IsInstallState(
      new_app_id_, {ExpectedPreinstalledAppInstallState()}));
  EXPECT_TRUE(registrar().GetAppById(new_app_id_)->IsPreinstalledApp());
  EXPECT_FALSE(registrar().IsInRegistrar(old_app_id_));
}

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppMigrationTest, ReverseMigration) {
  InstallDefaultApps(/*install_old_app=*/true);

  EXPECT_FALSE(registrar().IsInRegistrar(new_app_id_));
  ASSERT_TRUE(registrar().IsInstallState(
      old_app_id_, {ExpectedPreinstalledAppInstallState()}));
  EXPECT_TRUE(registrar().GetAppById(old_app_id_)->IsPreinstalledApp());
}

}  // namespace web_app
