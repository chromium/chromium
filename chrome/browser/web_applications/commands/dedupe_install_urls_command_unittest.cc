// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/dedupe_install_urls_command.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/pref_names.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#endif

namespace web_app {

class DedupeInstallUrlsCommandTest : public WebAppTest {
 public:
  DedupeInstallUrlsCommandTest()
      : bypass_dependencies_(
            PreinstalledWebAppManager::BypassAwaitingDependenciesForTesting()),
        skip_preinstalled_web_app_startup_(
            PreinstalledWebAppManager::SkipStartupForTesting()),
        bypass_offline_manifest_requirement_(
            PreinstalledWebAppManager::
                BypassOfflineManifestRequirementForTesting()) {}

  void SetUp() override {
    WebAppTest::SetUp();

    fake_web_contents_manager_ = static_cast<FakeWebContentsManager*>(
        &provider().web_contents_manager());

    test::AwaitStartWebAppProviderAndSubsystems(profile());

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Mocking the StatisticsProvider for testing.
    ash::system::StatisticsProvider::SetTestProvider(&statistics_provider);
    statistics_provider.SetMachineStatistic(ash::system::kActivateDateKey,
                                            "2023-18");
#endif
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::system::StatisticsProvider::SetTestProvider(nullptr);
#endif
    WebAppTest::TearDown();
  }

  WebAppProvider& provider() {
    return *WebAppProvider::GetForWebApps(profile());
  }

  void SetPolicyInstallUrlAndSynchronize(const GURL& url) {
    base::test::TestFuture<void> future;
    provider()
        .policy_manager()
        .SetOnAppsSynchronizedCompletedCallbackForTesting(future.GetCallback());
    profile()->GetPrefs()->SetList(
        prefs::kWebAppInstallForceList,
        base::Value::List().Append(base::Value::Dict().Set("url", url.spec())));
    CHECK(future.Wait());
  }

  void AddBuggyDefaultInstallToApp(const webapps::AppId& app_id,
                                   const GURL& install_url) {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    WebApp& placeholder_app = *update->UpdateApp(app_id);
    placeholder_app.AddSource(WebAppManagement::Type::kDefault);
    placeholder_app.AddInstallURLToManagementExternalConfigMap(
        WebAppManagement::Type::kDefault, install_url);
  }

  void SynchronizePreinstalledWebAppManagerWithInstallUrl(
      const GURL& install_url) {
    ScopedTestingPreinstalledAppData scope;
    ExternalInstallOptions options(install_url,
                                   mojom::UserDisplayMode::kStandalone,
                                   ExternalInstallSource::kExternalDefault);
    options.user_type_allowlist = {"unmanaged"};
    scope.apps.push_back(std::move(options));

    base::test::TestFuture<
        std::map<GURL /*install_url*/,
                 ExternallyManagedAppManager::InstallResult>,
        std::map<GURL /*install_url*/, webapps::UninstallResultCode>>
        future;
    provider().preinstalled_web_app_manager().LoadAndSynchronizeForTesting(
        future.GetCallback());
    CHECK(future.Wait());
  }

  void SynchronizePolicyWebAppManager() {
    base::test::TestFuture<void> future;
    provider()
        .policy_manager()
        .SetOnAppsSynchronizedCompletedCallbackForTesting(future.GetCallback());
    provider().policy_manager().RefreshPolicyInstalledAppsForTesting();
    CHECK(future.Wait());
  }

  webapps::AppId ExternallyInstallWebApp(
      webapps::WebappInstallSource install_surface,
      const GURL& install_url,
      const GURL& start_url) {
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->title = u"Test app";
    web_app_info->install_url = install_url;

    return test::InstallWebApp(profile(), std::move(web_app_info),
                               /*overwrite_existing_manifest_fields=*/true,
                               install_surface);
  }

