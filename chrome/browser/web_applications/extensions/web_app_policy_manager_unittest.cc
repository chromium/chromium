// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/policy/web_app_policy_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/test/test_pending_app_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using sync_preferences::TestingPrefServiceSyncable;

namespace web_app {

namespace {

const GURL kWindowedUrl("https://windowed.example/");
const GURL kTabbedUrl("https://tabbed.example/");
const GURL kNoContainerUrl("https://no-container.example/");

base::Value GetWindowedItem() {
  base::Value item(base::Value::Type::DICTIONARY);
  item.SetKey(kUrlKey, base::Value(kWindowedUrl.spec()));
  item.SetKey(kDefaultLaunchContainerKey,
              base::Value(kDefaultLaunchContainerWindowValue));
  return item;
}

ExternalInstallOptions GetWindowedInstallOptions() {
  ExternalInstallOptions options(kWindowedUrl, DisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.reinstall_placeholder = true;
  options.wait_for_windows_closed = true;
  return options;
}

base::Value GetTabbedItem() {
  base::Value item(base::Value::Type::DICTIONARY);
  item.SetKey(kUrlKey, base::Value(kTabbedUrl.spec()));
  item.SetKey(kDefaultLaunchContainerKey,
              base::Value(kDefaultLaunchContainerTabValue));
  return item;
}

ExternalInstallOptions GetTabbedInstallOptions() {
  ExternalInstallOptions options(kTabbedUrl, DisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.reinstall_placeholder = true;
  options.wait_for_windows_closed = true;
  return options;
}

base::Value GetNoContainerItem() {
  base::Value item(base::Value::Type::DICTIONARY);
  item.SetKey(kUrlKey, base::Value(kNoContainerUrl.spec()));
  return item;
}

ExternalInstallOptions GetNoContainerInstallOptions() {
  ExternalInstallOptions options(kNoContainerUrl, DisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.reinstall_placeholder = true;
  options.wait_for_windows_closed = true;
  return options;
}

base::Value GetCreateDesktopShorcutDefaultItem() {
  base::Value item(base::Value::Type::DICTIONARY);
  item.SetKey(kUrlKey, base::Value(kNoContainerUrl.spec()));
  return item;
}

ExternalInstallOptions GetCreateDesktopShorcutDefaultInstallOptions() {
  ExternalInstallOptions options(kNoContainerUrl, DisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.reinstall_placeholder = true;
  options.wait_for_windows_closed = true;
  return options;
}

base::Value GetCreateDesktopShorcutFalseItem() {
  base::Value item(base::Value::Type::DICTIONARY);
  item.SetKey(kUrlKey, base::Value(kNoContainerUrl.spec()));
  item.SetKey(kCreateDesktopShorcutKey, base::Value(false));
  return item;
}

ExternalInstallOptions GetCreateDesktopShorcutFalseInstallOptions() {
  ExternalInstallOptions options(kNoContainerUrl, DisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.reinstall_placeholder = true;
  options.wait_for_windows_closed = true;
  return options;
}

base::Value GetCreateDesktopShorcutTrueItem() {
  base::Value item(base::Value::Type::DICTIONARY);
  item.SetKey(kUrlKey, base::Value(kNoContainerUrl.spec()));
  item.SetKey(kCreateDesktopShorcutKey, base::Value(true));
  return item;
}

ExternalInstallOptions GetCreateDesktopShorcutTrueInstallOptions() {
  ExternalInstallOptions options(kNoContainerUrl, DisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalPolicy);
  options.add_to_applications_menu = true;
  options.add_to_desktop = true;
  options.add_to_quick_launch_bar = false;
  options.install_placeholder = true;
  options.reinstall_placeholder = true;
  options.wait_for_windows_closed = true;
  return options;
}

}  // namespace

class WebAppPolicyManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  WebAppPolicyManagerTest() {}

  ~WebAppPolicyManagerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    auto* provider = TestWebAppProvider::Get(profile());

    auto test_app_registrar = std::make_unique<TestAppRegistrar>();
    test_app_registrar_ = test_app_registrar.get();
    provider->SetRegistrar(std::move(test_app_registrar));

    auto test_pending_app_manager =
        std::make_unique<TestPendingAppManager>(test_app_registrar_);
    test_pending_app_manager_ = test_pending_app_manager.get();
    provider->SetPendingAppManager(std::move(test_pending_app_manager));

    auto web_app_policy_manager =
        std::make_unique<WebAppPolicyManager>(profile());
    web_app_policy_manager_ = web_app_policy_manager.get();
    provider->SetWebAppPolicyManager(std::move(web_app_policy_manager));

    provider->Start();
  }

  void SimulatePreviouslyInstalledApp(GURL url,
                                      ExternalInstallSource install_source) {
    pending_app_manager()->SimulatePreviouslyInstalledApp(url, install_source);
  }

 protected:
  TestPendingAppManager* pending_app_manager() {
    return test_pending_app_manager_;
  }

  WebAppPolicyManager* policy_manager() { return web_app_policy_manager_; }

 private:
  TestAppRegistrar* test_app_registrar_ = nullptr;
  TestPendingAppManager* test_pending_app_manager_ = nullptr;
  WebAppPolicyManager* web_app_policy_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebAppPolicyManagerTest);
};

TEST_F(WebAppPolicyManagerTest, NoForceInstalledAppsPrefValue) {
  policy_manager()->Start();

  base::RunLoop().RunUntilIdle();

  const auto& install_requests = pending_app_manager()->install_requests();
  EXPECT_TRUE(install_requests.empty());
}

TEST_F(WebAppPolicyManagerTest, NoForceInstalledApps) {
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList,
                             base::Value(base::Value::Type::LIST));

  policy_manager()->Start();
  base::RunLoop().RunUntilIdle();

  const auto& install_requests = pending_app_manager()->install_requests();
  EXPECT_TRUE(install_requests.empty());
}

TEST_F(WebAppPolicyManagerTest, TwoForceInstalledApps) {
  // Add two sites, one that opens in a window and one that opens in a tab.
  base::Value list(base::Value::Type::LIST);
  list.Append(GetWindowedItem());
  list.Append(GetTabbedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  policy_manager()->Start();
  base::RunLoop().RunUntilIdle();

  const auto& install_requests = pending_app_manager()->install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetWindowedInstallOptions());
  expected_install_options_list.push_back(GetTabbedInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);
}

TEST_F(WebAppPolicyManagerTest, ForceInstallAppWithNoDefaultLaunchContainer) {
  base::Value list(base::Value::Type::LIST);
  list.Append(GetNoContainerItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  policy_manager()->Start();
  base::RunLoop().RunUntilIdle();

  const auto& install_requests = pending_app_manager()->install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetNoContainerInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);
}

TEST_F(WebAppPolicyManagerTest,
       ForceInstallAppWithDefaultCreateDesktopShorcut) {
  base::Value list(base::Value::Type::LIST);
  list.Append(GetCreateDesktopShorcutDefaultItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  policy_manager()->Start();
  base::RunLoop().RunUntilIdle();

  const auto& install_requests = pending_app_manager()->install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(
      GetCreateDesktopShorcutDefaultInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);
}

TEST_F(WebAppPolicyManagerTest, ForceInstallAppWithCreateDesktopShortcut) {
  base::Value list(base::Value::Type::LIST);
  list.Append(GetCreateDesktopShorcutFalseItem());
  list.Append(GetCreateDesktopShorcutTrueItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  policy_manager()->Start();
  base::RunLoop().RunUntilIdle();

  const auto& install_requests = pending_app_manager()->install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(
      GetCreateDesktopShorcutFalseInstallOptions());
  expected_install_options_list.push_back(
      GetCreateDesktopShorcutTrueInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);
}

TEST_F(WebAppPolicyManagerTest, DynamicRefresh) {
  base::Value first_list(base::Value::Type::LIST);
  first_list.Append(GetWindowedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList,
                             std::move(first_list));

  policy_manager()->Start();
  base::RunLoop().RunUntilIdle();

  const auto& install_requests = pending_app_manager()->install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetWindowedInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);

  base::Value second_list(base::Value::Type::LIST);
  second_list.Append(GetTabbedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList,
                             std::move(second_list));

  base::RunLoop().RunUntilIdle();

  expected_install_options_list.push_back(GetTabbedInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);
}

TEST_F(WebAppPolicyManagerTest, UninstallAppInstalledInPreviousSession) {
  // Simulate two policy apps and a regular app that were installed in the
  // previous session.
  SimulatePreviouslyInstalledApp(kWindowedUrl,
                                 ExternalInstallSource::kExternalPolicy);
  SimulatePreviouslyInstalledApp(kTabbedUrl,
                                 ExternalInstallSource::kExternalPolicy);
  SimulatePreviouslyInstalledApp(kNoContainerUrl,
                                 ExternalInstallSource::kInternalDefault);

  // Push a policy with only one of the apps.
  base::Value first_list(base::Value::Type::LIST);
  first_list.Append(GetWindowedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList,
                             std::move(first_list));

  policy_manager()->Start();
  base::RunLoop().RunUntilIdle();

  // We should only try to install the app in the policy.
  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetWindowedInstallOptions());
  EXPECT_EQ(pending_app_manager()->install_requests(),
            expected_install_options_list);

  // We should try to uninstall the app that is no longer in the policy.
  EXPECT_EQ(std::vector<GURL>({kTabbedUrl}),
            pending_app_manager()->uninstall_requests());
}

// Tests that we correctly uninstall an app that we installed in the same
// session.
TEST_F(WebAppPolicyManagerTest, UninstallAppInstalledInCurrentSession) {
  policy_manager()->Start();
  base::RunLoop().RunUntilIdle();

  // Add two sites, one that opens in a window and one that opens in a tab.
  base::Value first_list(base::Value::Type::LIST);
  first_list.Append(GetWindowedItem());
  first_list.Append(GetTabbedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList,
                             std::move(first_list));
  base::RunLoop().RunUntilIdle();

  const auto& install_requests = pending_app_manager()->install_requests();

  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetWindowedInstallOptions());
  expected_install_options_list.push_back(GetTabbedInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);

