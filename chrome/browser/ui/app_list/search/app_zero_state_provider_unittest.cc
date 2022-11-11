// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/app_list/search/app_search_provider_test_base.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/uninstall_reason.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::ManifestLocation;

namespace app_list {

namespace {

constexpr char kRankingNormalAppName[] = "testRankingAppNormal";

base::Time MicrosecondsSinceEpoch(int microseconds) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(microseconds));
}

}  // namespace

class AppZeroStateProviderTest : public AppSearchProviderTestBase {
 public:
  AppZeroStateProviderTest()
      : AppSearchProviderTestBase(/*zero_state_provider=*/true) {}
  AppZeroStateProviderTest(const AppZeroStateProviderTest&) = delete;
  AppZeroStateProviderTest& operator=(const AppZeroStateProviderTest&) = delete;
  ~AppZeroStateProviderTest() override = default;
};

TEST_F(AppZeroStateProviderTest, FetchRecommendations) {
  InitializeSearchProvider();

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile_.get());

  prefs->SetLastLaunchTime(kHostedAppId, MicrosecondsSinceEpoch(20));
  prefs->SetLastLaunchTime(kPackagedApp1Id, MicrosecondsSinceEpoch(10));
  prefs->SetLastLaunchTime(kPackagedApp2Id, MicrosecondsSinceEpoch(5));
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2", RunZeroStateSearch());

  prefs->SetLastLaunchTime(kHostedAppId, MicrosecondsSinceEpoch(5));
  prefs->SetLastLaunchTime(kPackagedApp1Id, MicrosecondsSinceEpoch(10));
  prefs->SetLastLaunchTime(kPackagedApp2Id, MicrosecondsSinceEpoch(20));
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("Packaged App 2,Packaged App 1,Hosted App", RunZeroStateSearch());

  // Times in the future should just be handled as highest priority.
  prefs->SetLastLaunchTime(kHostedAppId, base::Time::Now() + base::Seconds(5));
  prefs->SetLastLaunchTime(kPackagedApp1Id, MicrosecondsSinceEpoch(10));
  prefs->SetLastLaunchTime(kPackagedApp2Id, MicrosecondsSinceEpoch(5));
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2", RunZeroStateSearch());

  // Validate that queried search does not clear out zero state results.
  RunQuery("No matches");
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2",
            GetSortedResultsString());
}

TEST_F(AppZeroStateProviderTest, DefaultRecommendedAppRanking) {
  // Disable the pre-installed high-priority extensions. This test simulates
  // a brand new profile being added to a device, and should not include these.
  service_->UninstallExtension(
      kHostedAppId, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  service_->UninstallExtension(
      kPackagedApp1Id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  service_->UninstallExtension(
      kPackagedApp2Id, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);

  base::RunLoop().RunUntilIdle();

  profile_->SetIsNewProfile(true);
  ASSERT_TRUE(profile()->IsNewProfile());

  // There are four default web apps. We use real app IDs here, as these are
  // used internally by the ranking logic. We can use arbitrary app names.
  //
  // TODO(crbug.com/1235272): There is one default ARC app (PlayStore). Figure
  // out how to test-install PlayStore as a default ARC app.
  const std::vector<std::string> kDefaultRecommendedWebAppIds = {
      web_app::kCanvasAppId, web_app::kHelpAppId, web_app::kOsSettingsAppId,
      web_app::kCameraAppId};
  const std::vector<std::string> kDefaultRecommendedWebAppNames = {
      "Canvas", "Help", "OsSettings", "Camera"};

  ASSERT_EQ(kDefaultRecommendedWebAppNames.size(),
            kDefaultRecommendedWebAppIds.size());

  // Install the default recommended web apps.
  // N.B. These are web apps and not extensions, but these installations are
  // simulated using extensions because it allows us to set the app ID.
  for (size_t i = 0; i < kDefaultRecommendedWebAppNames.size(); ++i) {
    AddExtension(kDefaultRecommendedWebAppIds[i],
                 kDefaultRecommendedWebAppNames[i],
                 ManifestLocation::kExternalPrefDownload,
                 extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
    service_->EnableExtension(kDefaultRecommendedWebAppIds[i]);
  }

  // Allow app installations to finish.
  base::RunLoop().RunUntilIdle();
  InitializeSearchProvider();

  EXPECT_EQ("OsSettings,Help,Canvas,Camera", RunZeroStateSearch());

  // Install a normal (non-default-installed) app.
  const std::string normal_app_id =
      crx_file::id_util::GenerateId(kRankingNormalAppName);
  AddExtension(normal_app_id, kRankingNormalAppName,
               ManifestLocation::kExternalPrefDownload,
               extensions::Extension::NO_FLAGS);
  WaitTimeUpdated();

  extensions::ExtensionPrefs* const prefs =
      extensions::ExtensionPrefs::Get(profile());
  ASSERT_TRUE(prefs);

  // Simulate launching the normal app. Expect that an app with a recorded
  // launch time takes precedence over the default-installed apps.
  prefs->SetLastLaunchTime(normal_app_id, base::Time::Now());
  InitializeSearchProvider();
  EXPECT_EQ(
      std::string(kRankingNormalAppName) + ",OsSettings,Help,Canvas,Camera",
      RunZeroStateSearch());

  // Simulate launching one of the default apps. Expect that this brings it to
  // higher precedence than all the others.
  prefs->SetLastLaunchTime(web_app::kCanvasAppId, base::Time::Now());
  InitializeSearchProvider();
  EXPECT_EQ("Canvas," + std::string(kRankingNormalAppName) +
                ",OsSettings,Help,Camera",
            RunZeroStateSearch());
}

TEST_F(AppZeroStateProviderTest, FetchUnlaunchedRecommendations) {
  InitializeSearchProvider();

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile_.get());

  // The order of unlaunched recommendations should be based on the install time
  // order.
  prefs->SetLastLaunchTime(kHostedAppId, base::Time::Now());
  prefs->SetLastLaunchTime(kPackagedApp1Id, MicrosecondsSinceEpoch(0));
  prefs->SetLastLaunchTime(kPackagedApp2Id, MicrosecondsSinceEpoch(0));
  EXPECT_EQ("Hosted App,Packaged App 1,Packaged App 2", RunZeroStateSearch());
}

}  // namespace app_list