 protected:
  raw_ptr<FakeWebContentsManager, DisableDanglingPtrDetection>
      fake_web_contents_manager_ = nullptr;
  base::AutoReset<bool> bypass_dependencies_;
  base::AutoReset<bool> skip_preinstalled_web_app_startup_;
  base::AutoReset<bool> bypass_offline_manifest_requirement_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::system::FakeStatisticsProvider statistics_provider;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

TEST_F(DedupeInstallUrlsCommandTest,
       PolicyUpgradePlaceholderWithTwoInstallSources) {
  // This tests for users affected by crbug.com/1427340, specifically those left
  // with a placeholder web app installed with kPolicy and kDefault install
  // sources.
  // They got into this state by the following steps:
  // - A web app policy installed install URL A, was unsuccessful and created
  //   placeholder P for install URL A.
  // - A web app preinstall installed install URL A, saw the placeholder P and
  //   added itself as another install source to it.
  // This test checks that the placeholder is removed with a successful policy
  // install of install URL A.

  base::HistogramTester histogram_tester;
  GURL install_url("https://example.com/install_url");
  GURL manifest_url("https://example.com/manifest.json");
  GURL start_url("https://example.com/start_url");
  webapps::AppId placeholder_app_id = GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(install_url));
  webapps::AppId real_app_id = GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(start_url));

  // Set up buggy state.
  {
    // Set up failure at install_url.
    // Default FakePageState is kFailedErrorPageLoaded.
    fake_web_contents_manager_->GetOrCreatePageState(install_url);

    // Install install_url via policy leading to placeholder installation.
    SetPolicyInstallUrlAndSynchronize(install_url);
    EXPECT_TRUE(provider().registrar_unsafe().IsPlaceholderApp(
        placeholder_app_id, WebAppManagement::Type::kPolicy));

    // Recreate preinstall bug by adding its config to the same placeholder.
    AddBuggyDefaultInstallToApp(placeholder_app_id, install_url);
  }

  // Perform successful policy install.
  {
    // Set up successful load at install_url.
    ASSERT_EQ(fake_web_contents_manager_->CreateBasicInstallPageState(
                  install_url, manifest_url, start_url),
              real_app_id);

    // Rerun existing policy install_url installation.
    SynchronizePolicyWebAppManager();
  }

