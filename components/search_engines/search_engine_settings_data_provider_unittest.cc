// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_settings_data_provider.h"

#include <memory>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/search_engines/search_engine_split_metrics.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#endif

namespace search_engines {

namespace {

constexpr char kCountOnSettingsPageLoadHistogram[] =
    "Search.OseSplitYahooJapan.CountOnSettingsPageLoad";
constexpr char kDseTypeOnSettingsPageLoadHistogram[] =
    "Search.OseSplitYahooJapan.DseTypeOnSettingsPageLoad";
constexpr char kEngineStateOnSettingsPageLoadHistogram[] =
    "Search.OseSplitYahooJapan.EngineStateOnSettingsPageLoad";

// `keyword_override` allows adding several engines that share a prepopulated
// keyword: `TemplateURLService` would otherwise deduplicate them.
std::unique_ptr<TemplateURL> CreatePrepopulatedEngine(
    const TemplateURLPrepopulateData::PrepopulatedEngine& engine,
    std::u16string_view keyword_override = u"") {
  auto data = TemplateURLDataFromPrepopulatedEngine(engine);
  if (!keyword_override.empty()) {
    data->SetKeyword(keyword_override);
  }
  return std::make_unique<TemplateURL>(*data);
}

MATCHER_P(HasShortName, name, "") {
  return base::UTF16ToASCII(arg->short_name()) == name;
}

}  // namespace

// Base fixture. `country` selects the region: use a search engine split region
// (e.g. "JP") to exercise telemetry, or a non-split one (e.g. "US") to verify
// it stays silent.
class SearchEngineSettingsDataProviderTestBase : public testing::Test {
 public:
  explicit SearchEngineSettingsDataProviderTestBase(std::string_view country) {
    feature_list_.InitAndEnableFeature(
        switches::kApplySearchEngineTypeMigration);
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, country);
  }

  TemplateURLService& template_url_service() {
    return *test_environment_.template_url_service();
  }

  const TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver() {
    return test_environment_.prepopulate_data_resolver();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  std::unique_ptr<SearchEngineSettingsDataProvider> CreateProvider() {
    return template_url_service().CreateSearchEngineSettingsDataProvider();
  }

  TemplateURL* AddTemplateURL(
      const std::u16string_view& short_name,
      const std::u16string_view& keyword,
      int prepopulate_id = 0,
      bool created_by_policy = false,
      TemplateURLData::ActiveStatus active_status =
          TemplateURLData::ActiveStatus::kTrue,
      template_url_starter_pack_data::StarterPackId starter_pack_id =
          template_url_starter_pack_data::StarterPackId::kNone) {
    TemplateURLData data;

    data.SetShortName(short_name);
    data.SetKeyword(keyword);
    data.SetURL("http://google.com/search?q={searchTerms}");
    data.prepopulate_id = prepopulate_id;
    data.policy_origin = created_by_policy
                             ? TemplateURLData::PolicyOrigin::kSiteSearch
                             : TemplateURLData::PolicyOrigin::kNoPolicy;
    data.featured_by_policy = created_by_policy;
    data.is_active = active_status;
    data.starter_pack_id = static_cast<int>(starter_pack_id);
    data.last_visited = base::Time::Now();

    return template_url_service().Add(std::make_unique<TemplateURL>(data));
  }

  TemplateURL* AddExtension(const std::u16string_view& short_name,
                            const std::u16string_view& keyword,
                            TemplateURLData::ActiveStatus active_status) {
    TemplateURLData data;

    data.SetShortName(short_name);
    data.SetKeyword(keyword);
    data.is_active = active_status;

    return template_url_service().Add(std::make_unique<TemplateURL>(
        data, TemplateURL::OMNIBOX_API_EXTENSION, base::UTF16ToASCII(keyword),
        base::Time::Now(), false));
  }

 private:
  base::test::ScopedCommandLine scoped_command_line_;
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  SearchEnginesTestEnvironment test_environment_;
  base::HistogramTester histogram_tester_;
};

// Categorization tests run against the default list country override, which
// provides a deterministic set of prepopulated engines.
class SearchEngineSettingsDataProviderTest
    : public SearchEngineSettingsDataProviderTestBase {
 public:
  SearchEngineSettingsDataProviderTest()
      : SearchEngineSettingsDataProviderTestBase(
            switches::kDefaultListCountryOverride) {}
};

class SearchEngineSettingsDataProviderSplitRegionTest
    : public SearchEngineSettingsDataProviderTestBase {
 public:
  SearchEngineSettingsDataProviderSplitRegionTest()
      : SearchEngineSettingsDataProviderTestBase("JP") {}
};

class SearchEngineSettingsDataProviderNonSplitRegionTest
    : public SearchEngineSettingsDataProviderTestBase {
 public:
  SearchEngineSettingsDataProviderNonSplitRegionTest()
      : SearchEngineSettingsDataProviderTestBase("US") {}
};

// --- Telemetry -------------------------------------------------------------

TEST_F(SearchEngineSettingsDataProviderSplitRegionTest,
       RecordsSettingsPageLoadMetricsOnce) {
  template_url_service().Add(
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::yahoo_jp));

