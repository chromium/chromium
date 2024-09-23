// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_app_icon_downloader.h"
#include "chrome/test/base/profile_destruction_waiter.h"
#include "components/user_manager/user_manager.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#endif

namespace web_app {

class ProfileMarkedForDeletionObserver : public ProfileManagerObserver {
 public:
  explicit ProfileMarkedForDeletionObserver(Profile* profile)
      : profile_(profile) {
    if (ProfileManager* profile_manager =
            g_browser_process->profile_manager()) {
      profile_manager_observation_.Observe(profile_manager);
    }
  }

  void Wait() { run_loop_.Run(); }

  void OnProfileMarkedForPermanentDeletion(
      Profile* profile_to_be_deleted) override {
    if (profile_to_be_deleted != profile_) {
      return;
    }

    run_loop_.Quit();
  }

  void OnProfileManagerDestroying() override {
    profile_manager_observation_.Reset();
  }

 private:
  raw_ptr<Profile> profile_ = nullptr;
  base::RunLoop run_loop_;
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
};

class WebAppProfileDeletionBrowserTest : public WebAppBrowserTestBase {
 public:
  WebAppProfileDeletionBrowserTest()
      : skip_preinstalled_(PreinstalledWebAppManager::SkipStartupForTesting()) {
  }
  WebAppRegistrar& registrar() {
    auto* provider = WebAppProvider::GetForTest(profile());
    CHECK(provider);
    return provider->registrar_unsafe();
  }

  void ScheduleCurrentProfileForDeletion() {
    g_browser_process->profile_manager()
        ->GetDeleteProfileHelper()
        .MaybeScheduleProfileForDeletion(
            profile()->GetPath(), base::DoNothing(),
            ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  }

  webapps::AppId InstallAppToProfile(Profile* profile, const GURL& start_url) {
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    web_app_info->title = u"A Web App";
    return web_app::test::InstallWebApp(profile, std::move(web_app_info));
  }

  std::unique_ptr<content::WebContents>
  CreateWebContentsScheduledForDeletion() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath profile_path_to_delete =
        profile_manager->GenerateNextProfileDirectoryPath();
    Profile& profile_to_delete = profiles::testing::CreateProfileSync(
        profile_manager, profile_path_to_delete);

    std::unique_ptr<content::WebContents> deleting_web_contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(&profile_to_delete));
    EXPECT_NE(deleting_web_contents, nullptr);

    ProfileDestructionWaiter destruction_waiter(&profile_to_delete);
    profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
        profile_to_delete.GetPath(), base::DoNothing(),
        ProfileMetrics::DELETE_PROFILE_SETTINGS);
    destruction_waiter.Wait();

    return deleting_web_contents;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void CreateSession(const AccountId& account_id) {
    auto* session_manager = session_manager::SessionManager::Get();
    session_manager->CreateSession(account_id, account_id.GetUserEmail(),
                                   false);
  }

  Profile& StartUserSession(const AccountId& account_id) {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    Profile& profile = profiles::testing::CreateProfileSync(
        profile_manager,
        ash::BrowserContextHelper::Get()->GetBrowserContextPathByUserIdHash(
            user_manager::UserManager::Get()
                ->FindUser(account_id)
                ->username_hash()));

    auto* session_manager = session_manager::SessionManager::Get();
    session_manager->NotifyUserProfileLoaded(account_id);
    session_manager->SessionStarted();
    return profile;
  }

 protected:
  // Use a real domain to avoid policy loading problems.
  const std::string kTestUserName = "test@gmail.com";
  const std::string kTestUserGaiaId = "9876543210";
  const AccountId test_account_id_ =
      AccountId::FromUserEmailGaiaId(kTestUserName, kTestUserGaiaId);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
 private:
  base::AutoReset<bool> skip_preinstalled_;
};

class NoOpWebAppPublisherDelegate : public WebAppPublisherHelper::Delegate {
  // WebAppPublisherHelper::Delegate:
  void PublishWebApps(std::vector<apps::AppPtr> apps) override {}
  void PublishWebApp(apps::AppPtr app) override {}
  void ModifyWebAppCapabilityAccess(
      const std::string& app_id,
      std::optional<bool> accessing_camera,
      std::optional<bool> accessing_microphone) override {}
};

