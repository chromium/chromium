// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_manager.h"

#include <algorithm>
#include <sstream>
#include <vector>

#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/test/fake_externally_managed_app_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/browser/install_result_code.h"

namespace web_app {

class ExternallyManagedAppManagerTest : public WebAppTest {
 protected:
  void SetUp() override {
    WebAppTest::SetUp();

    fake_registry_controller_ =
        std::make_unique<FakeWebAppRegistryController>();
    controller().SetUp(profile());

    externally_installed_app_prefs_ =
        std::make_unique<ExternallyInstalledWebAppPrefs>(profile()->GetPrefs());
    externally_managed_app_manager_ =
        std::make_unique<FakeExternallyManagedAppManager>(profile());

    externally_managed_app_manager().SetSubsystems(&app_registrar(), nullptr,
                                                   nullptr, nullptr, nullptr);
    externally_managed_app_manager().SetHandleInstallRequestCallback(
        base::BindLambdaForTesting(
            [this](const ExternalInstallOptions& install_options)
                -> ExternallyManagedAppManager::InstallResult {
              const GURL& install_url = install_options.install_url;
              if (!app_registrar().GetAppById(GenerateAppId(
                      /*manifest_id=*/absl::nullopt, install_url))) {
                std::unique_ptr<WebApp> web_app =
                    test::CreateWebApp(install_url, Source::kDefault);
                controller().RegisterApp(std::move(web_app));

                externally_installed_app_prefs().Insert(
                    install_url,
                    GenerateAppId(/*manifest_id=*/absl::nullopt, install_url),
                    install_options.install_source);
                ++deduped_install_count_;
              }
              return ExternallyManagedAppManager::InstallResult(
                  webapps::InstallResultCode::kSuccessNewInstall);
            }));
    externally_managed_app_manager().SetHandleUninstallRequestCallback(
        base::BindLambdaForTesting(
            [this](const GURL& app_url,
                   ExternalInstallSource install_source) -> bool {
              absl::optional<AppId> app_id =
                  app_registrar().LookupExternalAppId(app_url);
              if (app_id) {
                controller().UnregisterApp(*app_id);
                deduped_uninstall_count_++;
              }
              return true;
            }));

    controller().Init();
  }

  void DestroyExternallyManagedAppManager() {
    externally_managed_app_manager_.reset();
  }

  void Sync(const std::vector<GURL>& urls) {
    ResetCounts();

    std::vector<ExternalInstallOptions> install_options_list;
    install_options_list.reserve(urls.size());
    for (const auto& url : urls) {
      install_options_list.emplace_back(
          url, DisplayMode::kStandalone,
          ExternalInstallSource::kInternalDefault);
    }

    base::RunLoop run_loop;
    externally_managed_app_manager().SynchronizeInstalledApps(
        std::move(install_options_list),
        ExternalInstallSource::kInternalDefault,
        base::BindLambdaForTesting(
            [&run_loop, urls](
                std::map<GURL, ExternallyManagedAppManager::InstallResult>
                    install_results,
                std::map<GURL, bool> uninstall_results) { run_loop.Quit(); }));
    // Wait for SynchronizeInstalledApps to finish.
    run_loop.Run();
  }

  void Expect(int deduped_install_count,
              int deduped_uninstall_count,
              const std::vector<GURL>& installed_app_urls) {
    EXPECT_EQ(deduped_install_count, deduped_install_count_);
    EXPECT_EQ(deduped_uninstall_count, deduped_uninstall_count_);
    std::map<AppId, GURL> apps = app_registrar().GetExternallyInstalledApps(
        ExternalInstallSource::kInternalDefault);
    std::vector<GURL> urls;
    urls.reserve(apps.size());
    for (const auto& it : apps)
      urls.push_back(it.second);

    std::sort(urls.begin(), urls.end());
    EXPECT_EQ(installed_app_urls, urls);
  }

  void ResetCounts() {
    deduped_install_count_ = 0;
    deduped_uninstall_count_ = 0;
  }

  FakeWebAppRegistryController& controller() {
    return *fake_registry_controller_;
  }

  web_app::WebAppRegistrar& app_registrar() { return controller().registrar(); }

  ExternallyInstalledWebAppPrefs& externally_installed_app_prefs() {
    return *externally_installed_app_prefs_;
  }

  FakeExternallyManagedAppManager& externally_managed_app_manager() {
    return *externally_managed_app_manager_;
  }

 private:
  int deduped_install_count_ = 0;
  int deduped_uninstall_count_ = 0;

