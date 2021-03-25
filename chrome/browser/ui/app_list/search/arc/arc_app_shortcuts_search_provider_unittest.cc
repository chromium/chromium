// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_app_shortcuts_search_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/arc/icon_decode_request.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "components/arc/mojom/compatibility_mode.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

namespace {
constexpr char kFakeAppPackageName[] = "FakeAppPackageName";
}  // namespace

class ArcAppShortcutsSearchProviderTest
    : public AppListTestBase,
      public ::testing::WithParamInterface<bool> {
 protected:
  ArcAppShortcutsSearchProviderTest() = default;
  ~ArcAppShortcutsSearchProviderTest() override = default;

  // AppListTestBase:
  void SetUp() override {
    AppListTestBase::SetUp();
    arc_test_.SetUp(profile());
    controller_ = std::make_unique<test::TestAppListControllerDelegate>();
  }

  void TearDown() override {
    controller_.reset();
    arc_test_.TearDown();
    AppListTestBase::TearDown();
  }

  arc::mojom::AppInfo CreateAppInfo(const std::string& name,
                                    const std::string& activity,
                                    const std::string& package_name) {
    arc::mojom::AppInfo appinfo;
    appinfo.name = name;
    appinfo.package_name = package_name;
    appinfo.activity = activity;
    return appinfo;
  }

  std::string AddArcAppAndShortcut(const arc::mojom::AppInfo& app_info,
                                   bool launchable) {
    ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();
    // Adding app to the prefs, and check that the app is accessible by id.
    prefs->AddAppAndShortcut(
        app_info.name, app_info.package_name, app_info.activity,
        std::string() /* intent_uri */, std::string() /* icon_resource_id */,
        false /* sticky */, true /* notifications_enabled */,
        true /* app_ready */, false /* suspended */, false /* shortcut */,
        launchable);
    const std::string app_id =
        ArcAppListPrefs::GetAppId(app_info.package_name, app_info.activity);
    EXPECT_TRUE(prefs->GetApp(app_id));
    return app_id;
  }

  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<test::TestAppListControllerDelegate> controller_;
  ArcAppTest arc_test_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppShortcutsSearchProviderTest);
};

TEST_P(ArcAppShortcutsSearchProviderTest, Basic) {
  const bool launchable = GetParam();

  const std::string app_id = AddArcAppAndShortcut(
      CreateAppInfo("FakeName", "FakeActivity", kFakeAppPackageName),
      launchable);

  const size_t kMaxResults = launchable ? 4 : 0;
  constexpr char kQuery[] = "shortlabel";

  auto provider = std::make_unique<ArcAppShortcutsSearchProvider>(
      kMaxResults, profile(), controller_.get());
  EXPECT_TRUE(provider->results().empty());
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();

  provider->Start(base::UTF8ToUTF16(kQuery));
  const auto& results = provider->results();
  EXPECT_EQ(kMaxResults, results.size());
  // Verify search results.
  for (size_t i = 0; i < results.size(); ++i) {
    EXPECT_EQ(base::StringPrintf("ShortLabel %zu", i),
              base::UTF16ToUTF8(results[i]->title()));
    EXPECT_EQ(ash::SearchResultDisplayType::kTile, results[i]->display_type());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ArcAppShortcutsSearchProviderTest,
                         testing::Bool());

}  // namespace app_list