  auto provider = CreateProvider();
  auto displayed = provider->GetCategorizedTemplateURLs().active_site_shortcuts;
  ASSERT_FALSE(displayed.empty());

  provider->MaybeRecordSettingsPageLoadMetrics(displayed);
  histogram_tester().ExpectTotalCount(kCountOnSettingsPageLoadHistogram, 1);
  histogram_tester().ExpectBucketCount(kCountOnSettingsPageLoadHistogram, 1, 1);

  // Callers may call unconditionally on every refresh; only the first call
  // within a settings session records.
  provider->MaybeRecordSettingsPageLoadMetrics(displayed);
  histogram_tester().ExpectTotalCount(kCountOnSettingsPageLoadHistogram, 1);

  // A provider instance corresponds 1:1 with a settings UI controller, and so
  // with a single settings page load. A new instance therefore represents a
  // new page load and records again.
  auto new_provider = CreateProvider();
  new_provider->MaybeRecordSettingsPageLoadMetrics(displayed);
  histogram_tester().ExpectTotalCount(kCountOnSettingsPageLoadHistogram, 2);
}

TEST_F(SearchEngineSettingsDataProviderSplitRegionTest,
       ExtractionDoesNotRecordMetrics) {
  template_url_service().Add(
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::yahoo_jp));

  auto provider = CreateProvider();

  // Data extraction is side-effect free: telemetry is only emitted through the
  // explicit entry point, so that the UI decides what counts as "displayed".
  provider->GetCategorizedTemplateURLs();
  provider->GetCategorizedTemplateURLs();

  histogram_tester().ExpectTotalCount(kCountOnSettingsPageLoadHistogram, 0);
}

TEST_F(SearchEngineSettingsDataProviderNonSplitRegionTest,
       DoesNotRecordMetricsOutsideSplitRegions) {
  template_url_service().Add(
      CreatePrepopulatedEngine(TemplateURLPrepopulateData::yahoo_jp));

  auto provider = CreateProvider();
  provider->MaybeRecordSettingsPageLoadMetrics(
      provider->GetCategorizedTemplateURLs().active_site_shortcuts);

  histogram_tester().ExpectTotalCount(kCountOnSettingsPageLoadHistogram, 0);
}

TEST_F(SearchEngineSettingsDataProviderSplitRegionTest,
       RecordsEngineStateForEachSplitEngine) {
  template_url_service().Add(CreatePrepopulatedEngine(
      TemplateURLPrepopulateData::yahoo_jp, u"yj-legacy"));
  template_url_service().Add(CreatePrepopulatedEngine(
      TemplateURLPrepopulateData::yahoo_jp_next, u"yj-new"));

  auto provider = CreateProvider();
  auto displayed = provider->GetCategorizedTemplateURLs().active_site_shortcuts;
  ASSERT_EQ(displayed.size(), 2u);

  provider->MaybeRecordSettingsPageLoadMetrics(displayed);

  // The default search engine is not a split engine here, so there is no DSE
  // type to report.
  histogram_tester().ExpectTotalCount(kDseTypeOnSettingsPageLoadHistogram, 0);
  histogram_tester().ExpectUniqueSample(kCountOnSettingsPageLoadHistogram, 2,
                                        1);
  // Each displayed split engine is reported individually, on either side of
  // the split.
  histogram_tester().ExpectBucketCount(kEngineStateOnSettingsPageLoadHistogram,
                                       OseSplitEngineState::kLegacyNotDse, 1);
  histogram_tester().ExpectBucketCount(kEngineStateOnSettingsPageLoadHistogram,
                                       OseSplitEngineState::kNewNotDse, 1);
  histogram_tester().ExpectTotalCount(kEngineStateOnSettingsPageLoadHistogram,
                                      2);
}

