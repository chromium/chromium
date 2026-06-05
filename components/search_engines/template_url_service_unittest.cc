// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_service.h"

#include <stddef.h>

#include <memory>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_ostream_operators.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/search_engines/template_url_service_test_util.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"
#include "url/origin.h"

namespace {

// Matcher to check TemplateURL by short_name.
MATCHER_P(HasShortName, name, "") {
  return base::UTF16ToASCII(arg->short_name()) == name;
}

}  // namespace

class TemplateURLServiceUnitTest : public TemplateURLServiceUnitTestBase {
 public:
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
};

TEST_F(TemplateURLServiceUnitTest, SessionToken) {
  // Subsequent calls always get the same token.
  std::string token = template_url_service().GetSessionToken();
  std::string token2 = template_url_service().GetSessionToken();
  EXPECT_EQ(token, token2);
  EXPECT_FALSE(token.empty());

  // Calls do not regenerate a token.
  template_url_service().current_token_ = "PRE-EXISTING TOKEN";
  token = template_url_service().GetSessionToken();
  EXPECT_EQ(token, "PRE-EXISTING TOKEN");

  // ... unless the token has expired.
  template_url_service().current_token_.clear();
  const base::TimeDelta kSmallDelta = base::Milliseconds(1);
  template_url_service().token_expiration_time_ =
      base::TimeTicks::Now() - kSmallDelta;
  token = template_url_service().GetSessionToken();
  EXPECT_FALSE(token.empty());
  EXPECT_EQ(token, template_url_service().current_token_);

  // ... or cleared.
  template_url_service().current_token_.clear();
  template_url_service().ClearSessionToken();
  token = template_url_service().GetSessionToken();
  EXPECT_FALSE(token.empty());
  EXPECT_EQ(token, template_url_service().current_token_);

  // The expiration time is always updated.
  template_url_service().GetSessionToken();
  base::TimeTicks expiration_time_1 =
      template_url_service().token_expiration_time_;
  base::PlatformThread::Sleep(kSmallDelta);
  template_url_service().GetSessionToken();
  base::TimeTicks expiration_time_2 =
      template_url_service().token_expiration_time_;
  EXPECT_GT(expiration_time_2, expiration_time_1);
  EXPECT_GE(expiration_time_2, expiration_time_1 + kSmallDelta);
}

TEST_F(TemplateURLServiceUnitTest, UpdateUserSelectedDefaultSearchEnginePref) {
  template_url_service().Load();
  TemplateURLServiceLoadWaiter().WaitForLoadComplete(template_url_service());

  // Add a new search engine.
  TemplateURLData data;
  data.SetShortName(u"custom");
  data.SetKeyword(u"custom");
  data.SetURL("https://custom.com/search?q={searchTerms}");
  data.sync_guid = "custom-guid";

  TemplateURL* turl =
      template_url_service().Add(std::make_unique<TemplateURL>(data));

  // Make it the default search provider.
  template_url_service().SetUserSelectedDefaultSearchProvider(
      turl, search_engines::ChoiceMadeLocation::kOther);

  EXPECT_EQ("https://custom.com/search?q={searchTerms}",
            template_url_service().GetDefaultSearchProvider()->url());

  // Update the search engine URL.
  template_url_service().ResetTemplateURL(
      turl, u"custom2", u"custom2",
      "https://custom2.com/search2?q={searchTerms}");

  // Verify the updated URL in the TemplateURLService.
  EXPECT_EQ("https://custom2.com/search2?q={searchTerms}",
            template_url_service().GetDefaultSearchProvider()->url());

  // Verify the updated URL in the preferences.
  const auto& dict = pref_service().GetDict(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);
  const std::string* pref_url = dict.FindString(DefaultSearchManager::kURL);
  ASSERT_TRUE(pref_url);
  EXPECT_EQ("https://custom2.com/search2?q={searchTerms}", *pref_url);
}

TEST_F(TemplateURLServiceUnitTest, GenerateSearchURL) {
  // Set the default search provider to a custom one.
  TemplateURLData template_url_data;
  template_url_data.SetURL("https://www.example.com/?q={searchTerms}");
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().Add(
          std::make_unique<TemplateURL>(template_url_data)));

  EXPECT_EQ(
      "https://www.example.com/?q=foo",
      template_url_service().GenerateSearchURLForDefaultSearchProvider(u"foo"));
  EXPECT_EQ(
      "https://www.example.com/?q=",
      template_url_service().GenerateSearchURLForDefaultSearchProvider(u""));
}

TEST_F(TemplateURLServiceUnitTest, ExtractSearchMetadata) {
  TemplateURLData template_url_data;
  template_url_data.SetURL("https://www.example.com/?q={searchTerms}");
  template_url_data.search_intent_params = {"gs_ssp", "si"};
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().Add(
          std::make_unique<TemplateURL>(template_url_data)));

  GURL input("https://www.example.com/?q=MyQuery&si=my_si&other_param=foobar");
  auto result = template_url_service().ExtractSearchMetadata(input);
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(result->normalized_url,
            "https://www.example.com/?si=my_si&q=myquery")
      << "q parameter and si parameter should have been preserved. other_param "
         "should be discarded.";
  EXPECT_EQ(result->search_terms, u"myquery");
}

