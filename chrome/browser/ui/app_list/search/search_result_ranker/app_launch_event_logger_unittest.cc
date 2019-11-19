// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_event_logger.h"

#include <memory>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/ukm/app_source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace app_list {

const char kGmailChromeApp[] = "pjkljhegncpnkpknbcohdijeoejaedia";
const char kMapsArcApp[] = "gmhipfhgnoelkiiofcnimehjnpaejiel";
const char kCalculatorArcApp[] = "adeiokjnhlgkiokkojlphcelpojmlkpj";
const char kPhotosPWAApp[] = "ncmjhecbjeaamljdfahankockkkdmedg";

const GURL kPhotosPWAUrl = GURL("http://photos.google.com/");
const GURL kMapsArcUrl = GURL("app://play/gbpfhehadcpcndihhameeacbdmbjbhgi");
const GURL kCalculatorArcUrl =
    GURL("app://play/adeiokjnhlgkiokkojlphcelpojmlkpj");

const char kMapsPackageName[] = "com.google.android.apps.maps";
const char kCalculatorPackageName[] = "com.google.android.calculator";

namespace {

bool TestIsWebstoreExtension(base::StringPiece id) {
  return (id == kGmailChromeApp);
}

}  // namespace

class AppLaunchEventLoggerForTest : public AppLaunchEventLogger {
 public:
  AppLaunchEventLoggerForTest(extensions::ExtensionRegistry* registry,
                              base::DictionaryValue* arc_apps,
                              base::DictionaryValue* arc_packages) {
    arc_apps_ = arc_apps;
    arc_packages_ = arc_packages;
    registry_ = registry;
    // EnforceLoggingPolicy runs in the base constructor without the test data,
    // so run it again here after the test data is set.
    EnforceLoggingPolicy();
  }

 protected:
  const GURL& GetLaunchWebURL(const extensions::Extension* extension) override {
    return kPhotosPWAUrl;
  }
};

class AppLaunchEventLoggerTest : public testing::Test {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(ukm::kUkmAppLogging);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

TEST_F(AppLaunchEventLoggerTest, CheckUkmCodePWA) {
  extensions::ExtensionRegistry registry(nullptr);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test")
          .SetID(kPhotosPWAApp)
          .AddFlags(extensions::Extension::FROM_BOOKMARK)
          .Build();

  registry.AddEnabled(extension);

  AppLaunchEventLoggerForTest app_launch_event_logger_(&registry, nullptr,
                                                       nullptr);
  app_launch_event_logger_.OnGridClicked(kPhotosPWAApp);

  task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, kPhotosPWAUrl);
  test_ukm_recorder_.ExpectEntryMetric(entry, "AllClicksLast24Hours", 1);
  test_ukm_recorder_.ExpectEntryMetric(entry, "AllClicksLastHour", 1);
  test_ukm_recorder_.ExpectEntryMetric(entry, "AppType", 3);
  test_ukm_recorder_.ExpectEntryMetric(entry, "LaunchedFrom", 1);
  test_ukm_recorder_.ExpectEntryMetric(entry, "TotalHours", 0);

  const auto click_entries =
      test_ukm_recorder_.GetEntriesByName("AppListAppClickData");
  ASSERT_EQ(1ul, click_entries.size());
  const auto* photos_entry = click_entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(photos_entry, kPhotosPWAUrl);
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

  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);
  test_ukm_recorder_.SetIsWebstoreExtensionCallback(
      base::BindRepeating(&TestIsWebstoreExtension));

  AppLaunchEventLoggerForTest app_launch_event_logger_(&registry, nullptr,
                                                       nullptr);
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

  auto packages = std::make_unique<base::DictionaryValue>();
  packages->SetKey(kMapsPackageName, package.Clone());

  base::Value app(base::Value::Type::DICTIONARY);
  app.SetKey(AppLaunchEventLogger::kPackageName, base::Value(kMapsPackageName));

  auto arc_apps = std::make_unique<base::DictionaryValue>();
  arc_apps->SetKey(kMapsArcApp, app.Clone());

  AppLaunchEventLoggerForTest app_launch_event_logger_(nullptr, arc_apps.get(),
                                                       packages.get());
  app_launch_event_logger_.OnGridClicked(kMapsArcApp);

  task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, kMapsArcUrl);
  test_ukm_recorder_.ExpectEntryMetric(entry, "AppType", 2);
  test_ukm_recorder_.ExpectEntryMetric(entry, "LaunchedFrom", 1);
}