  // Placeholder app should no longer be present.
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(placeholder_app_id));

  // Real app should be installed
  const WebApp* real_app =
      provider().registrar_unsafe().GetAppById(real_app_id);
  EXPECT_TRUE(real_app);

  // Real app should contain both the policy and preinstall install_url
  // association configs.
  EXPECT_EQ(
      real_app->management_to_external_config_map(),
      (WebApp::ExternalConfigMap{
          {WebAppManagement::Type::kPolicy, {false, {install_url}, {}}},
          {WebAppManagement::Type::kDefault, {false, {install_url}, {}}}}));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "WebApp.DedupeInstallUrls.SessionRunCount"),
              base::BucketsAre(base::Bucket(/*runs=*/1, /*count=*/1),
                               base::Bucket(/*runs=*/2, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "WebApp.DedupeInstallUrls.InstallUrlsDeduped"),
              base::BucketsAre(base::Bucket(/*install_urls=*/0, /*count=*/1),
                               base::Bucket(/*install_urls=*/1, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("WebApp.DedupeInstallUrls.AppsDeduped"),
      base::BucketsAre(base::Bucket(/*apps=*/2, /*count=*/1)));
}

TEST_F(DedupeInstallUrlsCommandTest,
       PreinstallUpgradePlaceholderWithTwoInstallSources) {
  // This tests for users affected by crbug.com/1427340, specifically those left
  // with a placeholder web app installed with kPolicy and kDefault install
  // sources.
  // They got into this state by the following steps:
  // - A web app policy installed install URL A, was unsuccessful and created
  //   placeholder P for install URL A.
  // - A web app preinstall installed install URL A, saw the placeholder P and
  //   added itself as another install source to it.
  // This test checks that the placeholder is removed with a force reinstall of
  // install URL A via PreinstalledWebAppManager.

  base::HistogramTester histogram_tester;
  GURL install_url("https://example.com/install_url");
  GURL manifest_url("https://example.com/manifest.json");
  GURL start_url("https://example.com/start_url");
  webapps::AppId placeholder_app_id = GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(install_url));
  webapps::AppId real_app_id = GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(start_url));

  // Set up buggy state.
  {
    // Set up failure at install_url.
    // Default FakePageState is kFailedErrorPageLoaded.
    fake_web_contents_manager_->GetOrCreatePageState(install_url);

    // Install install_url via policy leading to placeholder installation.
    SetPolicyInstallUrlAndSynchronize(install_url);
    EXPECT_TRUE(provider().registrar_unsafe().IsPlaceholderApp(
        placeholder_app_id, WebAppManagement::Type::kPolicy));

    // Recreate preinstall bug by adding its config to the same placeholder.
    AddBuggyDefaultInstallToApp(placeholder_app_id, install_url);
  }

  // Run PreinstalledWebAppManager with a working install_url.
  ASSERT_EQ(fake_web_contents_manager_->CreateBasicInstallPageState(
                install_url, manifest_url, start_url),
            real_app_id);
  SynchronizePreinstalledWebAppManagerWithInstallUrl(install_url);

  // Placeholder app should no longer be present.
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(placeholder_app_id));

  // Real app should be installed
  const WebApp* real_app =
      provider().registrar_unsafe().GetAppById(real_app_id);
  EXPECT_TRUE(real_app);

  // Real app should contain both the policy and preinstall install_url
  // association configs.
  EXPECT_EQ(
      real_app->management_to_external_config_map(),
      (WebApp::ExternalConfigMap{
          {WebAppManagement::Type::kPolicy, {false, {install_url}, {}}},
          {WebAppManagement::Type::kDefault, {false, {install_url}, {}}}}));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "WebApp.DedupeInstallUrls.SessionRunCount"),
              base::BucketsAre(base::Bucket(/*runs=*/1, /*count=*/1),
                               base::Bucket(/*runs=*/2, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "WebApp.DedupeInstallUrls.InstallUrlsDeduped"),
              base::BucketsAre(base::Bucket(/*install_urls=*/0, /*count=*/1),
                               base::Bucket(/*install_urls=*/1, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("WebApp.DedupeInstallUrls.AppsDeduped"),
      base::BucketsAre(base::Bucket(/*apps=*/2, /*count=*/1)));
}

TEST_F(DedupeInstallUrlsCommandTest, SameInstallUrlForRealAndPlaceholder) {
  // This tests for users affected by crbug.com/1427340, specifically those left
  // with a kDefault placeholder-like app (placeholder in appearance but not in
  // configuration) and kPolicy real app.
  // They got into this state by the following steps:
  // - A web app policy installed install URL A, was unsuccessful and created
  //   placeholder P for install URL A.
  // - A web app preinstall installed install URL A, saw the placeholder P and
  //   added itself as another install source to it.
  // - The web app policy later retried installing install URL A and was
  //   successful, installed real app R and removed the policy install source
  //   from placeholder P. This did not uninstall placeholder P as the default
  //   install source remained.
  // - The user is left with placeholder P and real app R both associated with
  //   install URL A.
  // This test checks that the placeholder is deduped into the real app after a
  // policy synchronisation run that's already satisfied.

  base::HistogramTester histogram_tester;
  GURL install_url("https://example.com/install_url");
  GURL manifest_url("https://example.com/manifest.json");
  GURL start_url("https://example.com/start_url");
  webapps::AppId placeholder_app_id = GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(install_url));
  webapps::AppId real_app_id = GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(start_url));

  // Set up buggy state.
  {
    // Set up failure at install_url.
    // Default FakePageState is kFailedErrorPageLoaded.
    fake_web_contents_manager_->GetOrCreatePageState(install_url);

    // Install install_url via policy leading to placeholder installation.
    SetPolicyInstallUrlAndSynchronize(install_url);
    EXPECT_TRUE(provider().registrar_unsafe().IsPlaceholderApp(
        placeholder_app_id, WebAppManagement::Type::kPolicy));

    // Recreate preinstall bug by adding its config to the same placeholder.
    AddBuggyDefaultInstallToApp(placeholder_app_id, install_url);

    // Run policy install successfully without any dedupe logic.
    {
      base::AutoReset<bool> scope =
          DedupeInstallUrlsCommand::ScopedSuppressForTesting();

      // Set up successful load at install_url.
      ASSERT_EQ(fake_web_contents_manager_->CreateBasicInstallPageState(
                    install_url, manifest_url, start_url),
                real_app_id);

      SynchronizePolicyWebAppManager();

      // The placeholder app remains.
      const WebApp* placeholder_app =
          provider().registrar_unsafe().GetAppById(placeholder_app_id);
      ASSERT_TRUE(placeholder_app);
      EXPECT_EQ(placeholder_app->management_to_external_config_map(),
                (WebApp::ExternalConfigMap{{WebAppManagement::Type::kDefault,
                                            {false, {install_url}, {}}}}));

      // The placeholder is no longer marked as a placeholder despite still
      // looking like one.
      EXPECT_FALSE(provider().registrar_unsafe().IsPlaceholderApp(
          placeholder_app_id, WebAppManagement::Type::kPolicy));
      EXPECT_TRUE(LooksLikePlaceholder(*placeholder_app));
    }
  }

  // Rerun policy synchronize with deduping enabled.
  SynchronizePolicyWebAppManager();

  // Placeholder app should no longer be present.
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(placeholder_app_id));

  // Real app should be installed
  const WebApp* real_app =
      provider().registrar_unsafe().GetAppById(real_app_id);
  EXPECT_TRUE(real_app);

  // Real app should contain both the policy and preinstall install_url
  // association configs.
  EXPECT_EQ(
      real_app->management_to_external_config_map(),
      (WebApp::ExternalConfigMap{
          {WebAppManagement::Type::kPolicy, {false, {install_url}, {}}},
          {WebAppManagement::Type::kDefault, {false, {install_url}, {}}}}));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "WebApp.DedupeInstallUrls.SessionRunCount"),
              base::BucketsAre(base::Bucket(/*runs=*/1, /*count=*/1),
                               base::Bucket(/*runs=*/2, /*count=*/1),
                               base::Bucket(/*runs=*/3, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "WebApp.DedupeInstallUrls.InstallUrlsDeduped"),
              base::BucketsAre(base::Bucket(/*install_urls=*/0, /*count=*/1),
                               base::Bucket(/*install_urls=*/1, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("WebApp.DedupeInstallUrls.AppsDeduped"),
      base::BucketsAre(base::Bucket(/*apps=*/2, /*count=*/1)));
}

TEST_F(DedupeInstallUrlsCommandTest, DefaultPlaceholderForceReinstalled) {
  // This tests for users affected by crbug.com/1427340, specifically those left
  // with a kDefault placeholder-like app (placeholder in appearance but not in
  // configuration) and kPolicy real app for a different install URL.
  // They got into this state by the following steps:
  // - A web app policy installed install URL A, was unsuccessful and created
  //   placeholder P for install URL A.
  // - A web app preinstall installed install URL A, saw the placeholder P and
  //   added itself as another install source to it.
  // - The web app policy later retried installing install URL A and was
  //   successful, installed real app R and removed the policy install source
  //   from placeholder P. This did not uninstall placeholder P as the default
  //   install source remained.
  // - The web app policy is later later updated to install a similar install
  //   URL B with a slight tweak to the query params so it still installs the
  //   same app. Real app R is uninstalled then reinstalled again this time
  //   associated with install URL B.
  // - The user is left with placeholder P associated with install URL A via
  //   the kDefault install source and real app R associated with install URL B
  //   via the kPolicy install source.
  // This test checks that the placeholder is removed with a force reinstall of
  // install URL A via PreinstalledWebAppManager.

  base::HistogramTester histogram_tester;
  GURL install_url("https://example.com/install_url");
  GURL alternate_install_url(
      "https://example.com/install_url?with_query_param");
  GURL manifest_url("https://example.com/manifest.json");
  GURL start_url("https://example.com/start_url");
  webapps::AppId placeholder_app_id = GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(install_url));
  webapps::AppId real_app_id = GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(start_url));

  // Set up buggy state.
  {
    // Set up failure at install_url.
    // Default FakePageState is kFailedErrorPageLoaded.
    fake_web_contents_manager_->GetOrCreatePageState(install_url);

    // Install install_url via policy leading to placeholder installation.
    SetPolicyInstallUrlAndSynchronize(install_url);
    EXPECT_TRUE(provider().registrar_unsafe().IsPlaceholderApp(
        placeholder_app_id, WebAppManagement::Type::kPolicy));

    // Recreate preinstall bug by adding its config to the same placeholder.
    AddBuggyDefaultInstallToApp(placeholder_app_id, install_url);

    // Run policy install successfully on a slightly different install_url that
    // installs the same web app.
    ASSERT_EQ(fake_web_contents_manager_->CreateBasicInstallPageState(
                  alternate_install_url, manifest_url, start_url),
              real_app_id);
    SetPolicyInstallUrlAndSynchronize(alternate_install_url);

    // The placeholder app remains.
    const WebApp* placeholder_app =
        provider().registrar_unsafe().GetAppById(placeholder_app_id);
    ASSERT_TRUE(placeholder_app);
    EXPECT_EQ(placeholder_app->management_to_external_config_map(),
              (WebApp::ExternalConfigMap{{WebAppManagement::Type::kDefault,
                                          {false, {install_url}, {}}}}));

    // The placeholder is no longer marked as a placeholder despite still
    // looking like one.
    EXPECT_FALSE(provider().registrar_unsafe().IsPlaceholderApp(
        placeholder_app_id, WebAppManagement::Type::kPolicy));
    EXPECT_TRUE(LooksLikePlaceholder(*placeholder_app));
  }

  // Run PreinstalledWebAppManager with a working install_url.
  ASSERT_EQ(fake_web_contents_manager_->CreateBasicInstallPageState(
                install_url, manifest_url, start_url),
            real_app_id);
  SynchronizePreinstalledWebAppManagerWithInstallUrl(install_url);

  // Placeholder app should no longer be present.
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(placeholder_app_id));

  // Real app should be installed
  const WebApp* real_app =
      provider().registrar_unsafe().GetAppById(real_app_id);
  EXPECT_TRUE(real_app);

  // Real app should contain both the policy and preinstall configs for their
  // respective install URLs.
  EXPECT_EQ(real_app->management_to_external_config_map(),
            (WebApp::ExternalConfigMap{{WebAppManagement::Type::kPolicy,
                                        {false, {alternate_install_url}, {}}},
                                       {WebAppManagement::Type::kDefault,
                                        {false, {install_url}, {}}}}));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "WebApp.DedupeInstallUrls.SessionRunCount"),
              base::BucketsAre(base::Bucket(/*runs=*/1, /*count=*/1),
                               base::Bucket(/*runs=*/2, /*count=*/1),
                               base::Bucket(/*runs=*/3, /*count=*/1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "WebApp.DedupeInstallUrls.InstallUrlsDeduped"),
              base::BucketsAre(base::Bucket(/*install_urls=*/0, /*count=*/2),
                               base::Bucket(/*install_urls=*/1, /*count=*/1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("WebApp.DedupeInstallUrls.AppsDeduped"),
      base::BucketsAre(base::Bucket(/*apps=*/2, /*count=*/1)));
}

// Other tests just cover two apps getting deduped, this tests multiple
// install_urls getting deduped with more than two apps in each.
TEST_F(DedupeInstallUrlsCommandTest, MoreThanTwoDuplicates) {
  base::HistogramTester histogram_tester;

  // Set up duplicate apps.
  GURL install_url_a("https://www.a.com/");
  webapps::AppId app_id_a1 =
      ExternallyInstallWebApp(webapps::WebappInstallSource::EXTERNAL_DEFAULT,
                              install_url_a, install_url_a.Resolve("default"));
  webapps::AppId app_id_a2 =
      ExternallyInstallWebApp(webapps::WebappInstallSource::KIOSK,
                              install_url_a, install_url_a.Resolve("kiosk"));
  webapps::AppId app_id_a3 =
      ExternallyInstallWebApp(webapps::WebappInstallSource::EXTERNAL_POLICY,
                              install_url_a, install_url_a.Resolve("policy"));

  GURL install_url_b("https://www.b.com/");
  webapps::AppId app_id_b1 =
      ExternallyInstallWebApp(webapps::WebappInstallSource::ARC, install_url_b,
                              install_url_b.Resolve("arc"));
  webapps::AppId app_id_b2 =
      ExternallyInstallWebApp(webapps::WebappInstallSource::PRELOADED_OEM,
                              install_url_b, install_url_b.Resolve("oem"));
  webapps::AppId app_id_b3 =
      ExternallyInstallWebApp(webapps::WebappInstallSource::MICROSOFT_365_SETUP,
                              install_url_b, install_url_b.Resolve("ms365"));

  // All app IDs must be unique.
  ASSERT_EQ(base::flat_set<webapps::AppId>({app_id_a1, app_id_a2, app_id_a3,
                                            app_id_b1, app_id_b2, app_id_b3})
                .size(),
            6u);

  // Run dedupe scan.
  base::test::TestFuture<void> future;
  provider().scheduler().ScheduleDedupeInstallUrls(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // The most recently installed web app is chosen as the dedupe into target.
  const WebAppRegistrar& registrar = provider().registrar_unsafe();
  EXPECT_FALSE(registrar.IsInstalled(app_id_a1));
  EXPECT_FALSE(registrar.IsInstalled(app_id_a2));
  const WebApp* app_a = registrar.GetAppById(app_id_a3);
  ASSERT_TRUE(app_a);
  EXPECT_EQ(app_a->GetSources(),
            WebAppManagementTypes({WebAppManagement::Type::kDefault,
                                   WebAppManagement::Type::kKiosk,
                                   WebAppManagement::Type::kPolicy}));

  EXPECT_FALSE(registrar.IsInstalled(app_id_b1));
  EXPECT_FALSE(registrar.IsInstalled(app_id_b2));
  const WebApp* app_b = registrar.GetAppById(app_id_b3);
  ASSERT_TRUE(app_b);
  EXPECT_EQ(
      app_b->GetSources(),
      WebAppManagementTypes({WebAppManagement::Type::kWebAppStore,
                             WebAppManagement::Type::kOem,
                             WebAppManagement::Type::kOneDriveIntegration}));

  histogram_tester.ExpectUniqueSample(
      "WebApp.DedupeInstallUrls.SessionRunCount", /*runs=*/1, /*count=*/1);
  histogram_tester.ExpectUniqueSample(
      "WebApp.DedupeInstallUrls.InstallUrlsDeduped", /*install_urls=*/2,
      /*count=*/1);
  histogram_tester.ExpectUniqueSample("WebApp.DedupeInstallUrls.AppsDeduped",
                                      /*apps=*/3,
                                      /*count=*/2);
}

}  // namespace web_app
