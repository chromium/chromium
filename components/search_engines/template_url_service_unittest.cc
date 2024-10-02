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
#include "base/strings/utf_string_conversions.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/search_engines/template_url_service_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

class TemplateURLServiceUnitTest : public TemplateURLServiceUnitTestBase {};

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
    CHECK(template_url->created_from_play_api());
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
  EXPECT_TRUE(new_play_engine->created_from_play_api());
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
  EXPECT_TRUE(new_play_engine->created_from_play_api());
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
  EXPECT_TRUE(new_play_engine->created_from_play_api());
  EXPECT_EQ(new_play_engine->prepopulate_id(), 0);
  EXPECT_EQ(new_play_engine, template_url_service().GetDefaultSearchProvider());

  // The properties are the ones coming from play, not the prepopulated ones.
  EXPECT_EQ(new_play_engine->url(), new_engine_url_from_play);
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
  EXPECT_TRUE(new_play_engine->created_from_play_api());
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
  EXPECT_TRUE(new_play_engine->created_from_play_api());
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
  ASSERT_EQ(policy_engine->created_by_policy(),
            TemplateURLData::CreatedByPolicy::kDefaultSearchProvider);

  const auto new_play_engine_data =
      CreatePlayAPITemplateURLData(kNewPlayEngineKeyword);

  EXPECT_TRUE(
      template_url_service().ResetPlayAPISearchEngine(new_play_engine_data));
  // The new engine is added, flagged as coming from Play, but not set as
  // default.
  auto* new_play_engine =
      template_url_service().GetTemplateURLForKeyword(kNewPlayEngineKeyword);
  EXPECT_TRUE(new_play_engine);
  EXPECT_TRUE(new_play_engine->created_from_play_api());

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
  ASSERT_EQ(policy_engine->created_by_policy(),
            TemplateURLData::CreatedByPolicy::kDefaultSearchProvider);

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