TEST_F(SearchEngineSettingsDataProviderSplitRegionTest,
       RecordsDseTypeWhenSplitEngineIsDefault) {
  // Until the service is loaded, the default search provider is held as a
  // separate pre-loading copy, which would not be the object the settings page
  // displays.
  template_url_service().Load();

  auto data = TemplateURLDataFromPrepopulatedEngine(
      TemplateURLPrepopulateData::yahoo_jp);
  data->SetKeyword(u"yj-legacy");
  // A user-selected engine is no longer auto-replaceable.
  data->safe_for_autoreplace = false;
  TemplateURL* yahoo_jp =
      template_url_service().Add(std::make_unique<TemplateURL>(*data));
  template_url_service().SetUserSelectedDefaultSearchProvider(yahoo_jp);

  auto provider = CreateProvider();
  auto displayed = provider->GetCategorizedTemplateURLs().active_site_shortcuts;
  ASSERT_THAT(displayed, testing::Contains(yahoo_jp));

  provider->MaybeRecordSettingsPageLoadMetrics(displayed);

  // The region's own prepopulated engines are displayed too, so only assert on
  // what is specific to the default one.
  histogram_tester().ExpectUniqueSample(kDseTypeOnSettingsPageLoadHistogram,
                                        OseSplitType::kLegacy, 1);
  histogram_tester().ExpectBucketCount(
      kEngineStateOnSettingsPageLoadHistogram,
      OseSplitEngineState::kLegacyDseCustomized, 1);
}

// The site shortcut lists are not the whole page: feature shortcuts (starter
// packs and omnibox extensions) are displayed too, so the categorized overload
// has to count every category.
TEST_F(SearchEngineSettingsDataProviderSplitRegionTest,
       RecordsEveryDisplayedCategory) {
  auto add_split_engine = [&](std::u16string_view keyword) {
    return template_url_service().Add(CreatePrepopulatedEngine(
        TemplateURLPrepopulateData::yahoo_jp, keyword));
  };

  CategorizedTemplateUrls displayed;
  displayed.active_site_shortcuts.push_back(add_split_engine(u"yj-1"));
  displayed.inactive_site_shortcuts.push_back(add_split_engine(u"yj-2"));
  displayed.active_feature_shortcuts.push_back(add_split_engine(u"yj-3"));
  displayed.inactive_feature_shortcuts.push_back(add_split_engine(u"yj-4"));

  auto provider = CreateProvider();
  provider->MaybeRecordSettingsPageLoadMetrics(displayed);

  histogram_tester().ExpectUniqueSample(kCountOnSettingsPageLoadHistogram, 4,
                                        1);
  histogram_tester().ExpectTotalCount(kEngineStateOnSettingsPageLoadHistogram,
                                      4);
}

// --- Categorization --------------------------------------------------------

TEST_F(SearchEngineSettingsDataProviderTest, GetCategorizedTemplateURLs_Empty) {
  auto provider = CreateProvider();
  auto data = provider->GetCategorizedTemplateURLs();

  EXPECT_THAT(data.active_site_shortcuts, testing::IsEmpty());
  EXPECT_THAT(data.inactive_site_shortcuts, testing::IsEmpty());
  EXPECT_THAT(data.active_feature_shortcuts, testing::IsEmpty());
  EXPECT_THAT(data.inactive_feature_shortcuts, testing::IsEmpty());
}

