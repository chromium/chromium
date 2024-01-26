// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_service.h"

#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "base/threading/platform_thread.h"
#include "components/country_codes/country_codes.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

class TemplateURLServiceUnitTest : public testing::Test {
 public:
  void SetUp() override {
    TemplateURLService::RegisterProfilePrefs(pref_service_.registry());
    TemplateURLPrepopulateData::RegisterProfilePrefs(pref_service_.registry());
    DefaultSearchManager::RegisterProfilePrefs(pref_service_.registry());

    search_engine_choice_service_ =
        std::make_unique<search_engines::SearchEngineChoiceService>(
            pref_service_);
    // Bypass the country checks.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry,
        country_codes::kCountryCodeUnknown);

    template_url_service_ = std::make_unique<TemplateURLService>(
        &pref_service_, search_engine_choice_service_.get(),
        std::make_unique<SearchTermsData>(),
        nullptr /* KeywordWebDataService */,
        nullptr /* TemplateURLServiceClient */, base::RepeatingClosure()
#if BUILDFLAG(IS_CHROMEOS_LACROS)
                                                    ,
        false
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    );
  }

  TemplateURLService& template_url_service() {
    return *template_url_service_.get();
  }

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<search_engines::SearchEngineChoiceService>
      search_engine_choice_service_;
  std::unique_ptr<TemplateURLService> template_url_service_;
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