TEST_F(TemplateURLServiceUnitTest, ValidDefaultSearchProviderOrigin) {
  const std::string dse_str = "https://www.example.com";
  url::Origin dse_origin = url::Origin::Create(GURL(dse_str));
  TemplateURLData template_url_data;
  template_url_data.SetURL(dse_str + "/?q={searchTerms}");
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().Add(
          std::make_unique<TemplateURL>(template_url_data)));

  EXPECT_EQ(template_url_service().GetDefaultSearchProviderOrigin(),
            dse_origin);
}

TEST_F(TemplateURLServiceUnitTest, InvalidDefaultSearchProviderOrigin) {
  const std::string dse_str = "https://invalid:test:site";
  url::Origin dse_origin = url::Origin::Create(GURL(dse_str));
  TemplateURLData template_url_data;
  template_url_data.SetURL(dse_str + "/?q={searchTerms}");
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().Add(
          std::make_unique<TemplateURL>(template_url_data)));

  EXPECT_NE(template_url_service().GetDefaultSearchProviderOrigin(),
            dse_origin);
  EXPECT_TRUE(template_url_service().GetDefaultSearchProviderOrigin().opaque());
}

TEST_F(TemplateURLServiceUnitTest, GetFeaturedEnterpriseSiteSearchEngines) {
  TemplateURLData sitesearch_turl_data;
  sitesearch_turl_data.SetKeyword(u"sitesearch");
  sitesearch_turl_data.SetShortName(u"sitesearch");
  sitesearch_turl_data.SetURL("https://www.sitesearch.com?q={searchTerms}");
  sitesearch_turl_data.policy_origin =
      TemplateURLData::PolicyOrigin::kSiteSearch;
  sitesearch_turl_data.enforced_by_policy = true;
  sitesearch_turl_data.featured_by_policy = false;
  sitesearch_turl_data.safe_for_autoreplace = false;
  template_url_service().Add(
      std::make_unique<TemplateURL>(sitesearch_turl_data));
  sitesearch_turl_data.SetKeyword(u"@sitesearch");
  sitesearch_turl_data.featured_by_policy = true;
  template_url_service().Add(
      std::make_unique<TemplateURL>(sitesearch_turl_data));

  TemplateURLData searchaggregator_turl_data;
  searchaggregator_turl_data.SetKeyword(u"searchaggregator");
  searchaggregator_turl_data.SetShortName(u"searchaggregator");
  searchaggregator_turl_data.SetURL(
      "https://www.searchaggregator.com?q={searchTerms}");
  searchaggregator_turl_data.policy_origin =
      TemplateURLData::PolicyOrigin::kSearchAggregator;
  searchaggregator_turl_data.enforced_by_policy = true;
  searchaggregator_turl_data.featured_by_policy = false;
  searchaggregator_turl_data.safe_for_autoreplace = false;
  template_url_service().Add(
      std::make_unique<TemplateURL>(searchaggregator_turl_data));
  searchaggregator_turl_data.SetKeyword(u"@searchaggregator");
  searchaggregator_turl_data.featured_by_policy = true;
  template_url_service().Add(
      std::make_unique<TemplateURL>(searchaggregator_turl_data));

  EXPECT_EQ(static_cast<int>(template_url_service()
                                 .GetFeaturedEnterpriseSiteSearchEngines()
                                 .size()),
            1);
  EXPECT_EQ(template_url_service()
                .GetFeaturedEnterpriseSiteSearchEngines()[0]
                ->keyword(),
            u"@sitesearch");
  EXPECT_EQ(template_url_service()
                .GetFeaturedEnterpriseSiteSearchEngines()[0]
                ->short_name(),
            u"sitesearch");
  EXPECT_EQ(
      template_url_service().GetFeaturedEnterpriseSiteSearchEngines()[0]->url(),
      "https://www.sitesearch.com?q={searchTerms}");
}

