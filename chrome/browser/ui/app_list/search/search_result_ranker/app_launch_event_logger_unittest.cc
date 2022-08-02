// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_event_logger.h"

#include <memory>

#include "ash/components/arc/arc_prefs.h"
#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/ukm/app_source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace app_list {

namespace {

const char kGmailChromeApp[] = "pjkljhegncpnkpknbcohdijeoejaedia";
const char kMapsArcApp[] = "gmhipfhgnoelkiiofcnimehjnpaejiel";
const char kCalculatorArcApp[] = "adeiokjnhlgkiokkojlphcelpojmlkpj";

const char kMapsPackageName[] = "com.google.android.apps.maps";
const char kCalculatorPackageName[] = "com.google.android.calculator";

const char kPhotosPwaUrl[] = "http://photos.google.com/";

bool TestIsWebstoreExtension(base::StringPiece id) {
  return (id == kGmailChromeApp);
}

}  // namespace

class AppLaunchEventLoggerForTest : public AppLaunchEventLogger {
 public:
  AppLaunchEventLoggerForTest(extensions::ExtensionRegistry* registry,
                              base::Value::Dict* arc_apps,
                              base::Value::Dict* arc_packages,
                              Profile* profile)
      : AppLaunchEventLogger(profile) {
    arc_apps_ = arc_apps;
    arc_packages_ = arc_packages;
    registry_ = registry;

    // EnforceLoggingPolicy runs in the base constructor without the test data,
    // so run it again here after the test data is set.
    EnforceLoggingPolicy();
  }
};

class AppLaunchEventLoggerTest : public testing::Test {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(ukm::kUkmAppLogging);
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  TestingProfile profile_;
};

TEST_F(AppLaunchEventLoggerTest, CheckUkmCodePWA) {
  web_app::FakeWebAppProvider* provider =
      web_app::FakeWebAppProvider::Get(&profile_);

  std::unique_ptr<web_app::WebApp> web_app =
      web_app::test::CreateWebApp(GURL(kPhotosPwaUrl));
  web_app::AppId app_id = web_app->app_id();
  provider->GetRegistrarMutable().registry().emplace(app_id,
                                                     std::move(web_app));
  provider->Start();

  AppLaunchEventLoggerForTest app_launch_event_logger_(nullptr, nullptr,
                                                       nullptr, &profile_);
  app_launch_event_logger_.OnGridClicked(app_id);

  task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, GURL(kPhotosPwaUrl));
  test_ukm_recorder_.ExpectEntryMetric(entry, "AllClicksLast24Hours", 1);
  test_ukm_recorder_.ExpectEntryMetric(entry, "AllClicksLastHour", 1);
  test_ukm_recorder_.ExpectEntryMetric(entry, "AppType", 3);
  test_ukm_recorder_.ExpectEntryMetric(entry, "LaunchedFrom", 1);
  test_ukm_recorder_.ExpectEntryMetric(entry, "TotalHours", 0);

  const auto click_entries =
      test_ukm_recorder_.GetEntriesByName("AppListAppClickData");
  ASSERT_EQ(1ul, click_entries.size());
  const auto* photos_entry = click_entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(photos_entry, GURL(kPhotosPwaUrl));
  test_ukm_recorder_.ExpectEntryMetric(photos_entry, "AppLaunchId",
                                       entry->source_id);
  test_ukm_recorder_.ExpectEntryMetric(photos_entry, "AppType", 3);
}

TEST_F(AppLaunchEventLoggerTest, CheckUkmCodeChrome) {
  extensions::ExtensionRegistry registry(nullptr);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test")
          .SetID(kGmailChromeApp)
          .AddFlags(extensions::Extension::FROM_WEBSTORE)
          .Build();
  registry.AddEnabled(extension);

  GURL url(std::string("chrome-extension://") + kGmailChromeApp + "/");

  test_ukm_recorder_.SetIsWebstoreExtensionCallback(
      base::BindRepeating(&TestIsWebstoreExtension));

  AppLaunchEventLoggerForTest app_launch_event_logger_(&registry, nullptr,
                                                       nullptr, &profile_);
  app_launch_event_logger_.OnGridClicked(kGmailChromeApp);

  task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, url);
  test_ukm_recorder_.ExpectEntryMetric(entry, "AppType", 1);
  test_ukm_recorder_.ExpectEntryMetric(entry, "LaunchedFrom", 1);
}

