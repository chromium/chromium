// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/policy/web_app_policy_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/web_applications/bookmark_apps/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/test_pending_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_ids_map.h"
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

const char kWindowedUrl[] = "https://windowed.example";
const char kTabbedUrl[] = "https://tabbed.example";
const char kDefaultContainerUrl[] = "https://default-container.example";

base::Value GetWindowedItem() {
  base::Value item(base::Value::Type::DICTIONARY);
  item.SetKey(kUrlKey, base::Value(kWindowedUrl));
  item.SetKey(kLaunchContainerKey, base::Value(kLaunchContainerWindowValue));
  return item;
}

PendingAppManager::AppInfo GetWindowedAppInfo() {
  return PendingAppManager::AppInfo(
      GURL(kWindowedUrl), LaunchContainer::kWindow,
      InstallSource::kExternalPolicy, false /* create_shortcuts */);
}

base::Value GetTabbedItem() {
  base::Value item(base::Value::Type::DICTIONARY);
  item.SetKey(kUrlKey, base::Value(kTabbedUrl));
  item.SetKey(kLaunchContainerKey, base::Value(kLaunchContainerTabValue));
  return item;
}

PendingAppManager::AppInfo GetTabbedAppInfo() {
  return PendingAppManager::AppInfo(GURL(kTabbedUrl), LaunchContainer::kTab,
                                    InstallSource::kExternalPolicy,
                                    false /* create_shortcuts */);
}

base::Value GetDefaultContainerItem() {
  base::Value item(base::Value::Type::DICTIONARY);
  item.SetKey(kUrlKey, base::Value(kDefaultContainerUrl));
  return item;
}

PendingAppManager::AppInfo GetDefaultContainerAppInfo() {
  return PendingAppManager::AppInfo(
      GURL(kDefaultContainerUrl), LaunchContainer::kDefault,
      InstallSource::kExternalPolicy, false /* create_shortcuts */);
}

}  // namespace

class WebAppPolicyManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  WebAppPolicyManagerTest() = default;
  ~WebAppPolicyManagerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // Reset WebAppProvider so that its WebAppPolicyManager doesn't interfere
    // with tests.
    web_app::WebAppProvider::Get(profile())->Reset();
  }

  std::string GenerateFakeExtensionId(GURL& url) {
    return crx_file::id_util::GenerateId("fake_app_id_for:" + url.spec());
  }

  void SimulatePreviouslyInstalledApp(
      TestPendingAppManager* pending_app_manager,
      GURL url,
      InstallSource install_source) {
    std::string id = GenerateFakeExtensionId(url);
    extensions::ExtensionRegistry::Get(profile())->AddEnabled(
        extensions::ExtensionBuilder("Dummy Name").SetID(id).Build());

    ExtensionIdsMap extension_ids_map(profile()->GetPrefs());
    extension_ids_map.Insert(url, id, install_source);

    pending_app_manager->SimulatePreviouslyInstalledApp(url, install_source);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebAppPolicyManagerTest);
};

TEST_F(WebAppPolicyManagerTest, NoForceInstalledAppsPrefValue) {
  auto pending_app_manager = std::make_unique<TestPendingAppManager>();
  WebAppPolicyManager web_app_policy_manager(profile(),
                                             pending_app_manager.get());
  base::RunLoop().RunUntilIdle();

  const auto& apps_to_install = pending_app_manager->install_requests();
  EXPECT_TRUE(apps_to_install.empty());
}

TEST_F(WebAppPolicyManagerTest, NoForceInstalledApps) {
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList,
                             base::Value(base::Value::Type::LIST));

  auto pending_app_manager = std::make_unique<TestPendingAppManager>();
  WebAppPolicyManager web_app_policy_manager(profile(),
                                             pending_app_manager.get());
  base::RunLoop().RunUntilIdle();

  const auto& apps_to_install = pending_app_manager->install_requests();
  EXPECT_TRUE(apps_to_install.empty());
}

