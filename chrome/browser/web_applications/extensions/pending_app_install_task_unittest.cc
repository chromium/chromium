// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/pending_app_install_task.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/macros.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/browser/web_applications/test/test_data_retriever.h"
#include "chrome/browser/web_applications/test/test_install_finalizer.h"
#include "chrome/browser/web_applications/test/test_os_integration_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/test/test_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/gurl.h"

namespace web_app {

using Result = PendingAppInstallTask::Result;

namespace {

// Returns a factory that will return |data_retriever| the first time it gets
// called. It will DCHECK if called more than once.
WebAppInstallManager::DataRetrieverFactory GetFactoryForRetriever(
    std::unique_ptr<WebAppDataRetriever> data_retriever) {
  // Ideally we would return this lambda directly but passing a mutable lambda
  // to BindLambdaForTesting results in a OnceCallback which cannot be used as a
  // DataRetrieverFactory because DataRetrieverFactory is a RepeatingCallback.
  // For this reason, wrap the OnceCallback in a repeating callback that DCHECKs
  // if it gets called more than once.
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

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL WebAppUrl() {
  return GURL("https://foo.example");
}

// TODO(ortuno): Move this to ExternallyInstalledWebAppPrefs or replace with a
// method in ExternallyInstalledWebAppPrefs once there is one.
bool IsPlaceholderApp(Profile* profile, const GURL& url) {
  const base::Value* map =
      profile->GetPrefs()->GetDictionary(prefs::kWebAppsExtensionIDs);

  const base::Value* entry = map->FindKey(url.spec());

  return entry->FindBoolKey("is_placeholder").value();
}

class TestPendingAppInstallFinalizer : public InstallFinalizer {
 public:
  explicit TestPendingAppInstallFinalizer(TestAppRegistrar* registrar)
      : registrar_(registrar) {}
  ~TestPendingAppInstallFinalizer() override = default;

  // Returns what would be the AppId if an app is installed with |url|.
  AppId GetAppIdForUrl(const GURL& url) {
    return TestInstallFinalizer::GetAppIdForUrl(url);
  }

  void SetNextFinalizeInstallResult(const GURL& url, InstallResultCode code) {
    DCHECK(!base::Contains(next_finalize_install_results_, url));

    AppId app_id;
    if (code == InstallResultCode::kSuccessNewInstall) {
      app_id = GetAppIdForUrl(url);
    }
    next_finalize_install_results_[url] = {app_id, code};
  }

  void SetNextUninstallExternalWebAppResult(const GURL& app_url,
                                            bool uninstalled) {
    DCHECK(!base::Contains(next_uninstall_external_web_app_results_, app_url));

    next_uninstall_external_web_app_results_[app_url] = {
        GetAppIdForUrl(app_url), uninstalled};
  }

  const std::vector<WebApplicationInfo>& web_app_info_list() {
    return web_app_info_list_;
  }

  const std::vector<FinalizeOptions>& finalize_options_list() {
    return finalize_options_list_;
  }

  const std::vector<GURL>& uninstall_external_web_app_urls() const {
    return uninstall_external_web_app_urls_;
  }

  size_t num_reparent_tab_calls() { return num_reparent_tab_calls_; }

  // InstallFinalizer
  void FinalizeInstall(const WebApplicationInfo& web_app_info,
                       const FinalizeOptions& options,
                       InstallFinalizedCallback callback) override {
    DCHECK(
        base::Contains(next_finalize_install_results_, web_app_info.start_url));

    web_app_info_list_.push_back(web_app_info);
    finalize_options_list_.push_back(options);

    AppId app_id;
    InstallResultCode code;
    std::tie(app_id, code) =
        next_finalize_install_results_[web_app_info.start_url];
    next_finalize_install_results_.erase(web_app_info.start_url);
    const GURL& url = web_app_info.start_url;

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting(
            [&, app_id, url, code, callback = std::move(callback)]() mutable {
              registrar_->AddExternalApp(
                  app_id, {url, ExternalInstallSource::kExternalPolicy});
              std::move(callback).Run(app_id, code);
            }));
  }

  void FinalizeUninstallAfterSync(const AppId& app_id,
                                  UninstallWebAppCallback callback) override {
    NOTREACHED();
  }

  void FinalizeUpdate(const WebApplicationInfo& web_app_info,
                      InstallFinalizedCallback callback) override {
    NOTREACHED();
  }

  void UninstallExternalWebApp(const AppId& app_id,
                               ExternalInstallSource external_install_source,
                               UninstallWebAppCallback callback) override {
    registrar_->RemoveExternalApp(app_id);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*uninstalled=*/true));
  }