TEST_F(TemplateURLServiceUnitTest, HiddenFromLists) {
  auto create_template_url_data =
      [](const std::u16string& keyword,
         TemplateURLData::PolicyOrigin policy_origin,
         bool featured_by_policy) -> TemplateURLData {
    TemplateURLData data;
    data.SetShortName(keyword);
    data.SetKeyword(keyword);
    data.SetURL("https://" + base::UTF16ToUTF8(keyword) + "/?q={searchTerms}");
    data.policy_origin = policy_origin;
    data.featured_by_policy = featured_by_policy;
    return data;
  };

  // Engines with no conflicts. Should NOT be hidden.
  {
    TemplateURL* turl = template_url_service().Add(
        std::make_unique<TemplateURL>(create_template_url_data(
            u"work", TemplateURLData::PolicyOrigin::kNoPolicy,
            /*featured_by_policy=*/false)));
    TemplateURL* turl_default_search_provider = template_url_service().Add(
        std::make_unique<TemplateURL>(create_template_url_data(
            u"default_search_provider",
            TemplateURLData::PolicyOrigin::kDefaultSearchProvider,
            /*featured_by_policy=*/false)));
    TemplateURL* turl_site_search = template_url_service().Add(
        std::make_unique<TemplateURL>(create_template_url_data(
            u"site_search", TemplateURLData::PolicyOrigin::kSiteSearch,
            /*featured_by_policy=*/false)));
    TemplateURL* turl_search_aggregator = template_url_service().Add(
        std::make_unique<TemplateURL>(create_template_url_data(
            u"search_aggregator",
            TemplateURLData::PolicyOrigin::kSearchAggregator,
            /*featured_by_policy=*/false)));
    ASSERT_FALSE(template_url_service().HiddenFromLists(turl));
    ASSERT_FALSE(
        template_url_service().HiddenFromLists(turl_default_search_provider));
    ASSERT_FALSE(template_url_service().HiddenFromLists(turl_site_search));
    ASSERT_FALSE(
        template_url_service().HiddenFromLists(turl_search_aggregator));
  }
  // User-defined engine and a non-featured policy engine exists with the same
  // keyword. User-defined engine should not be hidden. Policy engine should be
  // hidden.
  {
    TemplateURL* turl = template_url_service().Add(
        std::make_unique<TemplateURL>(create_template_url_data(
            u"conflict", TemplateURLData::PolicyOrigin::kNoPolicy,
            /*featured_by_policy=*/false)));
    TemplateURL* turl_policy = template_url_service().Add(
        std::make_unique<TemplateURL>(create_template_url_data(
            u"conflict", TemplateURLData::PolicyOrigin::kSiteSearch,
            /*featured_by_policy=*/false)));
    ASSERT_FALSE(template_url_service().HiddenFromLists(turl));
    ASSERT_TRUE(template_url_service().HiddenFromLists(turl_policy));
  }

  // User-defined engine and a featured policy engine exists with the same
  // keyword. User-defined engine should be hidden. Policy engine should not be
  // hidden.
  {
    TemplateURL* turl = template_url_service().Add(
        std::make_unique<TemplateURL>(create_template_url_data(
            u"@conflict", TemplateURLData::PolicyOrigin::kNoPolicy,
            /*featured_by_policy=*/false)));
    TemplateURL* turl_featured_policy = template_url_service().Add(
        std::make_unique<TemplateURL>(create_template_url_data(
            u"@conflict", TemplateURLData::PolicyOrigin::kSiteSearch,
            /*featured_by_policy=*/true)));
    ASSERT_TRUE(template_url_service().HiddenFromLists(turl));
    ASSERT_FALSE(template_url_service().HiddenFromLists(turl_featured_policy));
  }
}

TEST_F(TemplateURLServiceUnitTest, GetCategorizedTemplateURLs_Empty) {
  TemplateURLService::CategorizedTemplateUrls data =
      template_url_service().GetCategorizedTemplateURLs();

  EXPECT_THAT(data.active_site_shortcuts, testing::IsEmpty());
  EXPECT_THAT(data.inactive_site_shortcuts, testing::IsEmpty());
  EXPECT_THAT(data.active_feature_shortcuts, testing::IsEmpty());
  EXPECT_THAT(data.inactive_feature_shortcuts, testing::IsEmpty());
}

TEST_F(TemplateURLServiceUnitTest, GetCategorizedTemplateURLs_HiddenSkipped) {
  // User-defined engine and a featured policy engine exist with the same
  // keyword. User-defined engine should be hidden. Policy engine should not be
  // hidden.
  TemplateURL* custom_url = AddTemplateURL(u"Custom Engine", u"@conflict");
  TemplateURL* policy_url =
      AddTemplateURL(u"Policy Engine", u"@conflict", /*prepopulate_id=*/0,
                     /*created_by_policy=*/true);

  ASSERT_TRUE(template_url_service().HiddenFromLists(custom_url));
  ASSERT_FALSE(template_url_service().HiddenFromLists(policy_url));

  TemplateURLService::CategorizedTemplateUrls result =
      template_url_service().GetCategorizedTemplateURLs();

  // Only the not hidden TemplateURL should be added to the active site
  // shortcuts.
  EXPECT_THAT(result.active_site_shortcuts,
              testing::ElementsAre(HasShortName("Policy Engine")));
  EXPECT_THAT(result.inactive_site_shortcuts, testing::IsEmpty());
  EXPECT_THAT(result.active_feature_shortcuts, testing::IsEmpty());
  EXPECT_THAT(result.inactive_feature_shortcuts, testing::IsEmpty());
}

TEST_F(TemplateURLServiceUnitTest,
       GetCategorizedTemplateURLs_DisabledStarterPackIdsSkipped) {
  AddTemplateURL(u"AI Search", u"ai", /*prepopulate_id=*/0,
                 /*created_by_policy=*/false,
                 TemplateURLData::ActiveStatus::kTrue,
                 template_url_starter_pack_data::StarterPackId::kAiMode);

  template_url_starter_pack_data::StarterPackIdSet disabled_starter_pack_ids{
      template_url_starter_pack_data::StarterPackId::kAiMode};

  TemplateURLService::CategorizedTemplateUrls data_no_ai =
      template_url_service().GetCategorizedTemplateURLs(
          disabled_starter_pack_ids);
  EXPECT_THAT(data_no_ai.active_feature_shortcuts, testing::IsEmpty());

  TemplateURLService::CategorizedTemplateUrls data_ai =
      template_url_service().GetCategorizedTemplateURLs();
  EXPECT_THAT(data_ai.active_feature_shortcuts,
              testing::ElementsAre(HasShortName("AI Search")));
}

