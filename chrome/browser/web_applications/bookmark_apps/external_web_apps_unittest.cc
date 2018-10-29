// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/external_web_apps.h"

#include <algorithm>
#include <vector>

#include "base/feature_list.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

static constexpr char kWebAppDefaultApps[] = "web_app_default_apps";

// Returns the chrome/test/data/web_app_default_apps/sub_dir directory that
// holds the *.json data files from which ScanDirForExternalWebAppsForTesting
// should extract URLs from.
static base::FilePath test_dir(const char* sub_dir) {
  base::FilePath dir;
  if (!base::PathService::Get(chrome::DIR_TEST_DATA, &dir)) {
    ADD_FAILURE()
        << "base::PathService::Get could not resolve chrome::DIR_TEST_DATA";
  }
  return dir.AppendASCII(kWebAppDefaultApps).AppendASCII(sub_dir);
}

using AppInfos = std::vector<web_app::PendingAppManager::AppInfo>;

}  // namespace

class ScanDirForExternalWebAppsTest : public testing::Test {};

TEST_F(ScanDirForExternalWebAppsTest, GoodJson) {
  auto app_infos =
      web_app::ScanDirForExternalWebAppsForTesting(test_dir("good_json"));

  // The good_json directory contains two good JSON files:
  // chrome_platform_status.json and google_io_2016.json.
  // google_io_2016.json is missing a "create_shortcuts" field, so the default
  // value of false should be used.
  EXPECT_EQ(2u, app_infos.size());
  static const web_app::PendingAppManager::AppInfo test_app_infos[] = {
      web_app::PendingAppManager::AppInfo(
          GURL("https://www.chromestatus.com/features"),
          web_app::LaunchContainer::kTab,
          web_app::InstallSource::kExternalDefault,
          /* create_shortcuts */ true,
          web_app::PendingAppManager::AppInfo::
              kDefaultOverridePreviousUserUninstall,
          web_app::PendingAppManager::AppInfo::kDefaultBypassServiceWorkerCheck,
          /* require_manifest */ true),
      web_app::PendingAppManager::AppInfo(
          GURL("https://events.google.com/io2016/?utm_source=web_app_manifest"),
          web_app::LaunchContainer::kWindow,
          web_app::InstallSource::kExternalDefault,
          /* create_shortcuts */ false,
          web_app::PendingAppManager::AppInfo::
              kDefaultOverridePreviousUserUninstall,
          web_app::PendingAppManager::AppInfo::kDefaultBypassServiceWorkerCheck,
          /* require_manifest */ true),
  };
  for (const auto& app_info : test_app_infos) {
    EXPECT_TRUE(base::ContainsValue(app_infos, app_info));
  }
}

TEST_F(ScanDirForExternalWebAppsTest, BadJson) {
  auto app_infos =
      web_app::ScanDirForExternalWebAppsForTesting(test_dir("bad_json"));

  // The bad_json directory contains one (malformed) JSON file.
  EXPECT_EQ(0u, app_infos.size());
}

TEST_F(ScanDirForExternalWebAppsTest, TxtButNoJson) {
  auto app_infos =
      web_app::ScanDirForExternalWebAppsForTesting(test_dir("txt_but_no_json"));

  // The txt_but_no_json directory contains one file, and the contents of that
  // file is valid JSON, but that file's name does not end with ".json".
  EXPECT_EQ(0u, app_infos.size());
}

TEST_F(ScanDirForExternalWebAppsTest, MixedJson) {
  auto app_infos =
      web_app::ScanDirForExternalWebAppsForTesting(test_dir("mixed_json"));

  // The mixed_json directory contains one empty JSON file, one malformed JSON
  // file and one good JSON file. ScanDirForExternalWebAppsForTesting should
  // still pick up that one good JSON file: polytimer.json.
  EXPECT_EQ(1u, app_infos.size());
  if (app_infos.size() == 1) {
    EXPECT_EQ(app_infos[0].url.spec(),
              std::string("https://polytimer.rocks/?homescreen=1"));
  }
}

TEST_F(ScanDirForExternalWebAppsTest, MissingAppUrl) {
  auto app_infos =
      web_app::ScanDirForExternalWebAppsForTesting(test_dir("missing_app_url"));

  // The missing_app_url directory contains one JSON file which is correct
  // except for a missing "app_url" field.
  EXPECT_EQ(0u, app_infos.size());
}

TEST_F(ScanDirForExternalWebAppsTest, InvalidAppUrl) {
  auto app_infos =
      web_app::ScanDirForExternalWebAppsForTesting(test_dir("invalid_app_url"));

  // The invalid_app_url directory contains one JSON file which is correct
  // except for an invalid "app_url" field.
  EXPECT_EQ(0u, app_infos.size());
}

TEST_F(ScanDirForExternalWebAppsTest, InvalidCreateShortcuts) {
  auto app_infos = web_app::ScanDirForExternalWebAppsForTesting(
      test_dir("invalid_create_shortcuts"));

  // The invalid_create_shortcuts directory contains one JSON file which is
  // correct except for an invalid "create_shortctus" field.
  EXPECT_EQ(0u, app_infos.size());
}

TEST_F(ScanDirForExternalWebAppsTest, MissingLaunchContainer) {
  auto app_infos = web_app::ScanDirForExternalWebAppsForTesting(
      test_dir("missing_launch_container"));

  // The missing_launch_container directory contains one JSON file which is
  // correct except for a missing "launch_container" field.
  EXPECT_EQ(0u, app_infos.size());
}

TEST_F(ScanDirForExternalWebAppsTest, InvalidLaunchContainer) {
  auto app_infos = web_app::ScanDirForExternalWebAppsForTesting(
      test_dir("invalid_launch_container"));

  // The invalidg_launch_container directory contains one JSON file which is
  // correct except for an invalid "launch_container" field.
  EXPECT_EQ(0u, app_infos.size());
}

TEST_F(ScanDirForExternalWebAppsTest, EnabledByFinch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      base::Feature{"test_feature_name", base::FEATURE_DISABLED_BY_DEFAULT});
  auto app_infos = web_app::ScanDirForExternalWebAppsForTesting(
      test_dir("enabled_by_finch"));

  // The enabled_by_finch directory contains two JSON file containing apps
  // that have field trials. As the matching featureis enabled, they should be
  // in our list of apps to install.
  EXPECT_EQ(2u, app_infos.size());
}

TEST_F(ScanDirForExternalWebAppsTest, NotEnabledByFinch) {
  auto app_infos = web_app::ScanDirForExternalWebAppsForTesting(
      test_dir("enabled_by_finch"));

  // The enabled_by_finch directory contains two JSON file containing apps
  // that have field trials. As the matching featureis enabled, they should not
  // be in our list of apps to install.
  EXPECT_EQ(0u, app_infos.size());
}
