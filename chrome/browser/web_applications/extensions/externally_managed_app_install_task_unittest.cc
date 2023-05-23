// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_install_task.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/fake_data_retriever.h"
#include "chrome/browser/web_applications/test/fake_install_finalizer.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace web_app {

namespace {

// Returns a factory that will return |data_retriever| the first time it gets
// called. It will DCHECK if called more than once.
ExternallyManagedAppInstallTask::DataRetrieverFactory GetFactoryForRetriever(
    std::unique_ptr<WebAppDataRetriever> data_retriever) {
  // Ideally we would return this lambda directly but passing a mutable lambda
  // to BindLambdaForTesting results in a OnceCallback which cannot be used as
  // a DataRetrieverFactory because DataRetrieverFactory is a
  // RepeatingCallback. For this reason, wrap the OnceCallback in a repeating
  // callback that DCHECKs if it gets called more than once.
  auto callback = base::BindLambdaForTesting(
      [data_retriever = std::move(data_retriever)]() mutable {
        return std::move(data_retriever);
      });

  return base::BindRepeating(
      [](base::OnceCallback<std::unique_ptr<WebAppDataRetriever>()> callback) {
        DCHECK(callback);
        return std::move(callback).Run();
      },
      base::Passed(std::move(callback)));
}

class TestExternallyManagedAppInstallFinalizer : public WebAppInstallFinalizer {
 public:
  explicit TestExternallyManagedAppInstallFinalizer(
      WebAppRegistrarMutable* registrar)
      : WebAppInstallFinalizer(nullptr), registrar_(registrar) {}
  TestExternallyManagedAppInstallFinalizer(
      const TestExternallyManagedAppInstallFinalizer&) = delete;
  TestExternallyManagedAppInstallFinalizer& operator=(
      const TestExternallyManagedAppInstallFinalizer&) = delete;
  ~TestExternallyManagedAppInstallFinalizer() override = default;

  // Returns what would be the AppId if an app is installed with |url|.
  AppId GetAppIdForUrl(const GURL& url) {
    return FakeInstallFinalizer::GetAppIdForUrl(url);
  }

  void RegisterApp(std::unique_ptr<WebApp> web_app) {
    AppId app_id = web_app->app_id();
    registrar_->registry().emplace(std::move(app_id), std::move(web_app));
  }

  void UnregisterApp(const AppId& app_id) {
    auto it = registrar_->registry().find(app_id);
    DCHECK(it != registrar_->registry().end());

    registrar_->registry().erase(it);
  }

  void SetNextFinalizeInstallResult(const GURL& url,
                                    webapps::InstallResultCode code) {
    DCHECK(!base::Contains(next_finalize_install_results_, url));

    AppId app_id;
    if (code == webapps::InstallResultCode::kSuccessNewInstall) {
      app_id = GetAppIdForUrl(url);
    }
    next_finalize_install_results_[url] = {app_id, code};
  }

  void SetNextUninstallExternalWebAppResult(const GURL& app_url,
                                            webapps::UninstallResultCode code) {
    DCHECK(!base::Contains(next_uninstall_external_web_app_results_, app_url));

    next_uninstall_external_web_app_results_[app_url] = {
        GetAppIdForUrl(app_url), code};
  }

  const std::vector<WebAppInstallInfo>& web_app_info_list() {
    return web_app_info_list_;
  }

  const std::vector<FinalizeOptions>& finalize_options_list() {
    return finalize_options_list_;
  }

  const std::vector<GURL>& uninstall_external_web_app_urls() const {
    return uninstall_external_web_app_urls_;
  }

  size_t num_reparent_tab_calls() const { return num_reparent_tab_calls_; }