TEST_F(AppLaunchEventLoggerTest, CheckMultipleClicks) {
  extensions::ExtensionRegistry registry(nullptr);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test")
          .SetID(kPhotosPWAApp)
          .AddFlags(extensions::Extension::FROM_BOOKMARK)
          .Build();
  registry.AddEnabled(extension);

  base::Value maps_package(base::Value::Type::DICTIONARY);
  base::Value calculator_package(base::Value::Type::DICTIONARY);
  maps_package.SetKey(AppLaunchEventLogger::kShouldSync, base::Value(true));
  calculator_package.SetKey(AppLaunchEventLogger::kShouldSync,
                            base::Value(true));

  auto packages = std::make_unique<base::DictionaryValue>();
  packages->SetKey(kMapsPackageName, maps_package.Clone());
  packages->SetKey(kCalculatorPackageName, calculator_package.Clone());

  base::Value maps_app(base::Value::Type::DICTIONARY);
  base::Value calculator_app(base::Value::Type::DICTIONARY);
  maps_app.SetKey(AppLaunchEventLogger::kPackageName,
                  base::Value(kMapsPackageName));
  calculator_app.SetKey(AppLaunchEventLogger::kPackageName,
                        base::Value(kCalculatorPackageName));

  auto arc_apps = std::make_unique<base::DictionaryValue>();
  arc_apps->SetKey(kMapsArcApp, maps_app.Clone());
  arc_apps->SetKey(kCalculatorArcApp, calculator_app.Clone());

  AppLaunchEventLoggerForTest app_launch_event_logger_(
      &registry, arc_apps.get(), packages.get());
  // 3 clicks on photos, 2 clicks on calculator, 1 click on maps.
  app_launch_event_logger_.OnGridClicked(kPhotosPWAApp);
  app_launch_event_logger_.OnGridClicked(kMapsArcApp);
  app_launch_event_logger_.OnGridClicked(kCalculatorArcApp);
  app_launch_event_logger_.OnSuggestionChipOrSearchBoxClicked(kPhotosPWAApp, 3,
                                                              2);
  app_launch_event_logger_.OnGridClicked(kCalculatorArcApp);
  app_launch_event_logger_.OnGridClicked(kPhotosPWAApp);

  task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(6ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, kPhotosPWAUrl);
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

  test_ukm_recorder_.ExpectEntrySourceHasUrl(entries_map[kPhotosPWAUrl],
                                             kPhotosPWAUrl);
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entries_map[kMapsArcUrl],
                                             kMapsArcUrl);
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entries_map[kCalculatorArcUrl],
                                             kCalculatorArcUrl);

  test_ukm_recorder_.ExpectEntryMetric(entries_map[kPhotosPWAUrl],
                                       "AppLaunchId", entry->source_id);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kMapsArcUrl], "AppLaunchId",
                                       entry->source_id);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kCalculatorArcUrl],
                                       "AppLaunchId", entry->source_id);

  test_ukm_recorder_.ExpectEntryMetric(entries_map[kPhotosPWAUrl],
                                       "ClicksLast24Hours", 2);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kMapsArcUrl],
                                       "ClicksLast24Hours", 1);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kCalculatorArcUrl],
                                       "ClicksLast24Hours", 2);

  test_ukm_recorder_.ExpectEntryMetric(entries_map[kPhotosPWAUrl],
                                       "ClicksLastHour", 2);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kMapsArcUrl],
                                       "ClicksLastHour", 1);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kCalculatorArcUrl],
                                       "ClicksLastHour", 2);

  test_ukm_recorder_.ExpectEntryMetric(entries_map[kPhotosPWAUrl],
                                       "MostRecentlyUsedIndex", 1);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kMapsArcUrl],
                                       "MostRecentlyUsedIndex", 3);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kCalculatorArcUrl],
                                       "MostRecentlyUsedIndex", 0);

  test_ukm_recorder_.ExpectEntryMetric(entries_map[kPhotosPWAUrl],
                                       "TimeSinceLastClick", 0);

  test_ukm_recorder_.ExpectEntryMetric(entries_map[kPhotosPWAUrl],
                                       "TotalClicks", 2);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kMapsArcUrl], "TotalClicks",
                                       1);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kCalculatorArcUrl],
                                       "TotalClicks", 2);

  test_ukm_recorder_.ExpectEntryMetric(entries_map[kPhotosPWAUrl],
                                       "LastLaunchedFrom", 2);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kMapsArcUrl],
                                       "LastLaunchedFrom", 1);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kCalculatorArcUrl],
                                       "LastLaunchedFrom", 1);

  test_ukm_recorder_.ExpectEntryMetric(entries_map[kPhotosPWAUrl], "ClickRank",
                                       1);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kMapsArcUrl], "ClickRank",
                                       3);
  test_ukm_recorder_.ExpectEntryMetric(entries_map[kCalculatorArcUrl],
                                       "ClickRank", 1);
}

TEST_F(AppLaunchEventLoggerTest, CheckUkmCodeSuggestionChip) {
  extensions::ExtensionRegistry registry(nullptr);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test")
          .SetID(kPhotosPWAApp)
          .AddFlags(extensions::Extension::FROM_BOOKMARK)
          .Build();
  registry.AddEnabled(extension);

  AppLaunchEventLoggerForTest app_launch_event_logger_(&registry, nullptr,
                                                       nullptr);
  app_launch_event_logger_.OnSuggestionChipOrSearchBoxClicked(kPhotosPWAApp, 3,
                                                              2);

  task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, kPhotosPWAUrl);
  test_ukm_recorder_.ExpectEntryMetric(entry, "PositionIndex", 3);
  test_ukm_recorder_.ExpectEntryMetric(entry, "LaunchedFrom", 2);
}

TEST_F(AppLaunchEventLoggerTest, CheckUkmCodeSearchBox) {
  extensions::ExtensionRegistry registry(nullptr);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test")
          .SetID(kPhotosPWAApp)
          .AddFlags(extensions::Extension::FROM_BOOKMARK)
          .Build();
  registry.AddEnabled(extension);

  AppLaunchEventLoggerForTest app_launch_event_logger_(&registry, nullptr,
                                                       nullptr);
  app_launch_event_logger_.OnSuggestionChipOrSearchBoxClicked(kPhotosPWAApp, 3,
                                                              4);

  task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, kPhotosPWAUrl);
  test_ukm_recorder_.ExpectEntryMetric(entry, "PositionIndex", 3);
  test_ukm_recorder_.ExpectEntryMetric(entry, "LaunchedFrom", 4);
}

}  // namespace app_list