  // Push a new policy without the tabbed site.
  base::Value second_list(base::Value::Type::LIST);
  second_list.Append(GetWindowedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList,
                             std::move(second_list));
  base::RunLoop().RunUntilIdle();

  // We'll try to install the app again but PendingAppManager will handle
  // not re-installing the app.
  expected_install_options_list.push_back(GetWindowedInstallOptions());

  EXPECT_EQ(install_requests, expected_install_options_list);

  EXPECT_EQ(std::vector<GURL>({kTabbedUrl}),
            pending_app_manager()->uninstall_requests());
}

// Tests that we correctly reinstall a placeholder app.
TEST_F(WebAppPolicyManagerTest, ReinstallPlaceholderApp) {
  base::Value list(base::Value::Type::LIST);
  list.Append(GetWindowedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  policy_manager()->Start();
  base::RunLoop().RunUntilIdle();

  std::vector<ExternalInstallOptions> expected_options_list;
  expected_options_list.push_back(GetWindowedInstallOptions());

  const auto& install_options_list = pending_app_manager()->install_requests();
  EXPECT_EQ(expected_options_list, install_options_list);

  policy_manager()->ReinstallPlaceholderAppIfNecessary(kWindowedUrl);
  base::RunLoop().RunUntilIdle();

  auto reinstall_options = GetWindowedInstallOptions();
  reinstall_options.install_placeholder = false;
  reinstall_options.reinstall_placeholder = true;
  reinstall_options.wait_for_windows_closed = true;
  expected_options_list.push_back(std::move(reinstall_options));

  EXPECT_EQ(expected_options_list, install_options_list);
}

TEST_F(WebAppPolicyManagerTest, TryToInexistentPlaceholderApp) {
  base::Value list(base::Value::Type::LIST);
  list.Append(GetWindowedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  policy_manager()->Start();
  base::RunLoop().RunUntilIdle();

  std::vector<ExternalInstallOptions> expected_options_list;
  expected_options_list.push_back(GetWindowedInstallOptions());

  const auto& install_options_list = pending_app_manager()->install_requests();
  EXPECT_EQ(expected_options_list, install_options_list);

  // Try to reinstall for app not installed by policy.
  policy_manager()->ReinstallPlaceholderAppIfNecessary(kTabbedUrl);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(expected_options_list, install_options_list);
}

TEST_F(WebAppPolicyManagerTest, SayRefreshTwoTimesQuickly) {
  policy_manager()->Start();
  base::RunLoop().RunUntilIdle();
  // Add an app.
  {
    base::Value list(base::Value::Type::LIST);
    list.Append(GetWindowedItem());
    profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));
  }
  // Before it gets installed, set a policy that uninstalls it.
  {
    base::Value list(base::Value::Type::LIST);
    list.Append(GetTabbedItem());
    profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));
  }
  base::RunLoop().RunUntilIdle();

  // Both apps should have been installed.
  std::vector<ExternalInstallOptions> expected_options_list;
  expected_options_list.push_back(GetWindowedInstallOptions());
  expected_options_list.push_back(GetTabbedInstallOptions());

  const auto& install_options_list = pending_app_manager()->install_requests();
  EXPECT_EQ(expected_options_list, install_options_list);
  EXPECT_EQ(std::vector<GURL>({kWindowedUrl}),
            pending_app_manager()->uninstall_requests());

  // There should be exactly 1 app remaining.
  std::map<AppId, GURL> apps =
      WebAppProviderBase::GetProviderBase(profile())
          ->registrar()
          .GetExternallyInstalledApps(ExternalInstallSource::kExternalPolicy);
  EXPECT_EQ(1u, apps.size());
  for (auto& it : apps)
    EXPECT_EQ(it.second, kTabbedUrl);
}