TEST_F(WebAppPolicyManagerTest, TwoForceInstalledApps) {
  // Add two sites, one that opens in a window and one that opens in a tab.
  base::Value list(base::Value::Type::LIST);
  list.GetList().push_back(GetWindowedItem());
  list.GetList().push_back(GetTabbedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  auto pending_app_manager = std::make_unique<TestPendingAppManager>();
  WebAppPolicyManager web_app_policy_manager(profile(),
                                             pending_app_manager.get());
  base::RunLoop().RunUntilIdle();

  const auto& apps_to_install = pending_app_manager->install_requests();

  std::vector<PendingAppManager::AppInfo> expected_apps_to_install;
  expected_apps_to_install.push_back(GetWindowedAppInfo());
  expected_apps_to_install.push_back(GetTabbedAppInfo());

  EXPECT_EQ(apps_to_install, expected_apps_to_install);
}

TEST_F(WebAppPolicyManagerTest, ForceInstallAppWithNoForcedLaunchContainer) {
  base::Value list(base::Value::Type::LIST);
  list.GetList().push_back(GetDefaultContainerItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  auto pending_app_manager = std::make_unique<TestPendingAppManager>();
  WebAppPolicyManager web_app_policy_manager(profile(),
                                             pending_app_manager.get());
  base::RunLoop().RunUntilIdle();

  const auto& apps_to_install = pending_app_manager->install_requests();

  std::vector<PendingAppManager::AppInfo> expected_apps_to_install;
  expected_apps_to_install.push_back(GetDefaultContainerAppInfo());

  EXPECT_EQ(apps_to_install, expected_apps_to_install);
}

TEST_F(WebAppPolicyManagerTest, DynamicRefresh) {
  base::Value first_list(base::Value::Type::LIST);
  first_list.GetList().push_back(GetWindowedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList,
                             std::move(first_list));

  auto pending_app_manager = std::make_unique<TestPendingAppManager>();
  WebAppPolicyManager web_app_policy_manager(profile(),
                                             pending_app_manager.get());
  base::RunLoop().RunUntilIdle();

  const auto& apps_to_install = pending_app_manager->install_requests();

  std::vector<PendingAppManager::AppInfo> expected_apps_to_install;
  expected_apps_to_install.push_back(GetWindowedAppInfo());

  EXPECT_EQ(apps_to_install, expected_apps_to_install);

  base::Value second_list(base::Value::Type::LIST);
  second_list.GetList().push_back(GetTabbedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList,
                             std::move(second_list));

  base::RunLoop().RunUntilIdle();

  expected_apps_to_install.push_back(GetTabbedAppInfo());

  EXPECT_EQ(apps_to_install, expected_apps_to_install);
}

TEST_F(WebAppPolicyManagerTest, UninstallAppInstalledInPreviousSession) {
  auto pending_app_manager = std::make_unique<TestPendingAppManager>();

  // Simulate two policy apps and a regular app that were installed in the
  // previous session.
  SimulatePreviouslyInstalledApp(pending_app_manager.get(), GURL(kWindowedUrl),
                                 InstallSource::kExternalPolicy);
  SimulatePreviouslyInstalledApp(pending_app_manager.get(), GURL(kTabbedUrl),
                                 InstallSource::kExternalPolicy);
  SimulatePreviouslyInstalledApp(pending_app_manager.get(),
                                 GURL(kDefaultContainerUrl),
                                 InstallSource::kInternal);

  // Push a policy with only one of the apps.
  base::Value first_list(base::Value::Type::LIST);
  first_list.GetList().push_back(GetWindowedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList,
                             std::move(first_list));

  WebAppPolicyManager web_app_policy_manager(profile(),
                                             pending_app_manager.get());
  base::RunLoop().RunUntilIdle();

  // We should only try to install the app in the policy.
  std::vector<PendingAppManager::AppInfo> expected_apps_to_install;
  expected_apps_to_install.push_back(GetWindowedAppInfo());
  EXPECT_EQ(pending_app_manager->install_requests(), expected_apps_to_install);

  // We should try to uninstall the app that is no longer in the policy.
  EXPECT_EQ(std::vector<GURL>({GURL(kTabbedUrl)}),
            pending_app_manager->uninstall_requests());
}

// Tests that we correctly uninstall an app that we installed in the same
// session.
TEST_F(WebAppPolicyManagerTest, UninstallAppInstalledInCurrentSession) {
  auto pending_app_manager = std::make_unique<TestPendingAppManager>();
  WebAppPolicyManager web_app_policy_manager(profile(),
                                             pending_app_manager.get());
  base::RunLoop().RunUntilIdle();

  // Add two sites, one that opens in a window and one that opens in a tab.
  base::Value first_list(base::Value::Type::LIST);
  first_list.GetList().push_back(GetWindowedItem());
  first_list.GetList().push_back(GetTabbedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList,
                             std::move(first_list));
  base::RunLoop().RunUntilIdle();

  const auto& apps_to_install = pending_app_manager->install_requests();

  std::vector<PendingAppManager::AppInfo> expected_apps_to_install;
  expected_apps_to_install.push_back(GetWindowedAppInfo());
  expected_apps_to_install.push_back(GetTabbedAppInfo());

  EXPECT_EQ(apps_to_install, expected_apps_to_install);

  // Push a new policy without the tabbed site.
  base::Value second_list(base::Value::Type::LIST);
  second_list.GetList().push_back(GetWindowedItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList,
                             std::move(second_list));
  base::RunLoop().RunUntilIdle();

  // We'll try to install the app again but PendingAppManager will handle
  // not re-installing the app.
  expected_apps_to_install.push_back(GetWindowedAppInfo());

  EXPECT_EQ(apps_to_install, expected_apps_to_install);

  EXPECT_EQ(std::vector<GURL>({GURL(kTabbedUrl)}),
            pending_app_manager->uninstall_requests());
}

}  // namespace web_app