  // WebAppInstallFinalizer
  void FinalizeInstall(const WebAppInstallInfo& web_app_info,
                       const FinalizeOptions& options,
                       InstallFinalizedCallback callback) override {
    DCHECK(
        base::Contains(next_finalize_install_results_, web_app_info.start_url));

    web_app_info_list_.push_back(web_app_info.Clone());
    finalize_options_list_.push_back(options);

    AppId app_id;
    webapps::InstallResultCode code;
    std::tie(app_id, code) =
        next_finalize_install_results_[web_app_info.start_url];
    next_finalize_install_results_.erase(web_app_info.start_url);
    const GURL& url = web_app_info.start_url;
    bool is_placeholder = web_app_info.is_placeholder;
    WebAppManagement::Type source = options.source;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([&, app_id, url, code, is_placeholder,
                                    source,
                                    callback = std::move(callback)]() mutable {
          auto web_app = test::CreateWebApp(url, WebAppManagement::kPolicy);
          // This has to be done because the test does not use the actual
          // ExternalAppManager, it mocks the install by writing to the
          // registry, even though kWriteDataFailed is explicitly set in the
          // test.
          if (code != webapps::InstallResultCode::kWriteDataFailed)
            web_app->AddExternalSourceInformation(source, url, is_placeholder);
          RegisterApp(std::move(web_app));
          std::move(callback).Run(app_id, code, OsHooksErrors());
        }));
  }

  void FinalizeUpdate(const WebAppInstallInfo& web_app_info,
                      InstallFinalizedCallback callback) override {
    NOTREACHED();
  }

  void UninstallExternalWebApp(const AppId& app_id,
                               WebAppManagement::Type external_source,
                               webapps::WebappUninstallSource uninstall_source,
                               UninstallWebAppCallback callback) override {
    UnregisterApp(app_id);

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  webapps::UninstallResultCode::kSuccess));
  }

  void UninstallExternalWebAppByUrl(
      const GURL& app_url,
      WebAppManagement::Type external_source,
      webapps::WebappUninstallSource uninstall_source,
      UninstallWebAppCallback callback) override {
    DCHECK(base::Contains(next_uninstall_external_web_app_results_, app_url));
    uninstall_external_web_app_urls_.push_back(app_url);

    AppId app_id;
    webapps::UninstallResultCode code;
    std::tie(app_id, code) = next_uninstall_external_web_app_results_[app_url];
    next_uninstall_external_web_app_results_.erase(app_url);

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting(
            [&, app_id, code, callback = std::move(callback)]() mutable {
              if (code == webapps::UninstallResultCode::kSuccess)
                UnregisterApp(app_id);
              std::move(callback).Run(code);
            }));
  }

  void UninstallWebApp(const AppId& app_id,
                       webapps::WebappUninstallSource uninstall_source,
                       UninstallWebAppCallback callback) override {
    NOTIMPLEMENTED();
  }

  bool WasPreinstalledWebAppUninstalled(const AppId& app_id) {
    NOTIMPLEMENTED();
    return false;
  }

  bool CanReparentTab(const AppId& app_id,
                      bool shortcut_created) const override {
    return true;
  }

  void ReparentTab(const AppId& app_id,
                   bool shortcut_created,
                   content::WebContents* web_contents) override {
    ++num_reparent_tab_calls_;
  }

  void SetRemoveManagementTypeCallbackForTesting(
      base::RepeatingCallback<void(const AppId&)>) override {
    NOTIMPLEMENTED();
  }

 private:
  raw_ptr<WebAppRegistrarMutable> registrar_ = nullptr;

  std::vector<WebAppInstallInfo> web_app_info_list_;
  std::vector<FinalizeOptions> finalize_options_list_;
  std::vector<GURL> uninstall_external_web_app_urls_;

  size_t num_reparent_tab_calls_ = 0;

  std::map<GURL, std::pair<AppId, webapps::InstallResultCode>>
      next_finalize_install_results_;

  // Maps app URLs to the id of the app that would have been installed for
  // that url and the result of trying to uninstall it.
  std::map<GURL, std::pair<AppId, webapps::UninstallResultCode>>
      next_uninstall_external_web_app_results_;
};

}  // namespace