TEST_F(WebAppPolicyManagerTest, InstallResultHistogram) {
  base::HistogramTester histograms;
  policy_manager()->Start();
  {
    base::Value list(base::Value::Type::LIST);
    list.Append(GetWindowedItem());
    profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

    histograms.ExpectTotalCount(
        WebAppPolicyManager::kInstallResultHistogramName, 0);

    base::RunLoop().RunUntilIdle();

    histograms.ExpectTotalCount(
        WebAppPolicyManager::kInstallResultHistogramName, 1);
    histograms.ExpectBucketCount(
        WebAppPolicyManager::kInstallResultHistogramName,
        InstallResultCode::kSuccessNewInstall, 1);
  }
  {
    base::Value list(base::Value::Type::LIST);
    list.Append(GetTabbedItem());
    list.Append(GetNoContainerItem());
    pending_app_manager()->SetInstallResultCode(
        InstallResultCode::kProfileDestroyed);

    profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

    base::RunLoop().RunUntilIdle();
    histograms.ExpectTotalCount(
        WebAppPolicyManager::kInstallResultHistogramName, 3);
    histograms.ExpectBucketCount(
        WebAppPolicyManager::kInstallResultHistogramName,
        InstallResultCode::kProfileDestroyed, 2);
  }
}

}  // namespace web_app