IN_PROC_BROWSER_TEST_F(WebAppProfileDeletionBrowserTest, OsIntegrationRemoved) {
  const GURL app_url = GetInstallableAppURL();

  /// Create a new profile and install a web app.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CreateSession(test_account_id_);
  Profile& profile_to_delete = StartUserSession(test_account_id_);
#else
  base::FilePath profile_path_to_delete =
      profile_manager->GenerateNextProfileDirectoryPath();
  Profile& profile_to_delete = profiles::testing::CreateProfileSync(
      profile_manager, profile_path_to_delete);
#endif
  Browser::Create(Browser::CreateParams(&profile_to_delete, true));
  const webapps::AppId app_id =
      InstallAppToProfile(&profile_to_delete, app_url);
  auto* provider = WebAppProvider::GetForTest(&profile_to_delete);
  auto* web_app = provider->registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(web_app);

  base::test::TestFuture<const webapps::AppId&> app_id_future;
  provider->os_integration_manager().SetForceUnregisterCalledForTesting(
      app_id_future.GetRepeatingCallback());

  // Trigger profile deletion while uninstalling the app to simulate profile
  // pointer being invalidated.
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      profile_to_delete.GetPath(), base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);

  ASSERT_TRUE(app_id_future.Wait());
  EXPECT_EQ(app_id_future.Get(), app_id);
}

IN_PROC_BROWSER_TEST_F(WebAppProfileDeletionBrowserTest,
                       CommandsNotScheduledAfterProfileMarkedForDeletion) {
  // Create a new profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CreateSession(test_account_id_);
  Profile& profile_to_delete = StartUserSession(test_account_id_);
#else
  base::FilePath profile_path_to_delete =
      profile_manager->GenerateNextProfileDirectoryPath();
  Profile& profile_to_delete = profiles::testing::CreateProfileSync(
      profile_manager, profile_path_to_delete);
#endif

  WebAppCommandScheduler& command_scheduler =
      WebAppProvider::GetForTest(&profile_to_delete)->scheduler();
  ProfileMarkedForDeletionObserver deletion_observer(&profile_to_delete);

  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      profile_to_delete.GetPath(), base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  deletion_observer.Wait();

  // Scheduling a test command post profile deletion should automatically be
  // stopped and the callback run.
  base::test::TestFuture<bool> commands_not_scheduled_future;
  command_scheduler.ScheduleCallbackWithResult(
      "TestCommandPostProfileDeletion", web_app::NoopLockDescription(),
      base::BindOnce(
          [](web_app::NoopLock&, base::Value::Dict&) { return true; }),
      commands_not_scheduled_future.GetCallback(), /*arg_for_shutdown=*/false);

  ASSERT_TRUE(commands_not_scheduled_future.Wait());
  ASSERT_FALSE(commands_not_scheduled_future.Get());
}

using WebAppProfileDeletionBrowserTest_WebAppPublisher =
    WebAppProfileDeletionBrowserTest;
IN_PROC_BROWSER_TEST_F(WebAppProfileDeletionBrowserTest_WebAppPublisher,
                       UninstallWhileProfileIsBeingDeleted) {
  const GURL app_url = GetInstallableAppURL();

  /// Create a new profile and install a web app.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CreateSession(test_account_id_);
  Profile& profile_to_delete = StartUserSession(test_account_id_);
#else
  base::FilePath profile_path_to_delete =
      profile_manager->GenerateNextProfileDirectoryPath();
  Profile& profile_to_delete = profiles::testing::CreateProfileSync(
      profile_manager, profile_path_to_delete);
#endif
  Browser::Create(Browser::CreateParams(&profile_to_delete, true));
  const webapps::AppId app_id =
      InstallAppToProfile(&profile_to_delete, app_url);
  auto* provider = WebAppProvider::GetForTest(&profile_to_delete);
  auto* web_app = provider->registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(web_app);

  base::test::TestFuture<const webapps::AppId&> app_id_future;
  provider->os_integration_manager().SetForceUnregisterCalledForTesting(
      app_id_future.GetRepeatingCallback());

  // Uninstall the web app.
  NoOpWebAppPublisherDelegate no_op_delegate;
  auto web_app_publisher_helper = std::make_unique<WebAppPublisherHelper>(
      &profile_to_delete, provider, &no_op_delegate);
  web_app_publisher_helper->UninstallWebApp(
      web_app, apps::UninstallSource::kAppList, /*clear_site_data*/ true,
      /*report_abuse*/ false);

  // Trigger profile deletion while uninstalling the app to simulate profile
  // pointer being invalidated.
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      profile_to_delete.GetPath(), base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);

  ASSERT_TRUE(app_id_future.Wait());
  EXPECT_EQ(app_id_future.Get(), app_id);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// TODO(crbug.com/40283231): Figure out a way having this test be run on
// ChromeOS Ash, i.e. properly trigger a browser context shutdown.

using WebAppProfileDeletionTest_WebContentsGracefulShutdown =
    WebAppProfileDeletionBrowserTest;