class ExternallyManagedAppInstallTaskTest
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<test::ExternalPrefMigrationTestCases> {
 public:
  ExternallyManagedAppInstallTaskTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    switch (GetParam()) {
      case test::ExternalPrefMigrationTestCases::kDisableMigrationReadPref:
        disabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        disabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
      case test::ExternalPrefMigrationTestCases::kDisableMigrationReadDB:
        disabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        enabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
      case test::ExternalPrefMigrationTestCases::kEnableMigrationReadPref:
        enabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        disabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
      case test::ExternalPrefMigrationTestCases::kEnableMigrationReadDB:
        enabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        enabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ExternallyManagedAppInstallTaskTest(
      const ExternallyManagedAppInstallTaskTest&) = delete;
  ExternallyManagedAppInstallTaskTest& operator=(
      const ExternallyManagedAppInstallTaskTest&) = delete;
  ~ExternallyManagedAppInstallTaskTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    url_loader_ = std::make_unique<TestWebAppUrlLoader>();

    auto* provider = FakeWebAppProvider::Get(profile());
    provider->SetDefaultFakeSubsystems();
    registrar_ = &provider->GetRegistrarMutable();
    command_scheduler_ = &provider->scheduler();
    ui_manager_ = static_cast<FakeWebAppUiManager*>(&provider->GetUiManager());

    auto install_finalizer =
        std::make_unique<TestExternallyManagedAppInstallFinalizer>(
            &provider->GetRegistrarMutable());
    install_finalizer_ = install_finalizer.get();
    provider->SetInstallFinalizer(std::move(install_finalizer));

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 protected:
  bool IsPlaceholderApp(const GURL& url) {
    return registrar()
        ->LookupPlaceholderAppId(url, WebAppManagement::kPolicy)
        .has_value();
  }

  TestWebAppUrlLoader& url_loader() { return *url_loader_; }

  FakeWebAppUiManager* ui_manager() { return ui_manager_; }
  WebAppRegistrar* registrar() { return registrar_; }
  TestExternallyManagedAppInstallFinalizer* finalizer() {
    return install_finalizer_;
  }
  WebAppCommandScheduler* command_scheduler() { return command_scheduler_; }

  FakeDataRetriever* data_retriever() { return data_retriever_; }

  const WebAppInstallInfo& web_app_info() {
    DCHECK_EQ(1u, install_finalizer_->web_app_info_list().size());
    return install_finalizer_->web_app_info_list().at(0);
  }

  const WebAppInstallFinalizer::FinalizeOptions& finalize_options() {
    DCHECK_EQ(1u, install_finalizer_->finalize_options_list().size());
    return install_finalizer_->finalize_options_list().at(0);
  }

  std::unique_ptr<ExternallyManagedAppInstallTask>
  GetInstallationTaskWithTestMocks(ExternalInstallOptions options,
                                   bool mock_empty_web_app_info = false) {
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever_ = data_retriever.get();

    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = options.install_url;
    manifest->id = GenerateManifestIdFromStartUrlOnly(options.install_url);
    manifest->name = u"Manifest Name";

    if (!mock_empty_web_app_info)
      data_retriever_->SetRendererWebAppInstallInfo(
          std::make_unique<WebAppInstallInfo>());

    data_retriever_->SetManifest(
        std::move(manifest), webapps::InstallableStatusCode::NO_ERROR_DETECTED);

    data_retriever_->SetIcons(IconsMap{});

    install_finalizer_->SetNextFinalizeInstallResult(
        options.install_url, webapps::InstallResultCode::kSuccessNewInstall);

    auto task = std::make_unique<ExternallyManagedAppInstallTask>(
        profile(), url_loader_.get(), ui_manager_, install_finalizer_,
        command_scheduler_, std::move(options));
    task->SetDataRetrieverFactoryForTesting(
        GetFactoryForRetriever(std::move(data_retriever)));
    return task;
  }

 private:
  std::unique_ptr<TestWebAppUrlLoader> url_loader_;
  raw_ptr<WebAppCommandScheduler> command_scheduler_ = nullptr;
  raw_ptr<WebAppRegistrar> registrar_ = nullptr;
  raw_ptr<FakeDataRetriever> data_retriever_ = nullptr;
  raw_ptr<TestExternallyManagedAppInstallFinalizer> install_finalizer_ =
      nullptr;
  raw_ptr<FakeWebAppUiManager> ui_manager_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ExternallyManagedAppInstallTaskTest, InstallSucceeds) {
  const GURL kWebAppUrl("https://foo.example");
  auto task = GetInstallationTaskWithTestMocks(
      {kWebAppUrl, absl::nullopt, ExternalInstallSource::kInternalDefault});
  // PrepareForLoad happens twice: once for the URL, once before retrieving the
  // icons.
  url_loader().AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded,
                                         WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;

  task->Install(web_contents(),
                base::BindLambdaForTesting(
                    [&](ExternallyManagedAppManager::InstallResult result) {
                      absl::optional<AppId> id =
                          registrar()->LookupExternalAppId(kWebAppUrl);

                      EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_FALSE(IsPlaceholderApp(kWebAppUrl));

                      EXPECT_EQ(result.app_id.value(), id.value());

                      EXPECT_EQ(0u, finalizer()->num_reparent_tab_calls());

                      EXPECT_EQ(web_app_info().user_display_mode,
                                mojom::UserDisplayMode::kBrowser);
                      EXPECT_EQ(webapps::WebappInstallSource::INTERNAL_DEFAULT,
                                finalize_options().install_surface);

                      run_loop.Quit();
                    }));

  run_loop.Run();
}

TEST_P(ExternallyManagedAppInstallTaskTest, InstallFails) {
  const GURL kWebAppUrl("https://foo.example");
  auto task = GetInstallationTaskWithTestMocks(
      {kWebAppUrl, mojom::UserDisplayMode::kStandalone,
       ExternalInstallSource::kInternalDefault},
      /*mock_empty_web_app_info=*/true);
  url_loader().AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded,
                                         WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;

  task->Install(
      web_contents(),
      base::BindLambdaForTesting(
          [&](ExternallyManagedAppManager::InstallResult result) {
            absl::optional<AppId> id =
                registrar()->LookupExternalAppId(kWebAppUrl);

            EXPECT_EQ(webapps::InstallResultCode::kGetWebAppInstallInfoFailed,
                      result.code);
            EXPECT_FALSE(result.app_id.has_value());

            EXPECT_FALSE(id.has_value());

            run_loop.Quit();
          }));

  run_loop.Run();
}

TEST_P(ExternallyManagedAppInstallTaskTest, InstallForcedContainerWindow) {
  const GURL kWebAppUrl("https://foo.example");
  auto install_options =
      ExternalInstallOptions(kWebAppUrl, mojom::UserDisplayMode::kStandalone,
                             ExternalInstallSource::kInternalDefault);
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));
  url_loader().AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded,
                                         WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  task->Install(web_contents(),
                base::BindLambdaForTesting(
                    [&](ExternallyManagedAppManager::InstallResult result) {
                      EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());
                      EXPECT_EQ(web_app_info().user_display_mode,
                                mojom::UserDisplayMode::kStandalone);
                      run_loop.Quit();
                    }));

  run_loop.Run();
}