  void UninstallExternalWebAppByUrl(
      const GURL& app_url,
      ExternalInstallSource external_install_source,
      UninstallWebAppCallback callback) override {
    DCHECK(base::Contains(next_uninstall_external_web_app_results_, app_url));
    uninstall_external_web_app_urls_.push_back(app_url);

    AppId app_id;
    bool uninstalled;
    std::tie(app_id, uninstalled) =
        next_uninstall_external_web_app_results_[app_url];
    next_uninstall_external_web_app_results_.erase(app_url);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting(
            [&, app_id, uninstalled, callback = std::move(callback)]() mutable {
              if (uninstalled)
                registrar_->RemoveExternalApp(app_id);
              std::move(callback).Run(uninstalled);
            }));
  }

  bool CanUserUninstallFromSync(const AppId& app_id) const override {
    NOTIMPLEMENTED();
    return false;
  }

  void UninstallWebAppFromSyncByUser(
      const AppId& app_dd,
      UninstallWebAppCallback callback) override {
    NOTIMPLEMENTED();
  }

  bool CanUserUninstallExternalApp(const AppId& app_id) const override {
    NOTIMPLEMENTED();
    return false;
  }

  void UninstallExternalAppByUser(const AppId& app_id,
                                  UninstallWebAppCallback callback) override {
    NOTIMPLEMENTED();
  }

  bool WasExternalAppUninstalledByUser(const AppId& app_id) const override {
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

 private:
  TestAppRegistrar* registrar_ = nullptr;

  std::vector<WebApplicationInfo> web_app_info_list_;
  std::vector<FinalizeOptions> finalize_options_list_;
  std::vector<GURL> uninstall_external_web_app_urls_;

  size_t num_reparent_tab_calls_ = 0;

  std::map<GURL, std::pair<AppId, InstallResultCode>>
      next_finalize_install_results_;

  // Maps app URLs to the id of the app that would have been installed for that
  // url and the result of trying to uninstall it.
  std::map<GURL, std::pair<AppId, bool>>
      next_uninstall_external_web_app_results_;

  DISALLOW_COPY_AND_ASSIGN(TestPendingAppInstallFinalizer);
};

}  // namespace