IN_PROC_BROWSER_TEST_F(WebAppProfileDeletionTest_WebContentsGracefulShutdown,
                       UrlLoading) {
  ASSERT_TRUE(embedded_test_server()->Start());
  webapps::WebAppUrlLoader loader;

  std::unique_ptr<content::WebContents> deleting_web_contents =
      CreateWebContentsScheduledForDeletion();

  base::test::TestFuture<webapps::WebAppUrlLoaderResult> loader_result;
  loader.LoadUrl(embedded_test_server()->GetURL("/title1.html"),
                 deleting_web_contents.get(),
                 webapps::WebAppUrlLoader::UrlComparison::kExact,
                 loader_result.GetCallback());
  EXPECT_TRUE(loader_result.Wait());

  EXPECT_EQ(loader_result.Get<webapps::WebAppUrlLoaderResult>(),
            webapps::WebAppUrlLoaderResult::kFailedWebContentsDestroyed);
}

IN_PROC_BROWSER_TEST_F(WebAppProfileDeletionTest_WebContentsGracefulShutdown,
                       IconDownloading) {
  WebAppIconDownloader icon_downloader;

  std::unique_ptr<content::WebContents> deleting_web_contents =
      CreateWebContentsScheduledForDeletion();

  base::test::TestFuture<IconsDownloadedResult, IconsMap,
                         DownloadedIconsHttpResults>
      icon_download_future;
  IconUrlSizeSet icon_urls;
  icon_urls.insert(IconUrlWithSize::CreateForUnspecifiedSize(
      GURL("https://www.example.com/favicon.ico")));
  icon_downloader.Start(deleting_web_contents.get(), icon_urls,
                        icon_download_future.GetCallback(),
                        IconDownloaderOptions());
  EXPECT_TRUE(icon_download_future.Wait());

  EXPECT_EQ(icon_download_future.Get<IconsDownloadedResult>(),
            IconsDownloadedResult::kPrimaryPageChanged);
}

IN_PROC_BROWSER_TEST_F(WebAppProfileDeletionTest_WebContentsGracefulShutdown,
                       DataRetrieverWebAppInfoFetching) {
  WebAppDataRetriever data_retriever;

  std::unique_ptr<content::WebContents> deleting_web_contents =
      CreateWebContentsScheduledForDeletion();

  base::test::TestFuture<std::unique_ptr<WebAppInstallInfo>>
      install_info_fetcher;
  data_retriever.GetWebAppInstallInfo(deleting_web_contents.get(),
                                      install_info_fetcher.GetCallback());
  EXPECT_TRUE(install_info_fetcher.Wait());

  EXPECT_EQ(install_info_fetcher.Get<std::unique_ptr<WebAppInstallInfo>>(),
            nullptr);
}

IN_PROC_BROWSER_TEST_F(WebAppProfileDeletionTest_WebContentsGracefulShutdown,
                       DataRetrieverInstallabilityFetch) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebAppDataRetriever data_retriever;

  std::unique_ptr<content::WebContents> deleting_web_contents =
      CreateWebContentsScheduledForDeletion();

  base::test::TestFuture<blink::mojom::ManifestPtr, bool,
                         webapps::InstallableStatusCode>
      installability_future;
  data_retriever.CheckInstallabilityAndRetrieveManifest(
      deleting_web_contents.get(), installability_future.GetCallback());
  EXPECT_TRUE(installability_future.Wait());

  EXPECT_EQ(installability_future.Get<webapps::InstallableStatusCode>(),
            webapps::InstallableStatusCode::RENDERER_CANCELLED);
}

IN_PROC_BROWSER_TEST_F(WebAppProfileDeletionTest_WebContentsGracefulShutdown,
                       DataRetrieverIconFetch) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebAppDataRetriever data_retriever;

  std::unique_ptr<content::WebContents> deleting_web_contents =
      CreateWebContentsScheduledForDeletion();

  base::test::TestFuture<blink::mojom::ManifestPtr, bool,
                         webapps::InstallableStatusCode>
      installability_future;
  data_retriever.CheckInstallabilityAndRetrieveManifest(
      deleting_web_contents.get(), installability_future.GetCallback());
  EXPECT_TRUE(installability_future.Wait());

  base::test::TestFuture<IconsDownloadedResult, IconsMap,
                         DownloadedIconsHttpResults>
      icon_download_future;
  IconUrlSizeSet icon_urls;
  icon_urls.insert(IconUrlWithSize::CreateForUnspecifiedSize(
      GURL("https://www.example.com/favicon.ico")));
  data_retriever.GetIcons(deleting_web_contents.get(), icon_urls,
                          /*skip_page_favicons=*/false,
                          /*fail_all_if_any_fail=*/false,
                          icon_download_future.GetCallback());
  EXPECT_TRUE(icon_download_future.Wait());

  EXPECT_EQ(icon_download_future.Get<IconsDownloadedResult>(),
            IconsDownloadedResult::kPrimaryPageChanged);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace web_app