TEST_P(ExternallyManagedAppInstallTaskTest, InstallForcedContainerTab) {
  const GURL kWebAppUrl("https://foo.example");
  auto install_options =
      ExternalInstallOptions(kWebAppUrl, mojom::UserDisplayMode::kBrowser,
                             ExternalInstallSource::kInternalDefault);
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));
  url_loader().AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded,
                                         WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  task->Install(web_contents(),
                base::BindLambdaForTesting(
                    [&](ExternallyManagedAppManager::InstallResult result) {
                      EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());
                      EXPECT_EQ(web_app_info().user_display_mode,
                                mojom::UserDisplayMode::kBrowser);
                      run_loop.Quit();
                    }));

  run_loop.Run();
}

TEST_P(ExternallyManagedAppInstallTaskTest, InstallPreinstalledApp) {
  const GURL kWebAppUrl("https://foo.example");
  auto install_options = ExternalInstallOptions(
      kWebAppUrl, absl::nullopt, ExternalInstallSource::kInternalDefault);
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));
  url_loader().AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded,
                                         WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  task->Install(web_contents(),
                base::BindLambdaForTesting(
                    [&](ExternallyManagedAppManager::InstallResult result) {
                      EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_EQ(webapps::WebappInstallSource::INTERNAL_DEFAULT,
                                finalize_options().install_surface);
                      run_loop.Quit();
                    }));

  run_loop.Run();
}