class PendingAppInstallTaskTest : public ChromeRenderViewHostTestHarness {
 public:
  PendingAppInstallTaskTest() {
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kDesktopPWAsWithoutExtensions});
  }

  ~PendingAppInstallTaskTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    auto* provider = TestWebAppProvider::Get(profile());

    auto registrar = std::make_unique<TestAppRegistrar>();
    registrar_ = registrar.get();

    auto install_finalizer =
        std::make_unique<TestPendingAppInstallFinalizer>(registrar.get());
    install_finalizer_ = install_finalizer.get();

    auto install_manager = std::make_unique<WebAppInstallManager>(profile());
    install_manager_ = install_manager.get();

    auto os_integration_manager = std::make_unique<TestOsIntegrationManager>(
        profile(), /*app_shortcut_manager=*/nullptr,
        /*file_handler_manager=*/nullptr);
    os_integration_manager_ = os_integration_manager.get();

    auto ui_manager = std::make_unique<TestWebAppUiManager>();
    ui_manager_ = ui_manager.get();

    provider->SetRegistrar(std::move(registrar));
    provider->SetInstallManager(std::move(install_manager));
    provider->SetInstallFinalizer(std::move(install_finalizer));
    provider->SetWebAppUiManager(std::move(ui_manager));
    provider->SetOsIntegrationManager(std::move(os_integration_manager));

    provider->Start();
    // Start only WebAppInstallManager for real.
    install_manager_->Start();
  }

 protected:
  TestWebAppUiManager* ui_manager() { return ui_manager_; }
  TestAppRegistrar* registrar() { return registrar_; }
  TestPendingAppInstallFinalizer* finalizer() { return install_finalizer_; }
  WebAppInstallManager* install_manager() { return install_manager_; }
  TestOsIntegrationManager* os_integration_manager() {
    return os_integration_manager_;
  }

  TestDataRetriever* data_retriever() { return data_retriever_; }

  const WebApplicationInfo& web_app_info() {
    DCHECK_EQ(1u, install_finalizer_->web_app_info_list().size());
    return install_finalizer_->web_app_info_list().at(0);
  }

  const InstallFinalizer::FinalizeOptions& finalize_options() {
    DCHECK_EQ(1u, install_finalizer_->finalize_options_list().size());
    return install_finalizer_->finalize_options_list().at(0);
  }

  std::unique_ptr<PendingAppInstallTask> GetInstallationTaskWithTestMocks(
      ExternalInstallOptions options) {
    auto data_retriever = std::make_unique<TestDataRetriever>();
    data_retriever_ = data_retriever.get();

    install_manager_->SetDataRetrieverFactoryForTesting(
        GetFactoryForRetriever(std::move(data_retriever)));
    auto manifest = std::make_unique<blink::Manifest>();
    manifest->start_url = options.install_url;
    manifest->name = base::ASCIIToUTF16("Manifest Name");

    data_retriever_->SetRendererWebApplicationInfo(
        std::make_unique<WebApplicationInfo>());

    data_retriever_->SetManifest(std::move(manifest), /*is_installable=*/true);

    data_retriever_->SetIcons(IconsMap{});

    install_finalizer_->SetNextFinalizeInstallResult(
        options.install_url, InstallResultCode::kSuccessNewInstall);

    os_integration_manager_->SetNextCreateShortcutsResult(
        install_finalizer_->GetAppIdForUrl(options.install_url), true);

    auto task = std::make_unique<PendingAppInstallTask>(
        profile(), registrar_, os_integration_manager_, ui_manager_,
        install_finalizer_, install_manager_, std::move(options));
    return task;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  WebAppInstallManager* install_manager_ = nullptr;
  TestAppRegistrar* registrar_ = nullptr;
  TestDataRetriever* data_retriever_ = nullptr;
  TestPendingAppInstallFinalizer* install_finalizer_ = nullptr;
  TestWebAppUiManager* ui_manager_ = nullptr;
  TestOsIntegrationManager* os_integration_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PendingAppInstallTaskTest);
};