TEST_F(AppLaunchEventLoggerTest, CheckUkmCodeArc) {
  base::Value package(base::Value::Type::DICTIONARY);
  package.SetKey(AppLaunchEventLogger::kShouldSync, base::Value(true));

  auto packages = std::make_unique<base::Value::Dict>();
  packages->Set(kMapsPackageName, package.Clone());

  base::Value::Dict app;
  app.Set(AppLaunchEventLogger::kPackageName, base::Value(kMapsPackageName));

  auto arc_apps = std::make_unique<base::Value::Dict>();
  arc_apps->Set(kMapsArcApp, app.Clone());

  AppLaunchEventLoggerForTest app_launch_event_logger_(
      nullptr, arc_apps.get(), packages.get(), &profile_);
  app_launch_event_logger_.OnGridClicked(kMapsArcApp);

  task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(
      entry, GURL("app://play/gbpfhehadcpcndihhameeacbdmbjbhgi"));
  test_ukm_recorder_.ExpectEntryMetric(entry, "AppType", 2);
  test_ukm_recorder_.ExpectEntryMetric(entry, "LaunchedFrom", 1);
}

// TODO(1289705): This test is flaky.
TEST_F(AppLaunchEventLoggerTest, DISABLED_CheckMultipleClicks) {
  web_app::FakeWebAppProvider* provider =
      web_app::FakeWebAppProvider::Get(&profile_);

  std::unique_ptr<web_app::WebApp> web_app =
      web_app::test::CreateWebApp(GURL(kPhotosPwaUrl));
  web_app::AppId app_id = web_app->app_id();
  provider->GetRegistrarMutable().registry().emplace(app_id,
                                                     std::move(web_app));
  provider->Start();

  base::Value maps_package(base::Value::Type::DICTIONARY);
  base::Value calculator_package(base::Value::Type::DICTIONARY);
  maps_package.SetKey(AppLaunchEventLogger::kShouldSync, base::Value(true));
  calculator_package.SetKey(AppLaunchEventLogger::kShouldSync,
                            base::Value(true));

  auto packages = std::make_unique<base::Value::Dict>();
  packages->Set(kMapsPackageName, maps_package.Clone());
  packages->Set(kCalculatorPackageName, calculator_package.Clone());

  base::Value maps_app(base::Value::Type::DICTIONARY);
  base::Value calculator_app(base::Value::Type::DICTIONARY);
  maps_app.SetKey(AppLaunchEventLogger::kPackageName,
                  base::Value(kMapsPackageName));
  calculator_app.SetKey(AppLaunchEventLogger::kPackageName,
                        base::Value(kCalculatorPackageName));

  auto arc_apps = std::make_unique<base::Value::Dict>();
  arc_apps->Set(kMapsArcApp, maps_app.Clone());
  arc_apps->Set(kCalculatorArcApp, calculator_app.Clone());

  AppLaunchEventLoggerForTest app_launch_event_logger_(
      nullptr, arc_apps.get(), packages.get(), &profile_);
  // 3 clicks on photos, 2 clicks on calculator, 1 click on maps.
  app_launch_event_logger_.OnGridClicked(app_id);
  app_launch_event_logger_.OnGridClicked(kMapsArcApp);
  app_launch_event_logger_.OnGridClicked(kCalculatorArcApp);
  app_launch_event_logger_.OnSuggestionChipOrSearchBoxClicked(app_id, 3, 2);
  app_launch_event_logger_.OnGridClicked(kCalculatorArcApp);
  app_launch_event_logger_.OnGridClicked(app_id);

  task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(6ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, GURL(kPhotosPwaUrl));
  test_ukm_recorder_.ExpectEntryMetric(entry, "AllClicksLast24Hours", 6);
  test_ukm_recorder_.ExpectEntryMetric(entry, "AllClicksLastHour", 6);
  test_ukm_recorder_.ExpectEntryMetric(entry, "AppType", 3);
  test_ukm_recorder_.ExpectEntryMetric(entry, "LaunchedFrom", 1);
  test_ukm_recorder_.ExpectEntryMetric(entry, "TotalHours", 0);

  const auto click_entries =
      test_ukm_recorder_.GetEntriesByName("AppListAppClickData");
  // Three apps for each of the 6 events.
  ASSERT_EQ(18ul, click_entries.size());
  // Examine the last three events, which are created by the last click.
  base::flat_map<GURL, const ukm::mojom::UkmEntry*> entries_map;
  entries_map[test_ukm_recorder_
                  .GetSourceForSourceId(click_entries.at(15)->source_id)
                  ->url()] = click_entries.at(15);
  entries_map[test_ukm_recorder_
                  .GetSourceForSourceId(click_entries.at(16)->source_id)
                  ->url()] = click_entries.at(16);
  entries_map[test_ukm_recorder_
                  .GetSourceForSourceId(click_entries.at(17)->source_id)
                  ->url()] = click_entries.at(17);

  const GURL kMapsArcUrl("app://play/gbpfhehadcpcndihhameeacbdmbjbhgi");
  const GURL kCalculatorArcUrl("app://play/adeiokjnhlgkiokkojlphcelpojmlkpj");
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entries_map[GURL(kPhotosPwaUrl)],
                                             GURL(kPhotosPwaUrl));
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entries_map[kMapsArcUrl],
                                             kMapsArcUrl);
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entries_map[kCalculatorArcUrl],
                                             kCalculatorArcUrl);

  test_ukm_recorder_.ExpectEntryMetric(entries_map[GURL(kPhotosPwaUrl)],
                                       "AppLaunchId", entry->source_id);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kMapsArcUrl], "AppLaunchId",
                                       entry->source_id);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kCalculatorArcUrl],
                                       "AppLaunchId", entry->source_id);

  test_ukm_recorder_.ExpectEntryMetric(entries_map[GURL(kPhotosPwaUrl)],
                                       "ClicksLast24Hours", 2);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kMapsArcUrl],
                                       "ClicksLast24Hours", 1);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kCalculatorArcUrl],
                                       "ClicksLast24Hours", 2);

  test_ukm_recorder_.ExpectEntryMetric(entries_map[GURL(kPhotosPwaUrl)],
                                       "ClicksLastHour", 2);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kMapsArcUrl],
                                       "ClicksLastHour", 1);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kCalculatorArcUrl],
                                       "ClicksLastHour", 2);

  test_ukm_recorder_.ExpectEntryMetric(entries_map[GURL(kPhotosPwaUrl)],
                                       "MostRecentlyUsedIndex", 1);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kMapsArcUrl],
                                       "MostRecentlyUsedIndex", 3);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kCalculatorArcUrl],
                                       "MostRecentlyUsedIndex", 0);

  test_ukm_recorder_.ExpectEntryMetric(entries_map[GURL(kPhotosPwaUrl)],
                                       "TimeSinceLastClick", 0);

  test_ukm_recorder_.ExpectEntryMetric(entries_map[GURL(kPhotosPwaUrl)],
                                       "TotalClicks", 2);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kMapsArcUrl], "TotalClicks",
                                       1);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kCalculatorArcUrl],
                                       "TotalClicks", 2);

  test_ukm_recorder_.ExpectEntryMetric(entries_map[GURL(kPhotosPwaUrl)],
                                       "LastLaunchedFrom", 2);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kMapsArcUrl],
                                       "LastLaunchedFrom", 1);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kCalculatorArcUrl],
                                       "LastLaunchedFrom", 1);

  test_ukm_recorder_.ExpectEntryMetric(entries_map[GURL(kPhotosPwaUrl)],
                                       "ClickRank", 1);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kMapsArcUrl], "ClickRank",
                                       3);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kCalculatorArcUrl],
                                       "ClickRank", 1);
}