TEST_P(ExternallyManagedAppInstallTaskTest, InstallAppFromPolicy) {
  const GURL kWebAppUrl("https://foo.example");
  auto install_options = ExternalInstallOptions(
      kWebAppUrl, absl::nullopt, ExternalInstallSource::kExternalPolicy);
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));
  url_loader().AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded,
                                         WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  task->Install(web_contents(),
                base::BindLambdaForTesting(
                    [&](ExternallyManagedAppManager::InstallResult result) {
                      EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                                result.code);
                      EXPECT_TRUE(result.app_id.has_value());

                      EXPECT_EQ(webapps::WebappInstallSource::EXTERNAL_POLICY,
                                finalize_options().install_surface);
                      run_loop.Quit();
                    }));

  run_loop.Run();
}

TEST_P(ExternallyManagedAppInstallTaskTest, InstallPlaceholder) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  auto task = GetInstallationTaskWithTestMocks(std::move(options));
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(
      kWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

  base::RunLoop run_loop;
  task->Install(
      web_contents(),
      base::BindLambdaForTesting(
          [&](ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                      result.code);
            EXPECT_TRUE(result.app_id.has_value());

            EXPECT_TRUE(IsPlaceholderApp(kWebAppUrl));

            EXPECT_EQ(1u, finalizer()->finalize_options_list().size());
            EXPECT_EQ(webapps::WebappInstallSource::EXTERNAL_POLICY,
                      finalize_options().install_surface);
            const WebAppInstallInfo& web_app_info =
                finalizer()->web_app_info_list().at(0);

            EXPECT_EQ(base::UTF8ToUTF16(kWebAppUrl.spec()), web_app_info.title);
            EXPECT_EQ(kWebAppUrl, web_app_info.start_url);
            EXPECT_EQ(web_app_info.user_display_mode,
                      mojom::UserDisplayMode::kStandalone);
            EXPECT_TRUE(web_app_info.manifest_icons.empty());
            EXPECT_TRUE(web_app_info.icon_bitmaps.any.empty());

            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_P(ExternallyManagedAppInstallTaskTest, InstallPlaceholderTwice) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  AppId placeholder_app_id;

  // Install a placeholder app.
  {
    auto task = GetInstallationTaskWithTestMocks(options);
    url_loader().SetPrepareForLoadResultLoaded();
    url_loader().SetNextLoadUrlResult(
        kWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

    base::RunLoop run_loop;
    task->Install(
        web_contents(),
        base::BindLambdaForTesting(
            [&](ExternallyManagedAppManager::InstallResult result) {
              EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                        result.code);
              placeholder_app_id = result.app_id.value();

              EXPECT_EQ(1u, finalizer()->finalize_options_list().size());
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Try to install it again.
  auto task = GetInstallationTaskWithTestMocks(options);
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(
      kWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

  base::RunLoop run_loop;
  task->Install(web_contents(),
                base::BindLambdaForTesting(
                    [&](ExternallyManagedAppManager::InstallResult result) {
                      EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                                result.code);
                      EXPECT_EQ(placeholder_app_id, result.app_id.value());

                      // There shouldn't be a second call to the finalizer.
                      EXPECT_EQ(1u,
                                finalizer()->finalize_options_list().size());

                      run_loop.Quit();
                    }));
  run_loop.Run();
}

TEST_P(ExternallyManagedAppInstallTaskTest, ReinstallPlaceholderSucceeds) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  AppId placeholder_app_id;

  // Install a placeholder app.
  {
    auto task = GetInstallationTaskWithTestMocks(options);
    url_loader().SetPrepareForLoadResultLoaded();
    url_loader().SetNextLoadUrlResult(
        kWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

    base::RunLoop run_loop;
    task->Install(
        web_contents(),
        base::BindLambdaForTesting(
            [&](ExternallyManagedAppManager::InstallResult result) {
              EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                        result.code);
              placeholder_app_id = result.app_id.value();

              EXPECT_EQ(1u, finalizer()->finalize_options_list().size());
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Replace the placeholder with a real app.
  options.reinstall_placeholder = true;
  auto task = GetInstallationTaskWithTestMocks(options);
  finalizer()->SetNextUninstallExternalWebAppResult(
      kWebAppUrl, webapps::UninstallResultCode::kSuccess);
  url_loader().AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded,
                                         WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  task->Install(
      web_contents(),
      base::BindLambdaForTesting(
          [&](ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                      result.code);
            EXPECT_TRUE(result.app_id.has_value());
            EXPECT_FALSE(IsPlaceholderApp(kWebAppUrl));

            EXPECT_EQ(1u,
                      finalizer()->uninstall_external_web_app_urls().size());
            EXPECT_EQ(kWebAppUrl,
                      finalizer()->uninstall_external_web_app_urls().at(0));

            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_P(ExternallyManagedAppInstallTaskTest, ReinstallPlaceholderFails) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  AppId placeholder_app_id;

  // Install a placeholder app.
  {
    auto task = GetInstallationTaskWithTestMocks(options);
    url_loader().SetPrepareForLoadResultLoaded();
    url_loader().SetNextLoadUrlResult(
        kWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

    base::RunLoop run_loop;
    task->Install(
        web_contents(),
        base::BindLambdaForTesting(
            [&](ExternallyManagedAppManager::InstallResult result) {
              EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                        result.code);
              placeholder_app_id = result.app_id.value();

              EXPECT_EQ(1u, finalizer()->finalize_options_list().size());

              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Replace the placeholder with a real app.
  options.reinstall_placeholder = true;
  auto task = GetInstallationTaskWithTestMocks(options);

  finalizer()->SetNextUninstallExternalWebAppResult(
      kWebAppUrl, webapps::UninstallResultCode::kError);
  url_loader().AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded,
                                         WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                    WebAppUrlLoader::Result::kUrlLoaded);

  base::RunLoop run_loop;
  task->Install(
      web_contents(),
      base::BindLambdaForTesting(
          [&](ExternallyManagedAppManager::InstallResult result) {
            EXPECT_EQ(webapps::InstallResultCode::kFailedPlaceholderUninstall,
                      result.code);
            EXPECT_FALSE(result.app_id.has_value());
            EXPECT_TRUE(IsPlaceholderApp(kWebAppUrl));

            EXPECT_EQ(1u,
                      finalizer()->uninstall_external_web_app_urls().size());
            EXPECT_EQ(kWebAppUrl,
                      finalizer()->uninstall_external_web_app_urls().at(0));

            // There should have been no new calls to install a placeholder.
            EXPECT_EQ(1u, finalizer()->finalize_options_list().size());

            run_loop.Quit();
          }));
  run_loop.Run();
}

#if defined(CHROMEOS)
TEST_P(ExternallyManagedAppInstallTaskTest, InstallPlaceholderCustomName) {
  const GURL kWebAppUrl("https://foo.example");
  const std::string kCustomName("Custom äpp näme");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  options.override_name = kCustomName;
  auto task = GetInstallationTaskWithTestMocks(std::move(options));
  url_loader().AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded,
                                         WebAppUrlLoader::Result::kUrlLoaded});
  url_loader().SetNextLoadUrlResult(
      kWebAppUrl, WebAppUrlLoader::Result::kRedirectedUrlLoaded);

  base::RunLoop run_loop;
  task->Install(web_contents(),
                base::BindLambdaForTesting(
                    [&](ExternallyManagedAppManager::InstallResult result) {
                      EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                                result.code);

                      const WebAppInstallInfo& web_app_info =
                          finalizer()->web_app_info_list().at(0);

                      EXPECT_EQ(base::UTF8ToUTF16(kCustomName),
                                web_app_info.title);

                      run_loop.Quit();
                    }));
  run_loop.Run();
}
#endif  // defined(CHROMEOS)

TEST_P(ExternallyManagedAppInstallTaskTest, UninstallAndReplace) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options = {kWebAppUrl, absl::nullopt,
                                    ExternalInstallSource::kInternalDefault};
  AppId app_id;
  {
    // Migrate app1 and app2.
    options.uninstall_and_replace = {"app1", "app2"};

    base::RunLoop run_loop;
    auto task = GetInstallationTaskWithTestMocks(options);
    url_loader().AddPrepareForLoadResults(
        {WebAppUrlLoader::Result::kUrlLoaded,
         WebAppUrlLoader::Result::kUrlLoaded});
    url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                      WebAppUrlLoader::Result::kUrlLoaded);

    task->Install(
        web_contents(),
        base::BindLambdaForTesting(
            [&](ExternallyManagedAppManager::InstallResult result) {
              app_id = result.app_id.value();

              EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                        result.code);
              EXPECT_EQ(result.app_id,
                        *registrar()->LookupExternalAppId(kWebAppUrl));

              run_loop.Quit();
            }));
    run_loop.Run();
  }
  {
    // Migration should run on every install of the app.
    options.uninstall_and_replace = {"app3"};

    base::RunLoop run_loop;
    auto task = GetInstallationTaskWithTestMocks(options);
    url_loader().AddPrepareForLoadResults(
        {WebAppUrlLoader::Result::kUrlLoaded,
         WebAppUrlLoader::Result::kUrlLoaded});
    url_loader().SetNextLoadUrlResult(kWebAppUrl,
                                      WebAppUrlLoader::Result::kUrlLoaded);

    task->Install(
        web_contents(),
        base::BindLambdaForTesting(
            [&](ExternallyManagedAppManager::InstallResult result) {
              EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall,
                        result.code);
              EXPECT_EQ(app_id, result.app_id.value());

              run_loop.Quit();
            }));
    run_loop.Run();
  }
}

TEST_P(ExternallyManagedAppInstallTaskTest, InstallURLLoadFailed) {
  struct ResultPair {
    WebAppUrlLoader::Result loader_result;
    webapps::InstallResultCode install_result;
  } result_pairs[] = {{WebAppUrlLoader::Result::kRedirectedUrlLoaded,
                       webapps::InstallResultCode::kInstallURLRedirected},
                      {WebAppUrlLoader::Result::kFailedUnknownReason,
                       webapps::InstallResultCode::kInstallURLLoadFailed},
                      {WebAppUrlLoader::Result::kFailedPageTookTooLong,
                       webapps::InstallResultCode::kInstallURLLoadTimeOut}};

  for (const auto& result_pair : result_pairs) {
    base::RunLoop run_loop;

    ExternalInstallOptions install_options(
        GURL(), mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kInternalDefault);
    ExternallyManagedAppInstallTask install_task(
        profile(), &url_loader(), ui_manager(), finalizer(),
        command_scheduler(), install_options);
    url_loader().SetPrepareForLoadResultLoaded();
    url_loader().SetNextLoadUrlResult(GURL(), result_pair.loader_result);

    install_task.Install(
        web_contents(),
        base::BindLambdaForTesting(
            [&](ExternallyManagedAppManager::InstallResult result) {
              EXPECT_EQ(result.code, result_pair.install_result);
              run_loop.Quit();
            }));

    run_loop.Run();
  }
}

TEST_P(ExternallyManagedAppInstallTaskTest, InstallFailedWebContentsDestroyed) {
  ExternalInstallOptions install_options(
      GURL(), mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  ExternallyManagedAppInstallTask install_task(
      profile(), &url_loader(), ui_manager(), finalizer(), command_scheduler(),
      install_options);
  url_loader().SetPrepareForLoadResultLoaded();
  url_loader().SetNextLoadUrlResult(
      GURL(), WebAppUrlLoader::Result::kFailedWebContentsDestroyed);

  install_task.Install(
      web_contents(),
      base::BindLambdaForTesting(
          [&](ExternallyManagedAppManager::InstallResult) { NOTREACHED(); }));

  base::RunLoop().RunUntilIdle();
}

TEST_P(ExternallyManagedAppInstallTaskTest, InstallWithWebAppInfoSucceeds) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kSystemInstalled);
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindLambdaForTesting([&kWebAppUrl]() {
    auto info = std::make_unique<WebAppInstallInfo>();
    info->start_url = kWebAppUrl;
    info->scope = kWebAppUrl.GetWithoutFilename();
    info->title = u"Foo Web App";
    return info;
  });

  ExternallyManagedAppInstallTask task(profile(), /*url_loader=*/nullptr,
                                       ui_manager(), finalizer(),
                                       command_scheduler(), std::move(options));

  finalizer()->SetNextFinalizeInstallResult(
      kWebAppUrl, webapps::InstallResultCode::kSuccessNewInstall);

  base::RunLoop run_loop;
  task.Install(
      /*web_contents=*/nullptr,
      base::BindLambdaForTesting(
          [&](ExternallyManagedAppManager::InstallResult result) {
            absl::optional<AppId> id =
                registrar()->LookupExternalAppId(kWebAppUrl);
            EXPECT_EQ(webapps::InstallResultCode::kSuccessOfflineOnlyInstall,
                      result.code);
            EXPECT_TRUE(result.app_id.has_value());

            EXPECT_FALSE(IsPlaceholderApp(kWebAppUrl));

            EXPECT_EQ(result.app_id.value(), id.value());

            EXPECT_EQ(0u, finalizer()->num_reparent_tab_calls());

            EXPECT_EQ(web_app_info().user_display_mode,
                      mojom::UserDisplayMode::kStandalone);
            EXPECT_EQ(webapps::WebappInstallSource::SYSTEM_DEFAULT,
                      finalize_options().install_surface);

            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_P(ExternallyManagedAppInstallTaskTest, InstallWithWebAppInfoFails) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kSystemInstalled);
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindLambdaForTesting([&kWebAppUrl]() {
    auto info = std::make_unique<WebAppInstallInfo>();
    info->start_url = kWebAppUrl;
    info->scope = kWebAppUrl.GetWithoutFilename();
    info->title = u"Foo Web App";
    return info;
  });

  ExternallyManagedAppInstallTask task(profile(), /*url_loader=*/nullptr,
                                       ui_manager(), finalizer(),
                                       command_scheduler(), std::move(options));

  finalizer()->SetNextFinalizeInstallResult(
      kWebAppUrl, webapps::InstallResultCode::kWriteDataFailed);

  base::RunLoop run_loop;

  task.Install(web_contents(),
               base::BindLambdaForTesting(
                   [&](ExternallyManagedAppManager::InstallResult result) {
                     absl::optional<AppId> id =
                         registrar()->LookupExternalAppId(kWebAppUrl);

                     EXPECT_EQ(webapps::InstallResultCode::kWriteDataFailed,
                               result.code);
                     EXPECT_FALSE(result.app_id.has_value());

                     EXPECT_FALSE(id.has_value());

                     run_loop.Quit();
                   }));

  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ExternallyManagedAppInstallTaskTest,
    ::testing::Values(
        test::ExternalPrefMigrationTestCases::kDisableMigrationReadPref,
        test::ExternalPrefMigrationTestCases::kDisableMigrationReadDB,
        test::ExternalPrefMigrationTestCases::kEnableMigrationReadPref,
        test::ExternalPrefMigrationTestCases::kEnableMigrationReadDB),
    test::GetExternalPrefMigrationTestName);

}  // namespace web_app