TEST_F(SearchEngineSettingsDataProviderTest,
       GetCategorizedTemplateURLs_HiddenSkipped) {
  TemplateURL* custom_url = AddTemplateURL(u"Custom Engine", u"@conflict");
  TemplateURL* policy_url =
      AddTemplateURL(u"Policy Engine", u"@conflict", /*prepopulate_id=*/0,
                     /*created_by_policy=*/true);

  ASSERT_TRUE(template_url_service().HiddenFromLists(custom_url));
  ASSERT_FALSE(template_url_service().HiddenFromLists(policy_url));

  auto provider = CreateProvider();
  auto result = provider->GetCategorizedTemplateURLs();

  EXPECT_THAT(result.active_site_shortcuts,
              testing::ElementsAre(HasShortName("Policy Engine")));
  EXPECT_THAT(result.inactive_site_shortcuts, testing::IsEmpty());
  EXPECT_THAT(result.active_feature_shortcuts, testing::IsEmpty());
  EXPECT_THAT(result.inactive_feature_shortcuts, testing::IsEmpty());
}

TEST_F(SearchEngineSettingsDataProviderTest,
       GetCategorizedTemplateURLs_DisabledStarterPackIdsSkipped) {
  AddTemplateURL(u"AI Search", u"ai", /*prepopulate_id=*/0,
                 /*created_by_policy=*/false,
                 TemplateURLData::ActiveStatus::kTrue,
                 template_url_starter_pack_data::StarterPackId::kAiMode);

  template_url_starter_pack_data::StarterPackIdSet disabled_starter_pack_ids{
      template_url_starter_pack_data::StarterPackId::kAiMode};

  auto provider = CreateProvider();
  auto data_no_ai =
      provider->GetCategorizedTemplateURLs(disabled_starter_pack_ids);
  EXPECT_THAT(data_no_ai.active_feature_shortcuts, testing::IsEmpty());

  auto data_ai = provider->GetCategorizedTemplateURLs();
  EXPECT_THAT(data_ai.active_feature_shortcuts,
              testing::ElementsAre(HasShortName("AI Search")));
}

TEST_F(SearchEngineSettingsDataProviderTest,
       GetCategorizedTemplateURLs_PrepopulatedPrioritizedAndOrdered) {
  AddTemplateURL(u"Custom Search", u"custom");
  AddTemplateURL(u"Custom Beta", u"cbeta");

  auto prepop_engines = prepopulate_data_resolver().GetPrepopulatedEngines();
  ASSERT_EQ(3u, prepop_engines.size());

  for (int i = static_cast<int>(prepop_engines.size()) - 1; i >= 0; --i) {
    AddTemplateURL(prepop_engines.at(i)->short_name(),
                   prepop_engines.at(i)->keyword(),
                   prepop_engines.at(i)->prepopulate_id);
  }

  auto provider = CreateProvider();
  auto data = provider->GetCategorizedTemplateURLs();

  EXPECT_THAT(data.active_site_shortcuts,
              testing::ElementsAre(
                  HasShortName(
                      base::UTF16ToASCII((prepop_engines.at(0)->short_name()))),
                  HasShortName(
                      base::UTF16ToASCII((prepop_engines.at(1)->short_name()))),
                  HasShortName(
                      base::UTF16ToASCII((prepop_engines.at(2)->short_name()))),
                  HasShortName("Custom Beta"), HasShortName("Custom Search")));
}