class PendingAppInstallTaskWithRunOnOsLoginTest
    : public PendingAppInstallTaskTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kDesktopPWAsRunOnOsLogin},
                                          {});
    PendingAppInstallTaskTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PendingAppInstallTaskTest,
       WebAppOrShortcutFromContents_InstallationSucceeds) {
  auto task = GetInstallationTaskWithTestMocks(
      {WebAppUrl(), DisplayMode::kUndefined,
       ExternalInstallSource::kInternalDefault});

  base::RunLoop run_loop;

  task->Install(
      web_contents(), WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
        base::Optional<AppId> id =
            ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
                .LookupAppId(WebAppUrl());

        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_TRUE(result.app_id.has_value());

        EXPECT_FALSE(IsPlaceholderApp(profile(), WebAppUrl()));

        EXPECT_EQ(result.app_id.value(), id.value());

        EXPECT_EQ(1u, os_integration_manager()->num_create_shortcuts_calls());
        EXPECT_TRUE(os_integration_manager()->did_add_to_desktop().value());

        EXPECT_EQ(
            1u,
            os_integration_manager()->num_add_app_to_quick_launch_bar_calls());
        EXPECT_EQ(0u, finalizer()->num_reparent_tab_calls());

        EXPECT_FALSE(web_app_info().open_as_window);
        EXPECT_EQ(WebappInstallSource::INTERNAL_DEFAULT,
                  finalize_options().install_source);

        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(PendingAppInstallTaskTest,
       WebAppOrShortcutFromContents_InstallationFails) {
  auto task = GetInstallationTaskWithTestMocks(
      {WebAppUrl(), DisplayMode::kStandalone,
       ExternalInstallSource::kInternalDefault});
  data_retriever()->SetRendererWebApplicationInfo(nullptr);

  base::RunLoop run_loop;

  task->Install(
      web_contents(), WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
        base::Optional<AppId> id =
            ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
                .LookupAppId(WebAppUrl());

        EXPECT_EQ(InstallResultCode::kGetWebApplicationInfoFailed, result.code);
        EXPECT_FALSE(result.app_id.has_value());

        EXPECT_FALSE(id.has_value());

        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(PendingAppInstallTaskTest,
       WebAppOrShortcutFromContents_NoDesktopShortcut) {
  ExternalInstallOptions install_options(
      WebAppUrl(), DisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  install_options.add_to_desktop = false;
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));

  base::RunLoop run_loop;

  task->Install(
      web_contents(), WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_TRUE(result.app_id.has_value());

        EXPECT_EQ(1u, os_integration_manager()->num_create_shortcuts_calls());
        EXPECT_FALSE(os_integration_manager()->did_add_to_desktop().value());

        EXPECT_EQ(
            1u,
            os_integration_manager()->num_add_app_to_quick_launch_bar_calls());
        EXPECT_EQ(0u, finalizer()->num_reparent_tab_calls());

        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(PendingAppInstallTaskTest,
       WebAppOrShortcutFromContents_NoQuickLaunchBarShortcut) {
  ExternalInstallOptions install_options(
      WebAppUrl(), DisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  install_options.add_to_quick_launch_bar = false;
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));

  base::RunLoop run_loop;
  task->Install(
      web_contents(), WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_TRUE(result.app_id.has_value());

        EXPECT_EQ(1u, os_integration_manager()->num_create_shortcuts_calls());
        EXPECT_TRUE(os_integration_manager()->did_add_to_desktop().value());

        EXPECT_EQ(
            0u,
            os_integration_manager()->num_add_app_to_quick_launch_bar_calls());
        EXPECT_EQ(0u, finalizer()->num_reparent_tab_calls());

        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(
    PendingAppInstallTaskTest,
    WebAppOrShortcutFromContents_NoDesktopShortcutAndNoQuickLaunchBarShortcut) {
  ExternalInstallOptions install_options(
      WebAppUrl(), DisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  install_options.add_to_desktop = false;
  install_options.add_to_quick_launch_bar = false;
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));

  base::RunLoop run_loop;
  task->Install(
      web_contents(), WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_TRUE(result.app_id.has_value());

        EXPECT_EQ(1u, os_integration_manager()->num_create_shortcuts_calls());
        EXPECT_FALSE(os_integration_manager()->did_add_to_desktop().value());

        EXPECT_EQ(
            0u,
            os_integration_manager()->num_add_app_to_quick_launch_bar_calls());
        EXPECT_EQ(0u, finalizer()->num_reparent_tab_calls());

        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(PendingAppInstallTaskTest,
       WebAppOrShortcutFromContents_ForcedContainerWindow) {
  auto install_options =
      ExternalInstallOptions(WebAppUrl(), DisplayMode::kStandalone,
                             ExternalInstallSource::kInternalDefault);
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));

  base::RunLoop run_loop;
  task->Install(
      web_contents(), WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_TRUE(result.app_id.has_value());
        EXPECT_TRUE(web_app_info().open_as_window);
        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(PendingAppInstallTaskTest,
       WebAppOrShortcutFromContents_ForcedContainerTab) {
  auto install_options =
      ExternalInstallOptions(WebAppUrl(), DisplayMode::kBrowser,
                             ExternalInstallSource::kInternalDefault);
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));

  base::RunLoop run_loop;
  task->Install(
      web_contents(), WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_TRUE(result.app_id.has_value());
        EXPECT_FALSE(web_app_info().open_as_window);
        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(PendingAppInstallTaskTest, WebAppOrShortcutFromContents_DefaultApp) {
  auto install_options =
      ExternalInstallOptions(WebAppUrl(), DisplayMode::kUndefined,
                             ExternalInstallSource::kInternalDefault);
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));

  base::RunLoop run_loop;
  task->Install(
      web_contents(), WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_TRUE(result.app_id.has_value());

        EXPECT_EQ(WebappInstallSource::INTERNAL_DEFAULT,
                  finalize_options().install_source);
        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(PendingAppInstallTaskTest, WebAppOrShortcutFromContents_AppFromPolicy) {
  auto install_options =
      ExternalInstallOptions(WebAppUrl(), DisplayMode::kUndefined,
                             ExternalInstallSource::kExternalPolicy);
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));

  base::RunLoop run_loop;
  task->Install(
      web_contents(), WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_TRUE(result.app_id.has_value());

        EXPECT_EQ(WebappInstallSource::EXTERNAL_POLICY,
                  finalize_options().install_source);
        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(PendingAppInstallTaskTest, InstallPlaceholder) {
  ExternalInstallOptions options(WebAppUrl(), DisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  auto task = GetInstallationTaskWithTestMocks(std::move(options));

  base::RunLoop run_loop;
  task->Install(
      web_contents(), WebAppUrlLoader::Result::kRedirectedUrlLoaded,
      base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_TRUE(result.app_id.has_value());

        EXPECT_TRUE(IsPlaceholderApp(profile(), WebAppUrl()));

        EXPECT_EQ(1u, os_integration_manager()->num_create_shortcuts_calls());
        EXPECT_EQ(1u, finalizer()->finalize_options_list().size());
        EXPECT_EQ(WebappInstallSource::EXTERNAL_POLICY,
                  finalize_options().install_source);
        const WebApplicationInfo& web_app_info =
            finalizer()->web_app_info_list().at(0);

        EXPECT_EQ(base::UTF8ToUTF16(WebAppUrl().spec()), web_app_info.title);
        EXPECT_EQ(WebAppUrl(), web_app_info.start_url);
        EXPECT_TRUE(web_app_info.open_as_window);
        EXPECT_TRUE(web_app_info.icon_infos.empty());
        EXPECT_TRUE(web_app_info.icon_bitmaps_any.empty());

        run_loop.Quit();
      }));
  run_loop.Run();
}

// Tests that palceholders are correctly installed when the platform doesn't
// support os shortcuts.
TEST_F(PendingAppInstallTaskTest, InstallPlaceholderNoCreateOsShorcuts) {
  ExternalInstallOptions options(WebAppUrl(), DisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  auto task = GetInstallationTaskWithTestMocks(std::move(options));
  os_integration_manager()->set_can_create_shortcuts(false);

  base::RunLoop run_loop;
  task->Install(
      web_contents(), WebAppUrlLoader::Result::kRedirectedUrlLoaded,
      base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_TRUE(result.app_id.has_value());

        EXPECT_TRUE(IsPlaceholderApp(profile(), WebAppUrl()));

        EXPECT_EQ(0u, os_integration_manager()->num_create_shortcuts_calls());
        EXPECT_EQ(1u, finalizer()->finalize_options_list().size());
        EXPECT_EQ(WebappInstallSource::EXTERNAL_POLICY,
                  finalize_options().install_source);
        const WebApplicationInfo& web_app_info =
            finalizer()->web_app_info_list().at(0);

        EXPECT_EQ(base::UTF8ToUTF16(WebAppUrl().spec()), web_app_info.title);
        EXPECT_EQ(WebAppUrl(), web_app_info.start_url);
        EXPECT_TRUE(web_app_info.open_as_window);
        EXPECT_TRUE(web_app_info.icon_infos.empty());
        EXPECT_TRUE(web_app_info.icon_bitmaps_any.empty());

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(PendingAppInstallTaskTest, InstallPlaceholderTwice) {
  ExternalInstallOptions options(WebAppUrl(), DisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  AppId placeholder_app_id;

  // Install a placeholder app.
  {
    auto task = GetInstallationTaskWithTestMocks(options);
    base::RunLoop run_loop;
    task->Install(
        web_contents(), WebAppUrlLoader::Result::kRedirectedUrlLoaded,
        base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
          EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
          placeholder_app_id = result.app_id.value();

          EXPECT_EQ(1u, finalizer()->finalize_options_list().size());
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Try to install it again.
  auto task = GetInstallationTaskWithTestMocks(options);
  base::RunLoop run_loop;
  task->Install(
      web_contents(), WebAppUrlLoader::Result::kRedirectedUrlLoaded,
      base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_EQ(placeholder_app_id, result.app_id.value());

        // There shouldn't be a second call to the finalizer.
        EXPECT_EQ(1u, finalizer()->finalize_options_list().size());

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(PendingAppInstallTaskTest, ReinstallPlaceholderSucceeds) {
  ExternalInstallOptions options(WebAppUrl(), DisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  AppId placeholder_app_id;

  // Install a placeholder app.
  {
    auto task = GetInstallationTaskWithTestMocks(options);

    base::RunLoop run_loop;
    task->Install(
        web_contents(), WebAppUrlLoader::Result::kRedirectedUrlLoaded,
        base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
          EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
          placeholder_app_id = result.app_id.value();

          EXPECT_EQ(1u, finalizer()->finalize_options_list().size());
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Replace the placeholder with a real app.
  options.reinstall_placeholder = true;
  auto task = GetInstallationTaskWithTestMocks(options);
  finalizer()->SetNextUninstallExternalWebAppResult(WebAppUrl(), true);

  base::RunLoop run_loop;
  task->Install(
      web_contents(), WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_TRUE(result.app_id.has_value());
        EXPECT_FALSE(IsPlaceholderApp(profile(), WebAppUrl()));

        EXPECT_EQ(1u, finalizer()->uninstall_external_web_app_urls().size());
        EXPECT_EQ(WebAppUrl(),
                  finalizer()->uninstall_external_web_app_urls().at(0));

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(PendingAppInstallTaskTest, ReinstallPlaceholderFails) {
  ExternalInstallOptions options(WebAppUrl(), DisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  AppId placeholder_app_id;

  // Install a placeholder app.
  {
    auto task = GetInstallationTaskWithTestMocks(options);
    base::RunLoop run_loop;
    task->Install(
        web_contents(), WebAppUrlLoader::Result::kRedirectedUrlLoaded,
        base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
          EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
          placeholder_app_id = result.app_id.value();

          EXPECT_EQ(1u, finalizer()->finalize_options_list().size());

          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Replace the placeholder with a real app.
  options.reinstall_placeholder = true;
  auto task = GetInstallationTaskWithTestMocks(options);

  finalizer()->SetNextUninstallExternalWebAppResult(WebAppUrl(), false);

  base::RunLoop run_loop;
  task->Install(
      web_contents(), WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
        EXPECT_EQ(InstallResultCode::kFailedPlaceholderUninstall, result.code);
        EXPECT_FALSE(result.app_id.has_value());
        EXPECT_TRUE(IsPlaceholderApp(profile(), WebAppUrl()));

        EXPECT_EQ(1u, finalizer()->uninstall_external_web_app_urls().size());
        EXPECT_EQ(WebAppUrl(),
                  finalizer()->uninstall_external_web_app_urls().at(0));

        // There should have been no new calls to install a placeholder.
        EXPECT_EQ(1u, finalizer()->finalize_options_list().size());

        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(PendingAppInstallTaskTest, UninstallAndReplace) {
  ExternalInstallOptions options = {WebAppUrl(), DisplayMode::kUndefined,
                                    ExternalInstallSource::kInternalDefault};
  AppId app_id;
  {
    // Migrate app1 and app2.
    options.uninstall_and_replace = {"app1", "app2"};

    base::RunLoop run_loop;
    auto task = GetInstallationTaskWithTestMocks(options);
    task->Install(
        web_contents(), WebAppUrlLoader::Result::kUrlLoaded,
        base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
          app_id = *result.app_id;

          EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
          EXPECT_EQ(app_id,
                    *ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
                         .LookupAppId(WebAppUrl()));

          EXPECT_TRUE(ui_manager()->DidUninstallAndReplace("app1", app_id));
          EXPECT_TRUE(ui_manager()->DidUninstallAndReplace("app2", app_id));

          run_loop.Quit();
        }));
    run_loop.Run();
  }
  {
    // Migration should run on every install of the app.
    options.uninstall_and_replace = {"app3"};

    base::RunLoop run_loop;
    auto task = GetInstallationTaskWithTestMocks(options);
    task->Install(
        web_contents(), WebAppUrlLoader::Result::kUrlLoaded,
        base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
          EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
          EXPECT_EQ(app_id, *result.app_id);

          EXPECT_TRUE(ui_manager()->DidUninstallAndReplace("app3", app_id));

          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

TEST_F(PendingAppInstallTaskTest, InstallURLLoadFailed) {
  struct ResultPair {
    WebAppUrlLoader::Result loader_result;
    InstallResultCode install_result;
  } result_pairs[] = {{WebAppUrlLoader::Result::kRedirectedUrlLoaded,
                       InstallResultCode::kInstallURLRedirected},
                      {WebAppUrlLoader::Result::kFailedUnknownReason,
                       InstallResultCode::kInstallURLLoadFailed},
                      {WebAppUrlLoader::Result::kFailedPageTookTooLong,
                       InstallResultCode::kInstallURLLoadTimeOut}};

  for (const auto& result_pair : result_pairs) {
    base::RunLoop run_loop;

    ExternalInstallOptions install_options(
        GURL(), DisplayMode::kStandalone,
        ExternalInstallSource::kInternalDefault);
    PendingAppInstallTask install_task(
        profile(), registrar(), os_integration_manager(), ui_manager(),
        finalizer(), install_manager(), install_options);

    install_task.Install(
        web_contents(), result_pair.loader_result,
        base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
          EXPECT_EQ(result.code, result_pair.install_result);
          run_loop.Quit();
        }));

    run_loop.Run();
  }
}

TEST_F(PendingAppInstallTaskTest, FailedWebContentsDestroyed) {
  ExternalInstallOptions install_options(
      GURL(), DisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  PendingAppInstallTask install_task(
      profile(), registrar(), os_integration_manager(), ui_manager(),
      finalizer(), install_manager(), install_options);

  install_task.Install(
      web_contents(), WebAppUrlLoader::Result::kFailedWebContentsDestroyed,
      base::BindLambdaForTesting(
          [&](PendingAppInstallTask::Result) { NOTREACHED(); }));

  base::RunLoop().RunUntilIdle();
}

TEST_F(PendingAppInstallTaskWithRunOnOsLoginTest,
       WebAppOrShortcutFromContents_RunOnOsLogin) {
  ExternalInstallOptions install_options(
      WebAppUrl(), DisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  install_options.run_on_os_login = true;

  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));

  base::RunLoop run_loop;

  task->Install(
      web_contents(), WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_TRUE(result.app_id.has_value());

        EXPECT_EQ(1u, os_integration_manager()->num_create_shortcuts_calls());
        EXPECT_TRUE(os_integration_manager()->did_add_to_desktop().value());

        EXPECT_EQ(
            1u, os_integration_manager()->num_register_run_on_os_login_calls());

        EXPECT_EQ(
            1u,
            os_integration_manager()->num_add_app_to_quick_launch_bar_calls());
        EXPECT_EQ(0u, finalizer()->num_reparent_tab_calls());

        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(PendingAppInstallTaskWithRunOnOsLoginTest,
       WebAppOrShortcutFromContents_NoRunOnOsLogin) {
  ExternalInstallOptions install_options(
      WebAppUrl(), DisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  install_options.run_on_os_login = false;
  auto task = GetInstallationTaskWithTestMocks(std::move(install_options));

  base::RunLoop run_loop;

  task->Install(
      web_contents(), WebAppUrlLoader::Result::kUrlLoaded,
      base::BindLambdaForTesting([&](PendingAppInstallTask::Result result) {
        EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result.code);
        EXPECT_TRUE(result.app_id.has_value());

        EXPECT_EQ(1u, os_integration_manager()->num_create_shortcuts_calls());
        EXPECT_TRUE(os_integration_manager()->did_add_to_desktop().value());

        EXPECT_EQ(
            0u, os_integration_manager()->num_register_run_on_os_login_calls());

        EXPECT_EQ(
            1u,
            os_integration_manager()->num_add_app_to_quick_launch_bar_calls());
        EXPECT_EQ(0u, finalizer()->num_reparent_tab_calls());

        run_loop.Quit();
      }));

  run_loop.Run();
}

}  // namespace web_app
