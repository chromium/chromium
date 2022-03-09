// Copyright 2022 The Chromium Authors. All rights reserved.
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

    base::Value blocklist_value(base::Value::Type::LIST);
    blocklist_value.Append("eeee");
    policy_map_.Set(key::kExtensionInstallBlocklist, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                    std::move(blocklist_value), nullptr);

    base::Value web_app_value(base::Value::Type::LIST);
    base::Value maps_web_app(base::Value::Type::DICTIONARY);
    maps_web_app.SetStringKey("url", "https://google.com/maps");
    web_app_value.Append(std::move(maps_web_app));
    policy_map_.Set(key::kWebAppInstallForceList, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                    std::move(web_app_value), nullptr);

    base::Value pinned_apps_value(base::Value::Type::LIST);
    pinned_apps_value.Append("ffff");
    policy_map_.Set(key::kPinnedLauncherApps, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                    std::move(pinned_apps_value), nullptr);
  }

 protected:
  DefaultChromeAppsMigrator migrator_;
  PolicyMap policy_map_;
};

TEST_F(DefaultChromeAppsMigratorTest, NoChromeApps) {
  PolicyMap expected_map(policy_map_.Clone());
  migrator_.Migrate(&policy_map_);

  // No chrome apps in ExtensionInstallForcelist policy, policy map should not
  // change.
  EXPECT_TRUE(policy_map_.Equals(expected_map));
}

TEST_F(DefaultChromeAppsMigratorTest, ChromeAppWithUpdateUrl) {
  PolicyMap expected_map(policy_map_.Clone());

  // Add force installed chrome app that should be migrated.
  base::Value* forcelist_value = policy_map_.GetMutableValue(
      key::kExtensionInstallForcelist, base::Value::Type::LIST);
  forcelist_value->Append(std::string(kAppId1) + ";https://example.com");

  // Chrome app should be blocked after migration.
  base::Value* blocklist_value = expected_map.GetMutableValue(
      key::kExtensionInstallBlocklist, base::Value::Type::LIST);
  blocklist_value->Append(std::string(kAppId1));

  // Corresponding web app should be force installed after migration.
  base::Value first_app(base::Value::Type::DICTIONARY);
  first_app.SetStringKey("url", kWebAppUrl1);
  base::Value* web_app_value = expected_map.GetMutableValue(
      key::kWebAppInstallForceList, base::Value::Type::LIST);
  web_app_value->Append(std::move(first_app));

  migrator_.Migrate(&policy_map_);

  EXPECT_TRUE(policy_map_.Equals(expected_map));
}

TEST_F(DefaultChromeAppsMigratorTest, ChromeAppsAndExtensions) {
  PolicyMap expected_map(policy_map_.Clone());

  // Add two force installed chrome apps and two extensions.
  base::Value* forcelist_value = policy_map_.GetMutableValue(
      key::kExtensionInstallForcelist, base::Value::Type::LIST);
  forcelist_value->Append("extension1");
  forcelist_value->Append(kAppId1);
  forcelist_value->Append("extension2");
  forcelist_value->Append(kAppId2);

  // Only extensions should be left now.
  base::Value* expected_forcelist = expected_map.GetMutableValue(
      key::kExtensionInstallForcelist, base::Value::Type::LIST);
  expected_forcelist->Append("extension1");
  expected_forcelist->Append("extension2");

  // Chrome apps should be blocked after migration.
  base::Value* blocklist_value = expected_map.GetMutableValue(
      key::kExtensionInstallBlocklist, base::Value::Type::LIST);
  blocklist_value->Append(kAppId1);
  blocklist_value->Append(kAppId2);

  // Corresponding web apps should be force installed after migration.
  base::Value first_app(base::Value::Type::DICTIONARY);
  first_app.SetStringKey("url", kWebAppUrl1);
  base::Value second_app(base::Value::Type::DICTIONARY);
  second_app.SetStringKey("url", kWebAppUrl2);
  base::Value* web_app_value = expected_map.GetMutableValue(
      key::kWebAppInstallForceList, base::Value::Type::LIST);
  web_app_value->Append(std::move(first_app));
  web_app_value->Append(std::move(second_app));

  migrator_.Migrate(&policy_map_);

  EXPECT_TRUE(policy_map_.Equals(expected_map));
}