TEST_F(SearchEngineSettingsDataProviderTest,
       GetCategorizedTemplateURLs_CategorizationLogic) {
  AddTemplateURL(u"Custom Active Site", u"cas");
  AddTemplateURL(u"Prepop Active Site", u"pas", /*prepopulate_id=*/1);
  AddTemplateURL(u"Custom Inactive Site", u"cis", /*prepopulate_id=*/0,
                 /*created_by_policy=*/false,
                 TemplateURLData::ActiveStatus::kFalse);
  AddTemplateURL(u"Lens Active", u"lensa", /*prepopulate_id=*/0,
                 /*created_by_policy=*/false,
                 TemplateURLData::ActiveStatus::kTrue,
                 template_url_starter_pack_data::StarterPackId::kTabs);
  AddExtension(u"Ext Active", u"exa", TemplateURLData::ActiveStatus::kTrue);
  AddTemplateURL(u"Lens Inactive", u"lensi", /*prepopulate_id=*/0,
                 /*created_by_policy=*/false,
                 TemplateURLData::ActiveStatus::kFalse,
                 template_url_starter_pack_data::StarterPackId::kTabs);
  AddExtension(u"Ext Inactive", u"exi", TemplateURLData::ActiveStatus::kFalse);

  auto provider = CreateProvider();
  auto data = provider->GetCategorizedTemplateURLs();

  EXPECT_THAT(
      data.active_site_shortcuts,
      testing::UnorderedElementsAre(HasShortName("Custom Active Site"),
                                    HasShortName("Prepop Active Site")));
  EXPECT_THAT(
      data.inactive_site_shortcuts,
      testing::UnorderedElementsAre(HasShortName("Custom Inactive Site")));
  EXPECT_THAT(data.active_feature_shortcuts,
              testing::UnorderedElementsAre(HasShortName("Lens Active"),
                                            HasShortName("Ext Active")));
  EXPECT_THAT(data.inactive_feature_shortcuts,
              testing::UnorderedElementsAre(HasShortName("Lens Inactive"),
                                            HasShortName("Ext Inactive")));
}

TEST_F(SearchEngineSettingsDataProviderTest,
       GetCategorizedTemplateURLs_Sorting) {
  AddTemplateURL(u"B Unmanaged", u"bu", /*prepopulate_id=*/0,
                 /*created_by_policy=*/false);
  AddTemplateURL(u"A Unmanaged", u"au", /*prepopulate_id=*/0,
                 /*created_by_policy=*/false);
  AddTemplateURL(u"C Managed", u"cu", /*prepopulate_id=*/0,
                 /*created_by_policy=*/true);
  AddTemplateURL(u"D Managed", u"dm", /*prepopulate_id=*/0,
                 /*created_by_policy=*/true);

  AddTemplateURL(u"Y Unmanaged", u"yu", /*prepopulate_id=*/0,
                 /*created_by_policy=*/false,
                 TemplateURLData::ActiveStatus::kFalse);
  AddTemplateURL(u"X Unmanaged", u"xu", /*prepopulate_id=*/0,
                 /*created_by_policy=*/false,
                 TemplateURLData::ActiveStatus::kFalse);
  AddTemplateURL(u"W Managed", u"wm", /*prepopulate_id=*/0,
                 /*created_by_policy=*/true,
                 TemplateURLData::ActiveStatus::kFalse);
  AddTemplateURL(u"V Managed", u"vm", /*prepopulate_id=*/0,
                 /*created_by_policy=*/true,
                 TemplateURLData::ActiveStatus::kFalse);

  auto provider = CreateProvider();
  auto data = provider->GetCategorizedTemplateURLs();

  EXPECT_THAT(data.active_site_shortcuts,
              testing::ElementsAre(
                  HasShortName("C Managed"), HasShortName("D Managed"),
                  HasShortName("A Unmanaged"), HasShortName("B Unmanaged")));

  EXPECT_THAT(data.inactive_site_shortcuts,
              testing::ElementsAre(
                  HasShortName("V Managed"), HasShortName("W Managed"),
                  HasShortName("X Unmanaged"), HasShortName("Y Unmanaged")));
}