  std::unique_ptr<FakeWebAppRegistryController> fake_registry_controller_;
  std::unique_ptr<ExternallyInstalledWebAppPrefs>
      externally_installed_app_prefs_;
  std::unique_ptr<FakeExternallyManagedAppManager>
      externally_managed_app_manager_;
};

// Test that destroying ExternallyManagedAppManager during a synchronize call
// that installs an app doesn't crash. Regression test for
// https://crbug.com/962808
TEST_F(ExternallyManagedAppManagerTest, DestroyDuringInstallInSynchronize) {
  std::vector<ExternalInstallOptions> install_options_list;
  install_options_list.emplace_back(GURL("https://foo.example"),
                                    DisplayMode::kStandalone,
                                    ExternalInstallSource::kInternalDefault);
  install_options_list.emplace_back(GURL("https://bar.example"),
                                    DisplayMode::kStandalone,
                                    ExternalInstallSource::kInternalDefault);

  externally_managed_app_manager().SynchronizeInstalledApps(
      std::move(install_options_list), ExternalInstallSource::kInternalDefault,
      // ExternallyManagedAppManager gives no guarantees about whether its
      // pending callbacks will be run or not when it gets destroyed.
      base::DoNothing());
  DestroyExternallyManagedAppManager();
  base::RunLoop().RunUntilIdle();
}

// Test that destroying ExternallyManagedAppManager during a synchronize call
// that uninstalls an app doesn't crash. Regression test for
// https://crbug.com/962808
TEST_F(ExternallyManagedAppManagerTest, DestroyDuringUninstallInSynchronize) {
  // Install an app that will be uninstalled next.
  {
    std::vector<ExternalInstallOptions> install_options_list;
    install_options_list.emplace_back(GURL("https://foo.example"),
                                      DisplayMode::kStandalone,
                                      ExternalInstallSource::kInternalDefault);
    base::RunLoop run_loop;
    externally_managed_app_manager().SynchronizeInstalledApps(
        std::move(install_options_list),
        ExternalInstallSource::kInternalDefault,
        base::BindLambdaForTesting(
            [&](std::map<GURL, ExternallyManagedAppManager::InstallResult>
                    install_results,
                std::map<GURL, bool> uninstall_results) { run_loop.Quit(); }));
    run_loop.Run();
  }

  externally_managed_app_manager().SynchronizeInstalledApps(
      std::vector<ExternalInstallOptions>(),
      ExternalInstallSource::kInternalDefault,
      // ExternallyManagedAppManager gives no guarantees about whether its
      // pending callbacks will be run or not when it gets destroyed.
      base::DoNothing());
  DestroyExternallyManagedAppManager();
  base::RunLoop().RunUntilIdle();
}

TEST_F(ExternallyManagedAppManagerTest, SynchronizeInstalledApps) {
  GURL a("https://a.example.com/");
  GURL b("https://b.example.com/");
  GURL c("https://c.example.com/");
  GURL d("https://d.example.com/");
  GURL e("https://e.example.com/");

  Sync(std::vector<GURL>{a, b, d});
  Expect(3, 0, std::vector<GURL>{a, b, d});

  Sync(std::vector<GURL>{b, e});
  Expect(1, 2, std::vector<GURL>{b, e});

  Sync(std::vector<GURL>{e});
  Expect(0, 1, std::vector<GURL>{e});

  Sync(std::vector<GURL>{c});
  Expect(1, 1, std::vector<GURL>{c});

  Sync(std::vector<GURL>{e, a, d});
  Expect(3, 1, std::vector<GURL>{a, d, e});

  Sync(std::vector<GURL>{c, a, b, d, e});
  Expect(2, 0, std::vector<GURL>{a, b, c, d, e});

  Sync(std::vector<GURL>{});
  Expect(0, 5, std::vector<GURL>{});

  // The remaining code tests duplicate inputs.

  Sync(std::vector<GURL>{b, a, b, c});
  Expect(3, 0, std::vector<GURL>{a, b, c});

  Sync(std::vector<GURL>{e, a, e, e, e, a});
  Expect(1, 2, std::vector<GURL>{a, e});

  Sync(std::vector<GURL>{b, c, d});
  Expect(3, 2, std::vector<GURL>{b, c, d});

  Sync(std::vector<GURL>{a, a, a, a, a, a});
  Expect(1, 3, std::vector<GURL>{a});

  Sync(std::vector<GURL>{});
  Expect(0, 1, std::vector<GURL>{});
}

}  // namespace web_app
