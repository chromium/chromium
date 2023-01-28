// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/default_chrome_apps_migrator.h"

#include <map>
#include <string>

#include "base/logging.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {
constexpr char kAppId1[] = "aaaa";
constexpr char kAppId2[] = "bbbb";
constexpr char kWebAppUrl1[] = "https://gmail.com";
constexpr char kWebAppUrl2[] = "https://google.com";
constexpr char kUninstallAndReplaceKey[] = "uninstall_and_replace";

// Creates Dict object for WebAppInstallForceList policy from Web App
// parameters.
base::Value::Dict CreateWebAppDict(std::string url,
                                   std::string replaced_extension_id) {
  base::Value::Dict web_app;
  web_app.Set("url", url);
  base::Value::List uninstall_list;
  uninstall_list.Append(replaced_extension_id);
  web_app.Set(kUninstallAndReplaceKey, std::move(uninstall_list));
  return web_app;
}

}  // namespace

class DefaultChromeAppsMigratorTest : public testing::Test {
  void SetUp() override {
    migrator_ = DefaultChromeAppsMigrator(std::map<std::string, std::string>{
        {kAppId1, kWebAppUrl1}, {kAppId2, kWebAppUrl2}});

    // Set initial values for ExtensionInstallForcelist,
    // ExtensionInstallBlocklist and WebAppInstallForceList policies.
    policy_map_.Set(key::kExtensionInstallForcelist, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                    base::Value(base::Value::Type::LIST), nullptr);

    base::Value::List web_app_list;
    base::Value::Dict maps_web_app;
    maps_web_app.Set("url", "https://google.com/maps");
    web_app_list.Append(std::move(maps_web_app));
    policy_map_.Set(key::kWebAppInstallForceList, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                    base::Value(std::move(web_app_list)), nullptr);

    base::Value::List pinned_apps_list;
    pinned_apps_list.Append("ffff");
    policy_map_.Set(key::kPinnedLauncherApps, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                    base::Value(std::move(pinned_apps_list)), nullptr);
  }

 protected:
  DefaultChromeAppsMigrator migrator_;
  PolicyMap policy_map_;
};

TEST_F(DefaultChromeAppsMigratorTest, NoChromeApps) {
  PolicyMap expected_map(policy_map_.Clone());
  migrator_.Migrate(&policy_map_);

  // No Chrome Apps in ExtensionInstallForcelist policy, policy map should not
  // change.
  EXPECT_TRUE(policy_map_.Equals(expected_map));
}

TEST_F(DefaultChromeAppsMigratorTest, ChromeAppWithUpdateUrl) {
  PolicyMap expected_map(policy_map_.Clone());

  // Add force installed Chrome App that should be migrated.
  base::Value::List& forcelist_value =
      policy_map_
          .GetMutableValue(key::kExtensionInstallForcelist,
                           base::Value::Type::LIST)
          ->GetList();
  forcelist_value.Append(std::string(kAppId1) + ";https://example.com");

  // Corresponding web app should be force installed after migration.
  base::Value::Dict web_app = CreateWebAppDict(kWebAppUrl1, kAppId1);

  base::Value::List& web_app_list =
      expected_map
          .GetMutableValue(key::kWebAppInstallForceList,
                           base::Value::Type::LIST)
          ->GetList();
  web_app_list.Append(std::move(web_app));

  migrator_.Migrate(&policy_map_);

  EXPECT_TRUE(policy_map_.Equals(expected_map));
}