// Tests the case when ExtensionInstallBlocklist is initially set to wrong type
// and we have to append chrome app id to it. The value should be overridden and
// error message should be added.
TEST_F(DefaultChromeAppsMigratorTest, ExtensionBlocklistPolicyWrongType) {
  PolicyMap expected_map(policy_map_.Clone());

  // Add force installed chrome app.
  base::Value* forcelist_value = policy_map_.GetMutableValue(
      key::kExtensionInstallForcelist, base::Value::Type::LIST);
  forcelist_value->Append(kAppId1);

  // Set ExtensionInstallBlocklist to non-list type.
  base::Value blocklist_value(base::Value::Type::DICTIONARY);
  policy_map_.GetMutable(key::kExtensionInstallBlocklist)
      ->set_value(std::move(blocklist_value));

  base::Value blocklist_expected_value(base::Value::Type::LIST);
  blocklist_expected_value.Append(kAppId1);
  PolicyMap::Entry* blocklist_expected_entry =
      expected_map.GetMutable(key::kExtensionInstallBlocklist);
  blocklist_expected_entry->set_value(std::move(blocklist_expected_value));
  blocklist_expected_entry->AddMessage(PolicyMap::MessageType::kError,
                                       IDS_POLICY_TYPE_ERROR);

  // Corresponding web app should be force installed after migration.
  base::Value first_app(base::Value::Type::DICTIONARY);
  first_app.SetStringKey("url", kWebAppUrl1);
  base::Value* web_app_value = expected_map.GetMutableValue(
      key::kWebAppInstallForceList, base::Value::Type::LIST);
  web_app_value->Append(std::move(first_app));

  migrator_.Migrate(&policy_map_);

  EXPECT_TRUE(policy_map_.Equals(expected_map));
}

// Tests the case when WebAppInstallForceList is initially set to wrong type and
// we have to append a web app to it. The value should be overridden and error
// message should be added.
TEST_F(DefaultChromeAppsMigratorTest, WebAppPolicyWrongType) {
  PolicyMap expected_map(policy_map_.Clone());

  // Add force installed chrome app.
  base::Value* forcelist_value = policy_map_.GetMutableValue(
      key::kExtensionInstallForcelist, base::Value::Type::LIST);
  forcelist_value->Append(kAppId1);

  // Set WebAppInstallForceList to non-list type.
  base::Value web_app_value(base::Value::Type::DICTIONARY);
  policy_map_.GetMutable(key::kWebAppInstallForceList)
      ->set_value(std::move(web_app_value));

  // Chrome app should be blocked after migration.
  base::Value* blocklist_value = expected_map.GetMutableValue(
      key::kExtensionInstallBlocklist, base::Value::Type::LIST);
  blocklist_value->Append(kAppId1);

  base::Value web_app_expected_value(base::Value::Type::LIST);
  base::Value first_app(base::Value::Type::DICTIONARY);
  first_app.SetStringKey("url", kWebAppUrl1);
  web_app_expected_value.Append(std::move(first_app));
  PolicyMap::Entry* web_app_expected_entry =
      expected_map.GetMutable(key::kWebAppInstallForceList);
  web_app_expected_entry->set_value(std::move(web_app_expected_value));
  web_app_expected_entry->AddMessage(PolicyMap::MessageType::kError,
                                     IDS_POLICY_TYPE_ERROR);

  migrator_.Migrate(&policy_map_);

  EXPECT_TRUE(policy_map_.Equals(expected_map));
}

TEST_F(DefaultChromeAppsMigratorTest, PinnedApp) {
  PolicyMap expected_map(policy_map_.Clone());

  // Add force installed chrome app that should be migrated.
  base::Value* forcelist_value = policy_map_.GetMutableValue(
      key::kExtensionInstallForcelist, base::Value::Type::LIST);
  forcelist_value->Append(std::string(kAppId1));

  // Make the chrome app pinned.
  base::Value* pinned_apps_value = policy_map_.GetMutableValue(
      key::kPinnedLauncherApps, base::Value::Type::LIST);
  pinned_apps_value->Append(std::string(kAppId1));

  // Chrome app should be blocked after migration.
  base::Value* blocklist_value = expected_map.GetMutableValue(
      key::kExtensionInstallBlocklist, base::Value::Type::LIST);
  blocklist_value->Append(std::string(kAppId1));

  // Corresponding web app should be force installed after migration.
  base::Value first_app(base::Value::Type::DICTIONARY);
  first_app.SetStringKey("url", kWebAppUrl1);
  base::Value* web_app_value = expected_map.GetMutableValue(
      key::kWebAppInstallForceList, base::Value::Type::LIST);
  web_app_value->Append(std::move(first_app));

  // The corresponding Web App should be pinned.
  base::Value* pinned_expected_value = expected_map.GetMutableValue(
      key::kPinnedLauncherApps, base::Value::Type::LIST);
  pinned_expected_value->Append(std::string(kWebAppUrl1));

  migrator_.Migrate(&policy_map_);

  EXPECT_TRUE(policy_map_.Equals(expected_map));
}

}  // namespace policy
