// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/dedupe_install_urls_command.h"

#include "base/test/bind.h"
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
#include "chrome/common/pref_names.h"

namespace web_app {

class DedupeInstallUrlsCommandTest : public WebAppTest {
 public:
  DedupeInstallUrlsCommandTest()
      : bypass_dependencies_(
            PreinstalledWebAppManager::BypassAwaitingDependenciesForTesting()) {
  }

  void SetUp() override {
    WebAppTest::SetUp();

    PreinstalledWebAppManager::SkipStartupForTesting();
    PreinstalledWebAppManager::BypassOfflineManifestRequirementForTesting();

    fake_web_contents_manager_ = static_cast<FakeWebContentsManager*>(
        &provider().web_contents_manager());

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  WebAppProvider& provider() {
    return *WebAppProvider::GetForWebApps(profile());
  }

  void TearDown() override {
    provider().Shutdown();
    WebAppTest::TearDown();
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

  void AddBuggyDefaultInstallToApp(const AppId& app_id,
                                   const GURL& install_url) {
    ScopedRegistryUpdate update(&provider().sync_bridge_unsafe());
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
    options.bypass_service_worker_check = true;
    scope.apps.push_back(std::move(options));

    base::test::TestFuture<std::map<GURL /*install_url*/,
                                    ExternallyManagedAppManager::InstallResult>,
                           std::map<GURL /*install_url*/, bool /*succeeded*/>>
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

 protected:
  raw_ptr<FakeWebContentsManager, DisableDanglingPtrDetection>
      fake_web_contents_manager_;
  base::AutoReset<bool> bypass_dependencies_;
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
  AppId placeholder_app_id = GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(install_url));
  AppId real_app_id = GenerateAppIdFromManifestId(
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
              base::BucketsAre(base::Bucket(1, 1), base::Bucket(2, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "WebApp.DedupeInstallUrls.InstallUrlsDeduped"),
              base::BucketsAre(base::Bucket(0, 1), base::Bucket(1, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("WebApp.DedupeInstallUrls.AppsDeduped"),
      base::BucketsAre(base::Bucket(2, 1)));
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
  AppId placeholder_app_id = GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(install_url));
  AppId real_app_id = GenerateAppIdFromManifestId(
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
              base::BucketsAre(base::Bucket(1, 1), base::Bucket(2, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "WebApp.DedupeInstallUrls.InstallUrlsDeduped"),
              base::BucketsAre(base::Bucket(0, 1), base::Bucket(1, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("WebApp.DedupeInstallUrls.AppsDeduped"),
      base::BucketsAre(base::Bucket(2, 1)));
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
  AppId placeholder_app_id = GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(install_url));
  AppId real_app_id = GenerateAppIdFromManifestId(
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
              base::BucketsAre(base::Bucket(1, 1), base::Bucket(2, 1),
                               base::Bucket(3, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "WebApp.DedupeInstallUrls.InstallUrlsDeduped"),
              base::BucketsAre(base::Bucket(0, 1), base::Bucket(1, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("WebApp.DedupeInstallUrls.AppsDeduped"),
      base::BucketsAre(base::Bucket(2, 1)));
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
  AppId placeholder_app_id = GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(install_url));
  AppId real_app_id = GenerateAppIdFromManifestId(
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
              base::BucketsAre(base::Bucket(1, 1), base::Bucket(2, 1),
                               base::Bucket(3, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "WebApp.DedupeInstallUrls.InstallUrlsDeduped"),
              base::BucketsAre(base::Bucket(0, 2), base::Bucket(1, 1)));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("WebApp.DedupeInstallUrls.AppsDeduped"),
      base::BucketsAre(base::Bucket(2, 1)));
}

}  // namespace web_app