TEST_F(DefaultChromeAppsMigratorTest, ChromeAppsAndExtensions) {
  PolicyMap expected_map(policy_map_.Clone());

  // Add two force installed Chrome Apps and two extensions.
  base::Value::List& forcelist_value =
      policy_map_
          .GetMutableValue(key::kExtensionInstallForcelist,
                           base::Value::Type::LIST)
          ->GetList();
  forcelist_value.Append("extension1");
  forcelist_value.Append(kAppId1);
  forcelist_value.Append("extension2");
  forcelist_value.Append(kAppId2);

  // Only extensions should be left now.
  base::Value::List& expected_forcelist =
      expected_map
          .GetMutableValue(key::kExtensionInstallForcelist,
                           base::Value::Type::LIST)
          ->GetList();
  expected_forcelist.Append("extension1");
  expected_forcelist.Append("extension2");

  // Corresponding web apps should be force installed after migration.
  base::Value::Dict first_app = CreateWebAppDict(kWebAppUrl1, kAppId1);
  base::Value::Dict second_app = CreateWebAppDict(kWebAppUrl2, kAppId2);
  base::Value::List& web_app_list =
      expected_map
          .GetMutableValue(key::kWebAppInstallForceList,
                           base::Value::Type::LIST)
          ->GetList();
  web_app_list.Append(std::move(first_app));
  web_app_list.Append(std::move(second_app));

  migrator_.Migrate(&policy_map_);

  EXPECT_TRUE(policy_map_.Equals(expected_map));
}

// Tests the case when WebAppInstallForceList is initially set to wrong type and
// we have to append a web app to it. The value should be overridden and error
// message should be added.
TEST_F(DefaultChromeAppsMigratorTest, WebAppPolicyWrongType) {
  PolicyMap expected_map(policy_map_.Clone());

  // Add force installed Chrome App.
  base::Value::List& forcelist_value =
      policy_map_
          .GetMutableValue(key::kExtensionInstallForcelist,
                           base::Value::Type::LIST)
          ->GetList();
  forcelist_value.Append(kAppId1);

  // Set WebAppInstallForceList to non-list type.
  base::Value web_app_value(base::Value::Type::DICT);
  policy_map_.GetMutable(key::kWebAppInstallForceList)
      ->set_value(std::move(web_app_value));

  base::Value::List web_app_expected_list;
  base::Value::Dict web_app = CreateWebAppDict(kWebAppUrl1, kAppId1);
  web_app_expected_list.Append(std::move(web_app));
  PolicyMap::Entry* web_app_expected_entry =
      expected_map.GetMutable(key::kWebAppInstallForceList);
  web_app_expected_entry->set_value(
      base::Value(std::move(web_app_expected_list)));
  web_app_expected_entry->AddMessage(PolicyMap::MessageType::kError,
                                     IDS_POLICY_TYPE_ERROR);

  migrator_.Migrate(&policy_map_);

  EXPECT_TRUE(policy_map_.Equals(expected_map));
}

TEST_F(DefaultChromeAppsMigratorTest, PinnedApp) {
  PolicyMap expected_map(policy_map_.Clone());

  // Add force installed Chrome App that should be migrated.
  base::Value::List& forcelist_value =
      policy_map_
          .GetMutableValue(key::kExtensionInstallForcelist,
                           base::Value::Type::LIST)
          ->GetList();
  forcelist_value.Append(kAppId1);

  // Make the Chrome App pinned.
  base::Value::List& pinned_apps_value =
      policy_map_
          .GetMutableValue(key::kPinnedLauncherApps, base::Value::Type::LIST)
          ->GetList();
  pinned_apps_value.Append(kAppId1);

  // Corresponding web app should be force installed after migration.
  base::Value::Dict web_app = CreateWebAppDict(kWebAppUrl1, kAppId1);
  base::Value::List& web_app_list =
      expected_map
          .GetMutableValue(key::kWebAppInstallForceList,
                           base::Value::Type::LIST)
          ->GetList();
  web_app_list.Append(std::move(web_app));

  // The corresponding Web App should be pinned.
  base::Value::List& pinned_expected_value =
      expected_map
          .GetMutableValue(key::kPinnedLauncherApps, base::Value::Type::LIST)
          ->GetList();
  pinned_expected_value.Append(kWebAppUrl1);

  migrator_.Migrate(&policy_map_);

  EXPECT_TRUE(policy_map_.Equals(expected_map));
}

}  // namespace policy