TEST_F(TemplateURLServiceUnitTest,
       GetCategorizedTemplateURLs_PrepopulatedPrioritizedAndOrdered) {
  // Add custom site shortcuts.
  AddTemplateURL(u"Custom Search", u"custom");
  AddTemplateURL(u"Custom Beta", u"cbeta");

  // Add the prepopulated engines in reverse order.
  auto prepop_engines = prepopulate_data_resolver().GetPrepopulatedEngines();
  ASSERT_EQ(3u, prepop_engines.size());

  for (int i = prepop_engines.size() - 1; i >= 0; --i) {
    AddTemplateURL(prepop_engines.at(i)->short_name(),
                   prepop_engines.at(i)->keyword(),
                   prepop_engines.at(i)->prepopulate_id);
  }

  TemplateURLService::CategorizedTemplateUrls data =
      template_url_service().GetCategorizedTemplateURLs();

  // Expected order: First the prepopulated engines in the order of
  // `GetPrepopulatedEngines()`, then alphabetically sorted custom shortcuts.
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

TEST_F(TemplateURLServiceUnitTest,
       GetCategorizedTemplateURLs_CategorizationLogic) {
  // Active Site Shortcuts
  AddTemplateURL(u"Custom Active Site", u"cas");
  AddTemplateURL(u"Prepop Active Site", u"pas",
                 /*prepopulate_id=*/1);

  // Inactive Site Shortcuts
  AddTemplateURL(u"Custom Inactive Site", u"cis", /*prepopulate_id=*/0,
                 /*created_by_policy=*/false,
                 TemplateURLData::ActiveStatus::kFalse);

  // Active Feature Shortcuts
  AddTemplateURL(u"Lens Active", u"lensa", /*prepopulate_id=*/0,
                 /*created_by_policy=*/false,
                 TemplateURLData::ActiveStatus::kTrue,
                 template_url_starter_pack_data::StarterPackId::kTabs);
  AddExtension(u"Ext Active", u"exa", TemplateURLData::ActiveStatus::kTrue);

  // Inactive Feature Shortcuts
  AddTemplateURL(u"Lens Inactive", u"lensi", /*prepopulate_id=*/0,
                 /*created_by_policy=*/false,
                 TemplateURLData::ActiveStatus::kFalse,
                 template_url_starter_pack_data::StarterPackId::kTabs);
  AddExtension(u"Ext Inactive", u"exi", TemplateURLData::ActiveStatus::kFalse);

  TemplateURLService::CategorizedTemplateUrls data =
      template_url_service().GetCategorizedTemplateURLs();

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

TEST_F(TemplateURLServiceUnitTest, GetCategorizedTemplateURLs_Sorting) {
  // Custom Active Site Shortcuts: Sorted by managed (policy) status then
  // alphabetically.
  AddTemplateURL(u"B Unmanaged", u"bu", /*prepopulate_id=*/0,
                 /*created_by_policy=*/false);
  AddTemplateURL(u"A Unmanaged", u"au", /*prepopulate_id=*/0,
                 /*created_by_policy=*/false);
  AddTemplateURL(u"C Managed", u"cu", /*prepopulate_id=*/0,
                 /*created_by_policy=*/true);
  AddTemplateURL(u"D Managed", u"dm", /*prepopulate_id=*/0,
                 /*created_by_policy=*/true);

  // Inactive Site Shortcuts: Sorted by managed (policy) status then
  // alphabetically.
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

  TemplateURLService::CategorizedTemplateUrls data =
      template_url_service().GetCategorizedTemplateURLs();

  // Managed URLs come first, then alphabetical within managed/unmanaged groups.
  EXPECT_THAT(data.active_site_shortcuts,
              testing::ElementsAre(
                  HasShortName("C Managed"), HasShortName("D Managed"),
                  HasShortName("A Unmanaged"), HasShortName("B Unmanaged")));

  EXPECT_THAT(data.inactive_site_shortcuts,
              testing::ElementsAre(
                  HasShortName("V Managed"), HasShortName("W Managed"),
                  HasShortName("X Unmanaged"), HasShortName("Y Unmanaged")));
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
TEST_F(TemplateURLServiceUnitTest,
       GetPrepopulatedAndRecentlyVisitedTemplateURLs_Empty) {
  TemplateURLService::PrepopulatedAndRecentlyVisitedTemplateUrls data =
      template_url_service().GetPrepopulatedAndRecentlyVisitedTemplateURLs();

  EXPECT_THAT(data.prepopulated_urls, testing::IsEmpty());
  EXPECT_THAT(data.recently_visited_urls, testing::IsEmpty());
}

TEST_F(TemplateURLServiceUnitTest,
       GetPrepopulatedAndRecentlyVisitedTemplateURLs_HiddenSkipped) {
  // Fetch a valid prepopulated engine ID from the test's resolver environment.
  auto prepop_engines = prepopulate_data_resolver().GetPrepopulatedEngines();
  ASSERT_FALSE(prepop_engines.empty());
  int valid_prepopulate_id = prepop_engines.at(0)->prepopulate_id;

  // Add a user-defined engine and the real prepopulated engine with matching
  // keywords.
  TemplateURL* custom_url = AddTemplateURL(u"Custom Engine", u"@conflict");
  TemplateURL* prepop_url = AddTemplateURL(u"Prepopulated Engine", u"@conflict",
                                           valid_prepopulate_id);

  ASSERT_TRUE(template_url_service().HiddenFromLists(custom_url));
  ASSERT_FALSE(template_url_service().HiddenFromLists(prepop_url));

  TemplateURLService::PrepopulatedAndRecentlyVisitedTemplateUrls data =
      template_url_service().GetPrepopulatedAndRecentlyVisitedTemplateURLs();

  EXPECT_THAT(data.prepopulated_urls,
              testing::ElementsAre(HasShortName("Prepopulated Engine")));
  EXPECT_THAT(data.recently_visited_urls, testing::IsEmpty());
}

TEST_F(TemplateURLServiceUnitTest,
       GetPrepopulatedAndRecentlyVisitedTemplateURLs_ActiveStatusIgnored) {
  // Add some prepopulated Template URLs.
  AddTemplateURL(u"Active Prepop Engine", u"ape", /*prepopulate_id=*/1);
  AddTemplateURL(u"Inactive Prepop Engine", u"ipe", /*prepopulate_id=*/2,
                 /*created_by_policy=*/false,
                 TemplateURLData::ActiveStatus::kFalse);

  // Add recently Visited Template URLs.
  AddTemplateURL(u"Active Recent Site Search", u"arss");
  AddTemplateURL(u"Inactive Recent Site Search", u"irss", /*prepopulate_id=*/0,
                 /*created_by_policy=*/false,
                 TemplateURLData::ActiveStatus::kFalse);

  TemplateURLService::PrepopulatedAndRecentlyVisitedTemplateUrls data =
      template_url_service().GetPrepopulatedAndRecentlyVisitedTemplateURLs();

  EXPECT_THAT(
      data.prepopulated_urls,
      testing::UnorderedElementsAre(HasShortName("Active Prepop Engine"),
                                    HasShortName("Inactive Prepop Engine")));
  EXPECT_THAT(data.recently_visited_urls,
              testing::UnorderedElementsAre(
                  HasShortName("Active Recent Site Search"),
                  HasShortName("Inactive Recent Site Search")));
}

TEST_F(TemplateURLServiceUnitTest,
       GetPrepopulatedAndRecentlyVisitedTemplateURLs_CategorizationLogic) {
  // Prepopulated Template URL.
  AddTemplateURL(u"Prepop Engine", u"pe", /*prepopulate_id=*/1);

  // Recently Visited Template URL (Not default, not starter pack, not
  // extension).
  AddTemplateURL(u"Recent Site Search", u"rss");

  // Feature items (Starter Pack / Extensions) should not be included.
  AddTemplateURL(u"Lens Starter Pack", u"lens", /*prepopulate_id=*/0,
                 /*created_by_policy=*/false,
                 TemplateURLData::ActiveStatus::kTrue,
                 template_url_starter_pack_data::StarterPackId::kTabs);
  AddExtension(u"Ext Feature", u"ext", TemplateURLData::ActiveStatus::kTrue);

  TemplateURLService::PrepopulatedAndRecentlyVisitedTemplateUrls data =
      template_url_service().GetPrepopulatedAndRecentlyVisitedTemplateURLs();

  EXPECT_THAT(data.prepopulated_urls,
              testing::UnorderedElementsAre(HasShortName("Prepop Engine")));
  EXPECT_THAT(
      data.recently_visited_urls,
      testing::UnorderedElementsAre(HasShortName("Recent Site Search")));
}

TEST_F(TemplateURLServiceUnitTest,
       GetPrepopulatedAndRecentlyVisitedTemplateURLs_PrepopulatedSorting) {
  // Add the prepopulated engines in reverse order.
  auto prepop_engines = prepopulate_data_resolver().GetPrepopulatedEngines();
  ASSERT_EQ(3u, prepop_engines.size());

  for (int i = prepop_engines.size() - 1; i >= 0; --i) {
    AddTemplateURL(prepop_engines.at(i)->short_name(),
                   prepop_engines.at(i)->keyword(),
                   prepop_engines.at(i)->prepopulate_id);
  }

  TemplateURLService::PrepopulatedAndRecentlyVisitedTemplateUrls data =
      template_url_service().GetPrepopulatedAndRecentlyVisitedTemplateURLs();

  // Prepopulated engine list matches
  // `internal::OrderTemplateUrlsByPrepopulatedAndManagedAndAlphabetically`.
  EXPECT_THAT(data.prepopulated_urls,
              testing::ElementsAre(HasShortName(base::UTF16ToASCII(
                                       (prepop_engines.at(0)->short_name()))),
                                   HasShortName(base::UTF16ToASCII(
                                       (prepop_engines.at(1)->short_name()))),
                                   HasShortName(base::UTF16ToASCII(
                                       (prepop_engines.at(2)->short_name())))));
}

TEST_F(
    TemplateURLServiceUnitTest,
    GetPrepopulatedAndRecentlyVisitedTemplateURLs_RecentlyVisitedSortingAndFiltering) {
  // Add some recently visited engines.
  AddTemplateURL(u"Recent Site Search 1", u"rss1");
  AddTemplateURL(u"Recent Site Search 2", u"rss2");
  AddTemplateURL(u"Recent Site Search 3", u"rss3");
  AddTemplateURL(u"Recent Site Search 4", u"rss4");

  TemplateURLService::PrepopulatedAndRecentlyVisitedTemplateUrls data =
      template_url_service().GetPrepopulatedAndRecentlyVisitedTemplateURLs();

  // Recently visited list matches
  // `internal::SortAndFilterRecentlyVisitedURLs()`.
  EXPECT_THAT(
      data.recently_visited_urls,
      testing::UnorderedElementsAre(HasShortName("Recent Site Search 4"),
                                    HasShortName("Recent Site Search 3"),
                                    HasShortName("Recent Site Search 2")));
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

TEST_F(TemplateURLServiceUnitTest,
       GetDefaultSearchProviderIgnoringExtensionsFallbackMatch) {
  // Verify that if the default search provider doesn't strictly match any
  // fields in the search provider database, that we still locate the right
  // entry.  See http://crbug.com/494210871.  This can happen because
  // not all properties are stored in the database, so when a search provider
  // is reloaded, it may fail to match.

  // Add a standard search engine to the service.
  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google.com");
  data.SetURL("https://www.google.com/search?q={searchTerms}");
  data.image_translate_url = "https://www.google.com/image_translate";
  data.sync_guid = "deterministic-guid-123";

  TemplateURL* turl =
      template_url_service().Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(turl);

  // Set it as the user-selected default.
  template_url_service().SetUserSelectedDefaultSearchProvider(turl);

  // Confirm strict match works initially.
  EXPECT_EQ(
      turl,
      template_url_service().GetDefaultSearchProviderIgnoringExtensions());

  // Simulate metadata drift in the "fresh" data from DefaultSearchManager.
  // We'll create data that has the SAME GUID but DIFFERENT image_translate_url.
  TemplateURLData drifted_data = data;
  drifted_data.image_translate_url =
      "https://www.google.com/new_image_translate";

  // Bypass TemplateURLService and inject this into DefaultSearchManager
  // so that GetDefaultSearchProviderIgnoringExtensions() sees the "drifted"
  // version.
  template_url_service().ApplyDefaultSearchChangeForTesting(
      &drifted_data, DefaultSearchManager::FROM_USER);

  // Verify that matching still finds the original TemplateURL via
  // GUID even though TemplateURL::MatchesData would now fail due to
  // image_translate_url mismatch.
  const TemplateURL* matched_turl =
      template_url_service().GetDefaultSearchProviderIgnoringExtensions();
  EXPECT_EQ(turl, matched_turl);
}

#if BUILDFLAG(IS_ANDROID)

class TemplateURLServiceWithDatabaseUnitTest
    : public LoadedTemplateURLServiceUnitTestBase {
 protected:
  const std::u16string kOldPlayEngineKeyword = u"old_keyword";
  const std::u16string kNewPlayEngineKeyword = u"new_keyword";

  TemplateURLData CreatePlayAPITemplateURLData(const std::u16string& keyword) {
    return TemplateURLService::CreatePlayAPITemplateURLData(
        keyword, keyword,
        base::StringPrintf("https://%s.com/q={searchTerms}",
                           base::UTF16ToUTF8(keyword).c_str()));
  }

  // Uses the "legacy" API way of registering a search engine coming from Play.
  TemplateURL* AddPlayApiEngineLegacy(const std::u16string& keyword,
                                      bool set_as_default) {
    // Create a custom search engine declared as coming from Play.
    TemplateURL* template_url = template_url_service().Add(
        std::make_unique<TemplateURL>(CreatePlayAPITemplateURLData(keyword)));

    CHECK(template_url);
    CHECK(template_url->GetRegulatoryExtensionType() ==
          RegulatoryExtensionType::kAndroidEEA);
    CHECK_EQ(template_url,
             template_url_service().GetTemplateURLForKeyword(keyword));

    if (set_as_default) {
      template_url_service().SetUserSelectedDefaultSearchProvider(template_url);
      CHECK_EQ(template_url_service().GetDefaultSearchProvider()->keyword(),
               keyword);
    } else {
      CHECK_EQ(template_url_service().GetDefaultSearchProvider()->keyword(),
               TemplateURLPrepopulateData::google.keyword);
    }

    return template_url;
  }
};

TEST_F(TemplateURLServiceWithDatabaseUnitTest, ResetPlayAPISearchEngine) {
  const size_t initial_turl_count =
      template_url_service().GetTemplateURLs().size();

  // Create a custom search engine declared as coming from Play.
  AddPlayApiEngineLegacy(kOldPlayEngineKeyword,
                         /*set_as_default=*/false);

  // The (old) play engine is added.
  EXPECT_EQ(template_url_service().GetTemplateURLs().size(),
            initial_turl_count + 1);

  const auto new_play_engine_data =
      CreatePlayAPITemplateURLData(kNewPlayEngineKeyword);
  EXPECT_TRUE(
      template_url_service().ResetPlayAPISearchEngine(new_play_engine_data));

  // The old DSE is not known anymore.
  EXPECT_FALSE(
      template_url_service().GetTemplateURLForKeyword(kOldPlayEngineKeyword));

  // The new engine is added, flagged as coming from Play, and set as default.
  auto* new_play_engine =
      template_url_service().GetTemplateURLForKeyword(kNewPlayEngineKeyword);
  EXPECT_TRUE(new_play_engine);
  ASSERT_EQ(new_play_engine->GetRegulatoryExtensionType(),
            RegulatoryExtensionType::kAndroidEEA);
  EXPECT_EQ(new_play_engine, template_url_service().GetDefaultSearchProvider());

  // We still have the same number of engines.
  EXPECT_EQ(template_url_service().GetTemplateURLs().size(),
            initial_turl_count + 1);
}

TEST_F(TemplateURLServiceWithDatabaseUnitTest,
       ResetPlayAPISearchEngine_OldIsDefault) {
  AddPlayApiEngineLegacy(kOldPlayEngineKeyword, /*set_as_default=*/true);

  const auto new_play_engine_data =
      CreatePlayAPITemplateURLData(kNewPlayEngineKeyword);
  EXPECT_TRUE(
      template_url_service().ResetPlayAPISearchEngine(new_play_engine_data));

  // The old DSE is not known anymore.
  EXPECT_FALSE(
      template_url_service().GetTemplateURLForKeyword(kOldPlayEngineKeyword));

  // The new engine is added, flagged as coming from Play, and set as default.
  auto* new_play_engine =
      template_url_service().GetTemplateURLForKeyword(kNewPlayEngineKeyword);
  EXPECT_TRUE(new_play_engine);
  ASSERT_EQ(new_play_engine->GetRegulatoryExtensionType(),
            RegulatoryExtensionType::kAndroidEEA);
  EXPECT_EQ(new_play_engine, template_url_service().GetDefaultSearchProvider());
}

TEST_F(TemplateURLServiceWithDatabaseUnitTest,
       ResetPlayAPISearchEngine_NewIsPrepopulated) {
  const std::u16string overriden_keyword =
      TemplateURLPrepopulateData::bing.keyword;
  TemplateURL* old_prepopulated_engine =
      template_url_service().GetTemplateURLForKeyword(overriden_keyword);
  ASSERT_TRUE(old_prepopulated_engine);

  const std::string new_engine_url_from_play =
      "https://new.chromium?q={searchTerms}";
  const std::u16string new_engine_name_from_play = u"New Engine From Play";
  const auto new_engine_data = TemplateURLService::CreatePlayAPITemplateURLData(
      overriden_keyword, new_engine_name_from_play, new_engine_url_from_play);
  EXPECT_TRUE(template_url_service().ResetPlayAPISearchEngine(new_engine_data));

  // The new engine is added, flagged as coming from Play, and set as default.
  auto* new_play_engine =
      template_url_service().GetTemplateURLForKeyword(overriden_keyword);
  EXPECT_TRUE(new_play_engine);
  ASSERT_EQ(new_play_engine->GetRegulatoryExtensionType(),
            RegulatoryExtensionType::kAndroidEEA);
  EXPECT_EQ(new_play_engine->prepopulate_id(), /* bing_id */ 3);
  EXPECT_EQ(new_play_engine, template_url_service().GetDefaultSearchProvider());

  // The properties are the ones coming from Chromium database: reconciliation
  // detects matching defiition and re-uses it.
  EXPECT_EQ(new_play_engine->url(), old_prepopulated_engine->url());
  EXPECT_EQ(new_play_engine->short_name(), new_engine_name_from_play);

  // Both the old prepopulated engine and the new one from play are registered.
  ASSERT_THAT(
      GetTemplateURLsMatchingKeyword(overriden_keyword),
      testing::UnorderedElementsAre(old_prepopulated_engine, new_play_engine));
}

TEST_F(TemplateURLServiceWithDatabaseUnitTest,
       ResetPlayAPISearchEngine_OldIsPrepopulated) {
  const std::u16string overriden_keyword =
      TemplateURLPrepopulateData::bing.keyword;

  // Asserting the legacy, baseline behaviour.

  // `overriden_keyword` is expected to be a preloaded engine.
  TemplateURL* old_prepopulated_engine =
      template_url_service().GetTemplateURLForKeyword(overriden_keyword);
  ASSERT_TRUE(old_prepopulated_engine);

  // When adding the old Play engine, both engines are still registered, but the
  // one from Play is now returned when looking up by keyword.
  TemplateURL* old_play_engine =
      AddPlayApiEngineLegacy(overriden_keyword,
                             /*set_as_default=*/true);
  ASSERT_THAT(
      GetTemplateURLsMatchingKeyword(overriden_keyword),
      testing::UnorderedElementsAre(old_prepopulated_engine, old_play_engine));
  ASSERT_EQ(old_play_engine,
            template_url_service().GetTemplateURLForKeyword(overriden_keyword));

  // Checking the behaviour of the new function

  const auto new_play_engine_data =
      CreatePlayAPITemplateURLData(kNewPlayEngineKeyword);
  EXPECT_TRUE(
      template_url_service().ResetPlayAPISearchEngine(new_play_engine_data));

  // The new engine is added, flagged as coming from Play, and set as default.
  auto* new_play_engine =
      template_url_service().GetTemplateURLForKeyword(kNewPlayEngineKeyword);
  EXPECT_TRUE(new_play_engine);
  ASSERT_EQ(new_play_engine->GetRegulatoryExtensionType(),
            RegulatoryExtensionType::kAndroidEEA);
  EXPECT_EQ(new_play_engine, template_url_service().GetDefaultSearchProvider());

  // The old prepopulated engine is still there and is exposed when looking up
  // the keyword.
  EXPECT_THAT(GetTemplateURLsMatchingKeyword(overriden_keyword),
              testing::UnorderedElementsAre(old_prepopulated_engine));
  EXPECT_EQ(old_prepopulated_engine,
            template_url_service().GetTemplateURLForKeyword(overriden_keyword));
}

TEST_F(TemplateURLServiceWithDatabaseUnitTest,
       ResetPlayAPISearchEngine_NewHasSameKeywordAsOld) {
  const std::u16string play_engine_keyword = u"play_keyword";
  AddPlayApiEngineLegacy(play_engine_keyword, /*set_as_default=*/false);

  const std::string new_engine_url_from_play =
      "https://play.chromium?q={searchTerms}&is_new=true";
  const std::u16string new_engine_name_from_play = u"Play Engine New";
  const auto new_play_engine_data =
      TemplateURLService::CreatePlayAPITemplateURLData(
          play_engine_keyword, new_engine_name_from_play,
          new_engine_url_from_play);

  EXPECT_TRUE(
      template_url_service().ResetPlayAPISearchEngine(new_play_engine_data));

  // The new engine is added, flagged as coming from Play, and set as default.
  auto* new_play_engine =
      template_url_service().GetTemplateURLForKeyword(play_engine_keyword);
  EXPECT_TRUE(new_play_engine);
  ASSERT_EQ(new_play_engine->GetRegulatoryExtensionType(),
            RegulatoryExtensionType::kAndroidEEA);
  EXPECT_EQ(new_play_engine, template_url_service().GetDefaultSearchProvider());

  // This is the only known engine matching this keyword.
  EXPECT_THAT(GetTemplateURLsMatchingKeyword(play_engine_keyword),
              testing::Contains(new_play_engine));

  // The properties are the ones coming from play, not the old ones.
  EXPECT_EQ(new_play_engine->url(), new_engine_url_from_play);
  EXPECT_EQ(new_play_engine->short_name(), new_engine_name_from_play);
}

TEST_F(TemplateURLServiceWithDatabaseUnitTest,
       ResetPlayAPISearchEngine_DseSetByPolicy) {
  TemplateURLData policy_engine_data;
  policy_engine_data.SetShortName(u"Chromium.org Search");
  policy_engine_data.SetKeyword(u"cr_search");
  policy_engine_data.SetURL("https://search.chromium.org?q={searchTerms}");

  template_url_service().ApplyDefaultSearchChangeForTesting(
      &policy_engine_data, DefaultSearchManager::FROM_POLICY);

  const TemplateURL* policy_engine =
      template_url_service().GetDefaultSearchProvider();
  ASSERT_EQ(policy_engine->keyword(), policy_engine_data.keyword());
  ASSERT_TRUE(policy_engine->CreatedByDefaultSearchProviderPolicy());

  const auto new_play_engine_data =
      CreatePlayAPITemplateURLData(kNewPlayEngineKeyword);

  EXPECT_TRUE(
      template_url_service().ResetPlayAPISearchEngine(new_play_engine_data));
  // The new engine is added, flagged as coming from Play, but not set as
  // default.
  auto* new_play_engine =
      template_url_service().GetTemplateURLForKeyword(kNewPlayEngineKeyword);
  EXPECT_TRUE(new_play_engine);
  ASSERT_EQ(new_play_engine->GetRegulatoryExtensionType(),
            RegulatoryExtensionType::kAndroidEEA);

  EXPECT_NE(new_play_engine, template_url_service().GetDefaultSearchProvider());

  // The policy engine is still the default one.
  EXPECT_EQ(policy_engine, template_url_service().GetDefaultSearchProvider());
}

TEST_F(TemplateURLServiceWithDatabaseUnitTest,
       ResetPlayAPISearchEngine_NewHasSameKeywordAsPolicyEngine) {
  TemplateURLData policy_engine_data;
  policy_engine_data.SetShortName(u"Chromium.org Search");
  policy_engine_data.SetKeyword(u"cr_search");
  policy_engine_data.SetURL("https://search.chromium.org?q={searchTerms}");

  template_url_service().ApplyDefaultSearchChangeForTesting(
      &policy_engine_data, DefaultSearchManager::FROM_POLICY);

  const TemplateURL* policy_engine =
      template_url_service().GetDefaultSearchProvider();
  ASSERT_EQ(policy_engine->keyword(), policy_engine_data.keyword());
  ASSERT_TRUE(policy_engine->CreatedByDefaultSearchProviderPolicy());

  // Add the Play API engine using the same keyword as the policy engine.
  const auto new_play_engine_data =
      TemplateURLService::CreatePlayAPITemplateURLData(
          policy_engine_data.keyword(), u"title2",
          "https://example2.com/q={searchTerms}");

  // The operation should fail.
  EXPECT_FALSE(
      template_url_service().ResetPlayAPISearchEngine(new_play_engine_data));

  // Only the policy engine is registered with the keyword, and it's still the
  // DSE.
  EXPECT_THAT(GetTemplateURLsMatchingKeyword(policy_engine_data.keyword()),
              testing::UnorderedElementsAre(policy_engine));
  EXPECT_EQ(policy_engine, template_url_service().GetDefaultSearchProvider());
}

#endif