TEST_F(AppLaunchEventLoggerTest, CheckUkmCodeSuggestionChip) {
  web_app::FakeWebAppProvider* provider =
      web_app::FakeWebAppProvider::Get(&profile_);

  std::unique_ptr<web_app::WebApp> web_app =
      web_app::test::CreateWebApp(GURL(kPhotosPwaUrl));
  web_app::AppId app_id = web_app->app_id();
  provider->GetRegistrarMutable().registry().emplace(app_id,
                                                     std::move(web_app));
  provider->Start();

  AppLaunchEventLoggerForTest app_launch_event_logger_(nullptr, nullptr,
                                                       nullptr, &profile_);
  app_launch_event_logger_.OnSuggestionChipOrSearchBoxClicked(app_id, 3, 2);

  task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, GURL(kPhotosPwaUrl));
  test_ukm_recorder_.ExpectEntryMetric(entry, "PositionIndex", 3);
  test_ukm_recorder_.ExpectEntryMetric(entry, "LaunchedFrom", 2);
}

TEST_F(AppLaunchEventLoggerTest, CheckUkmCodeSearchBox) {
  web_app::FakeWebAppProvider* provider =
      web_app::FakeWebAppProvider::Get(&profile_);

  std::unique_ptr<web_app::WebApp> web_app =
      web_app::test::CreateWebApp(GURL(kPhotosPwaUrl));
  web_app::AppId app_id = web_app->app_id();
  provider->GetRegistrarMutable().registry().emplace(app_id,
                                                     std::move(web_app));
  provider->Start();

  AppLaunchEventLoggerForTest app_launch_event_logger_(nullptr, nullptr,
                                                       nullptr, &profile_);
  app_launch_event_logger_.OnSuggestionChipOrSearchBoxClicked(app_id, 3, 4);

  task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, GURL(kPhotosPwaUrl));
  test_ukm_recorder_.ExpectEntryMetric(entry, "PositionIndex", 3);
  test_ukm_recorder_.ExpectEntryMetric(entry, "LaunchedFrom", 4);
}

}  // namespace app_list