TEST_F(SearchEngineSettingsDataProviderTest,
       GetTemplateUrlsByCategory_Filtering) {
  TemplateURL* prepop =
      AddTemplateURL(u"Prepop Engine", u"pe", /*prepopulate_id=*/1);
  template_url_service().SetUserSelectedDefaultSearchProvider(prepop);

  TemplateURL* active_site = AddTemplateURL(
      u"Active Site", u"as", /*prepopulate_id=*/0,
      /*created_by_policy=*/false, TemplateURLData::ActiveStatus::kTrue);
  TemplateURL* inactive_site = AddTemplateURL(
      u"Inactive Site", u"is", /*prepopulate_id=*/0,
      /*created_by_policy=*/false, TemplateURLData::ActiveStatus::kFalse);
  TemplateURL* ext = AddExtension(u"Extension Engine", u"ee",
                                  TemplateURLData::ActiveStatus::kTrue);
  TemplateURL* starter_pack = AddTemplateURL(
      u"Starter Pack", u"sp", /*prepopulate_id=*/0,
      /*created_by_policy=*/false, TemplateURLData::ActiveStatus::kTrue,
      template_url_starter_pack_data::StarterPackId::kBookmarks);

  auto provider = CreateProvider();

  auto default_urls =
      provider->GetTemplateUrlsByCategory(TemplateUrlCategory::kDefault);
  EXPECT_THAT(default_urls, testing::ElementsAre(prepop));

  auto active_urls = provider->GetTemplateUrlsByCategory(
      TemplateUrlCategory::kActiveSiteSearch,
      {template_url_starter_pack_data::StarterPackId::kBookmarks});
  EXPECT_THAT(active_urls, testing::ElementsAre(active_site));
  EXPECT_THAT(provider->GetTemplateUrlsByCategory(
                  TemplateUrlCategory::kActiveSiteSearch),
              testing::UnorderedElementsAre(active_site, starter_pack));

  auto inactive_urls = provider->GetTemplateUrlsByCategory(
      TemplateUrlCategory::kInactiveSiteSearch);
  EXPECT_THAT(inactive_urls, testing::ElementsAre(inactive_site));

  auto ext_urls =
      provider->GetTemplateUrlsByCategory(TemplateUrlCategory::kExtension);
  EXPECT_THAT(ext_urls, testing::ElementsAre(ext));
}

TEST_F(SearchEngineSettingsDataProviderTest,
       GetTemplateUrlsByCategory_Sorting) {
  AddTemplateURL(u"B Unmanaged", u"bu", /*prepopulate_id=*/0,
                 /*created_by_policy=*/false);
  AddTemplateURL(u"A Unmanaged", u"au", /*prepopulate_id=*/0,
                 /*created_by_policy=*/false);
  AddTemplateURL(u"C Managed", u"cu", /*prepopulate_id=*/0,
                 /*created_by_policy=*/true);
  AddTemplateURL(u"D Managed", u"dm", /*prepopulate_id=*/0,
                 /*created_by_policy=*/true);

  auto provider = CreateProvider();
  auto urls = provider->GetTemplateUrlsByCategory(
      TemplateUrlCategory::kActiveSiteSearch);

  EXPECT_THAT(urls, testing::ElementsAre(HasShortName("C Managed"),
                                         HasShortName("D Managed"),
                                         HasShortName("A Unmanaged"),
                                         HasShortName("B Unmanaged")));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(SearchEngineSettingsDataProviderTest,
       GetDisabledStarterPackIdsForAndroid) {
  // Enable features
  {
    omnibox_feature_configs::ScopedConfigForTesting<
        omnibox_feature_configs::ContextualSearch>
        scoped_config;
    scoped_config.Get().starter_pack_page = true;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(omnibox::kStarterPackExpansion);

    auto disabled_ids =
        SearchEngineSettingsDataProvider::GetDisabledStarterPackIdsForAndroid();
    EXPECT_FALSE(
        disabled_ids.Has(template_url_starter_pack_data::StarterPackId::kPage));
    EXPECT_FALSE(disabled_ids.Has(
        template_url_starter_pack_data::StarterPackId::kGemini));

    // @bookmarks is disabled by default.
    EXPECT_TRUE(disabled_ids.Has(
        template_url_starter_pack_data::StarterPackId::kBookmarks));
  }

  // Disable features
  {
    omnibox_feature_configs::ScopedConfigForTesting<
        omnibox_feature_configs::ContextualSearch>
        scoped_config;
    scoped_config.Get().starter_pack_page = false;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(omnibox::kStarterPackExpansion);

    auto disabled_ids =
        SearchEngineSettingsDataProvider::GetDisabledStarterPackIdsForAndroid();
    EXPECT_TRUE(
        disabled_ids.Has(template_url_starter_pack_data::StarterPackId::kPage));
    EXPECT_TRUE(disabled_ids.Has(
        template_url_starter_pack_data::StarterPackId::kGemini));

    // @bookmarks is disabled by default.
    EXPECT_TRUE(disabled_ids.Has(
        template_url_starter_pack_data::StarterPackId::kBookmarks));
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace search_engines
