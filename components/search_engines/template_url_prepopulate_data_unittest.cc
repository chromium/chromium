// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_prepopulate_data.h"

#include <stddef.h>

#include <memory>
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/to_vector.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/country_codes/country_codes.h"
#include "components/google/core/common/google_switches.h"
#include "components/regional_capabilities/regional_capabilities_country_id.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_test_utils.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data_resolver.h"
#include "components/search_engines/testing_search_terms_data.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"
#include "third_party/search_engines_data/resources/definitions/regional_settings.h"

using ::base::ASCIIToUTF16;
using ::country_codes::CountryId;
using ::TemplateURLPrepopulateData::BuiltinKeywordsMetadata;
using ::TemplateURLPrepopulateData::kCurrentDataVersion;

namespace TemplateURLPrepopulateData {
bool operator==(const BuiltinKeywordsMetadata& lhs,
                const BuiltinKeywordsMetadata& rhs) {
  return lhs.data_version == rhs.data_version &&
         lhs.country_id == rhs.country_id;
}

std::ostream& operator<<(std::ostream& os,
                         const BuiltinKeywordsMetadata& value) {
  return os << "{country_id=" << value.country_id.GetForTesting().Serialize()
            << ", data_version=" << value.data_version << "}";
}

}  // namespace TemplateURLPrepopulateData

namespace {

SearchEngineType GetEngineType(const std::string& url) {
  TemplateURLData data;
  data.SetURL(url);
  return TemplateURL(data).GetEngineType(SearchTermsData());
}

std::string GetHostFromTemplateURLData(const TemplateURLData& data) {
  return TemplateURL(data).url_ref().GetHost(SearchTermsData());
}

const CountryId kAllCountryIds[] = {
    CountryId("AD"), CountryId("AE"), CountryId("AF"), CountryId("AG"),
    CountryId("AI"), CountryId("AL"), CountryId("AM"), CountryId("AN"),
    CountryId("AO"), CountryId("AQ"), CountryId("AR"), CountryId("AS"),
    CountryId("AT"), CountryId("AU"), CountryId("AW"), CountryId("AX"),
    CountryId("AZ"), CountryId("BA"), CountryId("BB"), CountryId("BD"),
    CountryId("BE"), CountryId("BF"), CountryId("BG"), CountryId("BH"),
    CountryId("BI"), CountryId("BJ"), CountryId("BL"), CountryId("BM"),
    CountryId("BN"), CountryId("BO"), CountryId("BR"), CountryId("BS"),
    CountryId("BT"), CountryId("BV"), CountryId("BW"), CountryId("BY"),
    CountryId("BZ"), CountryId("CA"), CountryId("CC"), CountryId("CD"),
    CountryId("CF"), CountryId("CG"), CountryId("CH"), CountryId("CI"),
    CountryId("CK"), CountryId("CL"), CountryId("CM"), CountryId("CN"),
    CountryId("CO"), CountryId("CQ"), CountryId("CR"), CountryId("CU"),
    CountryId("CV"), CountryId("CX"), CountryId("CY"), CountryId("CZ"),
    CountryId("DE"), CountryId("DJ"), CountryId("DK"), CountryId("DM"),
    CountryId("DO"), CountryId("DZ"), CountryId("EA"), CountryId("EC"),
    CountryId("EE"), CountryId("EG"), CountryId("EH"), CountryId("ER"),
    CountryId("ES"), CountryId("ET"), CountryId("FI"), CountryId("FJ"),
    CountryId("FK"), CountryId("FM"), CountryId("FO"), CountryId("FR"),
    CountryId("GA"), CountryId("GB"), CountryId("GD"), CountryId("GE"),
    CountryId("GF"), CountryId("GG"), CountryId("GH"), CountryId("GI"),
    CountryId("GL"), CountryId("GM"), CountryId("GN"), CountryId("GP"),
    CountryId("GQ"), CountryId("GR"), CountryId("GS"), CountryId("GT"),
    CountryId("GU"), CountryId("GW"), CountryId("GY"), CountryId("HK"),
    CountryId("HM"), CountryId("HN"), CountryId("HR"), CountryId("HT"),
    CountryId("HU"), CountryId("IC"), CountryId("ID"), CountryId("IE"),
    CountryId("IL"), CountryId("IM"), CountryId("IN"), CountryId("IO"),
    CountryId("IQ"), CountryId("IR"), CountryId("IS"), CountryId("IT"),
    CountryId("JE"), CountryId("JM"), CountryId("JO"), CountryId("JP"),
    CountryId("KE"), CountryId("KG"), CountryId("KH"), CountryId("KI"),
    CountryId("KM"), CountryId("KN"), CountryId("KP"), CountryId("KR"),
    CountryId("KW"), CountryId("KY"), CountryId("KZ"), CountryId("LA"),
    CountryId("LB"), CountryId("LC"), CountryId("LI"), CountryId("LK"),
    CountryId("LR"), CountryId("LS"), CountryId("LT"), CountryId("LU"),
    CountryId("LV"), CountryId("LY"), CountryId("MA"), CountryId("MC"),
    CountryId("MD"), CountryId("ME"), CountryId("MF"), CountryId("MG"),
    CountryId("MH"), CountryId("MK"), CountryId("ML"), CountryId("MM"),
    CountryId("MN"), CountryId("MO"), CountryId("MP"), CountryId("MQ"),
    CountryId("MR"), CountryId("MS"), CountryId("MT"), CountryId("MU"),
    CountryId("MV"), CountryId("MW"), CountryId("MX"), CountryId("MY"),
    CountryId("MZ"), CountryId("NA"), CountryId("NC"), CountryId("NE"),
    CountryId("NF"), CountryId("NG"), CountryId("NI"), CountryId("NL"),
    CountryId("NO"), CountryId("NP"), CountryId("NR"), CountryId("NU"),
    CountryId("NZ"), CountryId("OM"), CountryId("PA"), CountryId("PE"),
    CountryId("PF"), CountryId("PG"), CountryId("PH"), CountryId("PK"),
    CountryId("PL"), CountryId("PM"), CountryId("PN"), CountryId("PR"),
    CountryId("PS"), CountryId("PT"), CountryId("PW"), CountryId("PY"),
    CountryId("QA"), CountryId("RE"), CountryId("RO"), CountryId("RS"),
    CountryId("RU"), CountryId("RW"), CountryId("SA"), CountryId("SB"),
    CountryId("SC"), CountryId("SD"), CountryId("SE"), CountryId("SG"),
    CountryId("SH"), CountryId("SI"), CountryId("SJ"), CountryId("SK"),
    CountryId("SL"), CountryId("SM"), CountryId("SN"), CountryId("SO"),
    CountryId("SR"), CountryId("ST"), CountryId("SV"), CountryId("SY"),
    CountryId("SZ"), CountryId("TC"), CountryId("TD"), CountryId("TF"),
    CountryId("TG"), CountryId("TH"), CountryId("TJ"), CountryId("TK"),
    CountryId("TL"), CountryId("TM"), CountryId("TN"), CountryId("TO"),
    CountryId("TR"), CountryId("TT"), CountryId("TV"), CountryId("TW"),
    CountryId("TZ"), CountryId("UA"), CountryId("UG"), CountryId("UM"),
    CountryId("US"), CountryId("UY"), CountryId("UZ"), CountryId("VA"),
    CountryId("VC"), CountryId("VE"), CountryId("VG"), CountryId("VI"),
    CountryId("VN"), CountryId("VU"), CountryId("WF"), CountryId("WS"),
    CountryId("YE"), CountryId("YT"), CountryId("ZA"), CountryId("ZM"),
    CountryId("ZW"), CountryId("ZZ")};

void CheckUrlIsEmptyOrSecure(const std::string url) {
  ASSERT_TRUE(url.empty() || url.starts_with("{google:") ||
              url.starts_with(url::kHttpsScheme));
}

void CheckTemplateUrlRefIsCryptographic(const TemplateURLRef& url_ref) {
  TestingSearchTermsData search_terms_data("https://www.google.com/");
  if (!url_ref.IsValid(search_terms_data)) {
    ADD_FAILURE() << url_ref.GetURL();
    return;
  }

  // Double parentheses around the string16 constructor to prevent the compiler
  // from parsing it as a function declaration.
  TemplateURLRef::SearchTermsArgs search_term_args((std::u16string()));
  GURL url(url_ref.ReplaceSearchTerms(search_term_args, search_terms_data));
  EXPECT_TRUE(url.is_empty() || url.SchemeIsCryptographic()) << url;
}

}  // namespace

class TemplateURLPrepopulateDataTest : public testing::Test {
 public:
  TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver() {
    return search_engines_test_environment_.prepopulate_data_resolver();
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return &search_engines_test_environment_.pref_service();
  }

  void SetupForChoiceScreenDisplay() {
    // Pick any EEA country
    const CountryId kFranceCountryId("FR");
    OverrideCountryId(kFranceCountryId);
  }

  void OverrideCountryId(CountryId country_id) {
    OverrideCountryCommandLine(country_id.CountryCode());
  }

  void OverrideCountryCommandLine(std::string_view country_string) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kSearchEngineChoiceCountry)) {
      base::CommandLine::ForCurrentProcess()->RemoveSwitch(
          switches::kSearchEngineChoiceCountry);
    }

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, country_string);
  }

 protected:
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
};

// Verifies the set of prepopulate data doesn't contain entries with duplicate
// ids.
TEST_F(TemplateURLPrepopulateDataTest, UniqueIDs) {
  for (CountryId country_id : kAllCountryIds) {
    OverrideCountryId(country_id);
    std::vector<std::unique_ptr<TemplateURLData>> urls =
        prepopulate_data_resolver().GetPrepopulatedEngines();
    std::set<int> unique_ids;
    for (const std::unique_ptr<TemplateURLData>& url : urls) {
      ASSERT_TRUE(unique_ids.find(url->prepopulate_id) == unique_ids.end());
      unique_ids.insert(url->prepopulate_id);
    }
  }
}

// Verifies that the prepopulated search engines configured by country are
// consistent with the set of countries in EeaChoiceCountry. For example, the
// per region limits `kMaxEeaPrepopulatedEngines` and
// `kMaxRowPrepopulatedEngines` should apply as expected.
TEST_F(TemplateURLPrepopulateDataTest, NumberOfEntriesPerCountryConsistency) {
  const size_t kMinEea = 8;
  const size_t kMinRow = 3;

  for (CountryId country_id : kAllCountryIds) {
    OverrideCountryId(country_id);

    const size_t kNumberOfSearchEngines =
        prepopulate_data_resolver().GetPrepopulatedEngines().size();

    if (regional_capabilities::IsEeaCountry(country_id)) {
      EXPECT_GE(kNumberOfSearchEngines, kMinEea)
          << " for country " << country_id.CountryCode();
      EXPECT_LE(kNumberOfSearchEngines,
                TemplateURLPrepopulateData::kMaxEeaPrepopulatedEngines)
          << " for country " << country_id.CountryCode();
    } else {
      EXPECT_GE(kNumberOfSearchEngines, kMinRow)
          << " for country " << country_id.CountryCode();
      EXPECT_LE(kNumberOfSearchEngines,
                TemplateURLPrepopulateData::kMaxRowPrepopulatedEngines)
          << " for country " << country_id.CountryCode();
    }
  }
}

TEST_F(TemplateURLPrepopulateDataTest, EntriesPerCountryConsistency) {
  for (CountryId country_id : kAllCountryIds) {
    if (!regional_capabilities::IsEeaCountry(country_id)) {
      // "unhandled" countries can cause some issues when inheriting a config
      // from an EEA country. Covering them via
      // TemplateURLPrepopulateDataTest.NumberOfEntriesPerCountryConsistency is
      // enough, so they we exclude non-EEA countries in the rest of this test
      // for simplicity.
      continue;
    }

    OverrideCountryId(country_id);

    // Obtained by calling the normal API to fetch engines for the current
    // country.
    std::vector<std::string> actual_urls =
        base::ToVector(prepopulate_data_resolver().GetPrepopulatedEngines(),
                       [](const auto& t_url) { return t_url->url(); });

    // Pulled straight from the country -> engine mapping.
    auto expected_urls = base::ToVector(
        TemplateURLPrepopulateData::kRegionalSettings.find(country_id)
            ->second->search_engines,
        &TemplateURLPrepopulateData::PrepopulatedEngine::search_url);

    EXPECT_THAT(actual_urls, testing::UnorderedElementsAreArray(expected_urls));
  }
}

// Verifies that the order of the randomly shuffled search engines stays
// constant per-profile.
TEST_F(TemplateURLPrepopulateDataTest,
       SearchEnginesOrderDoesNotChangePerProfile) {
  SetupForChoiceScreenDisplay();

  // Fetch the list of search engines twice and make sure the order stays the
  // same.
  std::vector<std::unique_ptr<TemplateURLData>> t_urls_1 =
      prepopulate_data_resolver().GetPrepopulatedEngines();
  std::vector<std::unique_ptr<TemplateURLData>> t_urls_2 =
      prepopulate_data_resolver().GetPrepopulatedEngines();

  ASSERT_EQ(t_urls_1.size(), t_urls_2.size());
  for (size_t i = 0; i < t_urls_1.size(); i++) {
    // Each prepopulated engine has a unique prepopulate_id, so we simply
    // compare those.
    ASSERT_EQ(t_urls_1[i]->prepopulate_id, t_urls_2[i]->prepopulate_id);
  }
}

// Verifies that default search providers from the preferences file
// override the built-in ones.
TEST_F(TemplateURLPrepopulateDataTest, ProvidersFromPrefs) {
  pref_service()->SetUserPref(prefs::kSearchProviderOverridesVersion,
                              std::make_unique<base::Value>(1));
  base::Value::List overrides;

  // Set only the minimal required settings for a search provider configuration.
  base::Value::Dict entry =
      base::Value::Dict()
          .Set("name", "foo")
          .Set("keyword", "fook")
          .Set("search_url", "http://foo.com/s?q={searchTerms}")
          .Set("favicon_url", "http://foi.com/favicon.ico")
          .Set("encoding", "UTF-8")
          .Set("id", 1001);
  overrides.Append(entry.Clone());
  pref_service()->SetUserPref(prefs::kSearchProviderOverrides,
                              std::move(overrides));

  int version = TemplateURLPrepopulateData::GetDataVersion(pref_service());
  EXPECT_EQ(1, version);

  std::vector<std::unique_ptr<TemplateURLData>> t_urls =
      prepopulate_data_resolver().GetPrepopulatedEngines();

  ASSERT_EQ(1u, t_urls.size());
  EXPECT_EQ(u"foo", t_urls[0]->short_name());
  EXPECT_EQ(u"fook", t_urls[0]->keyword());
  EXPECT_EQ("foo.com", GetHostFromTemplateURLData(*t_urls[0]));
  EXPECT_EQ("foi.com", t_urls[0]->favicon_url.host());
  EXPECT_EQ(1u, t_urls[0]->input_encodings.size());
  EXPECT_EQ(1001, t_urls[0]->prepopulate_id);
  EXPECT_TRUE(t_urls[0]->suggestions_url.empty());
  EXPECT_EQ(0u, t_urls[0]->alternate_urls.size());
  EXPECT_TRUE(t_urls[0]->safe_for_autoreplace);
  EXPECT_TRUE(t_urls[0]->date_created.is_null());
  EXPECT_TRUE(t_urls[0]->last_modified.is_null());

  // Test the optional settings too.
  entry.Set("suggest_url", "http://foo.com/suggest?q={searchTerms}");
  entry.Set("alternate_urls", base::Value::List().Append(
                                  "http://foo.com/alternate?q={searchTerms}"));

  overrides = base::Value::List().Append(entry.Clone());
  pref_service()->SetUserPref(prefs::kSearchProviderOverrides,
                              std::move(overrides));

  t_urls = prepopulate_data_resolver().GetPrepopulatedEngines();
  ASSERT_EQ(1u, t_urls.size());
  EXPECT_EQ(u"foo", t_urls[0]->short_name());
  EXPECT_EQ(u"fook", t_urls[0]->keyword());
  EXPECT_EQ("foo.com", GetHostFromTemplateURLData(*t_urls[0]));
  EXPECT_EQ("foi.com", t_urls[0]->favicon_url.host());
  EXPECT_EQ(1u, t_urls[0]->input_encodings.size());
  EXPECT_EQ(1001, t_urls[0]->prepopulate_id);
  EXPECT_EQ("http://foo.com/suggest?q={searchTerms}",
            t_urls[0]->suggestions_url);
  ASSERT_EQ(1u, t_urls[0]->alternate_urls.size());
  EXPECT_EQ("http://foo.com/alternate?q={searchTerms}",
            t_urls[0]->alternate_urls[0]);

  // Test that subsequent providers are loaded even if an intermediate
  // provider has an incomplete configuration.
  overrides = base::Value::List().Append(entry.Clone());
  entry.Set("id", 1002);
  entry.Set("name", "bar");
  entry.Set("keyword", "bark");
  entry.Set("encoding", std::string());
  overrides.Append(entry.Clone());
  entry.Set("id", 1003);
  entry.Set("name", "baz");
  entry.Set("keyword", "bazk");
  entry.Set("encoding", "UTF-8");
  overrides.Append(entry.Clone());
  pref_service()->SetUserPref(prefs::kSearchProviderOverrides,
                              std::move(overrides));

  t_urls = prepopulate_data_resolver().GetPrepopulatedEngines();
  EXPECT_EQ(2u, t_urls.size());
}

TEST_F(TemplateURLPrepopulateDataTest, ClearProvidersFromPrefs) {
  OverrideCountryId(CountryId());
  pref_service()->SetUserPref(prefs::kSearchProviderOverridesVersion,
                              std::make_unique<base::Value>(1));

  // Set only the minimal required settings for a search provider configuration.
  base::Value::Dict entry =
      base::Value::Dict()
          .Set("name", "foo")
          .Set("keyword", "fook")
          .Set("search_url", "http://foo.com/s?q={searchTerms}")
          .Set("favicon_url", "http://foi.com/favicon.ico")
          .Set("encoding", "UTF-8")
          .Set("id", 1001);
  base::Value::List overrides = base::Value::List().Append(std::move(entry));
  pref_service()->SetUserPref(prefs::kSearchProviderOverrides,
                              std::move(overrides));

  int version = TemplateURLPrepopulateData::GetDataVersion(pref_service());
  EXPECT_EQ(1, version);

  // This call removes the above search engine.
  TemplateURLPrepopulateData::ClearPrepopulatedEnginesInPrefs(pref_service());

  version = TemplateURLPrepopulateData::GetDataVersion(pref_service());
  EXPECT_EQ(TemplateURLPrepopulateData::kCurrentDataVersion, version);

  std::vector<std::unique_ptr<TemplateURLData>> t_urls =
      prepopulate_data_resolver().GetPrepopulatedEngines();
  ASSERT_FALSE(t_urls.empty());
  for (size_t i = 0; i < t_urls.size(); ++i) {
    EXPECT_NE(u"foo", t_urls[i]->short_name());
    EXPECT_NE(u"fook", t_urls[i]->keyword());
    EXPECT_NE("foi.com", t_urls[i]->favicon_url.host());
    EXPECT_NE("foo.com", GetHostFromTemplateURLData(*t_urls[i]));
    EXPECT_NE(1001, t_urls[i]->prepopulate_id);
  }

  // Ensures the fallback URL is Google and has the optional fields filled.
  std::unique_ptr<TemplateURLData> fallback_t_url =
      prepopulate_data_resolver().GetFallbackSearch();
  EXPECT_EQ(TemplateURLPrepopulateData::google.name,
            fallback_t_url->short_name());
  EXPECT_FALSE(fallback_t_url->suggestions_url.empty());
  EXPECT_FALSE(fallback_t_url->image_url.empty());
  EXPECT_FALSE(fallback_t_url->contextual_search_url.empty());
  EXPECT_FALSE(fallback_t_url->image_url_post_params.empty());
  EXPECT_EQ(TemplateURLPrepopulateData::google.type,
            TemplateURL(*fallback_t_url).GetEngineType(SearchTermsData()));
}

// Verifies that built-in search providers are processed correctly.
TEST_F(TemplateURLPrepopulateDataTest, ProvidersFromPrepopulated) {
  // Use United States.
  OverrideCountryId(CountryId("US"));
  std::vector<std::unique_ptr<TemplateURLData>> t_urls =
      prepopulate_data_resolver().GetPrepopulatedEngines();

  // Ensure all the URLs have the required fields populated.
  ASSERT_FALSE(t_urls.empty());
  for (size_t i = 0; i < t_urls.size(); ++i) {
    ASSERT_FALSE(t_urls[i]->short_name().empty());
    ASSERT_FALSE(t_urls[i]->keyword().empty());
    ASSERT_FALSE(t_urls[i]->favicon_url.host().empty());
    ASSERT_FALSE(GetHostFromTemplateURLData(*t_urls[i]).empty());
    ASSERT_FALSE(t_urls[i]->input_encodings.empty());
    EXPECT_GT(t_urls[i]->prepopulate_id, 0);
    EXPECT_TRUE(t_urls[0]->safe_for_autoreplace);
    EXPECT_TRUE(t_urls[0]->date_created.is_null());
    EXPECT_TRUE(t_urls[0]->last_modified.is_null());
  }

  // Ensures the fallback URL is Google and has the optional fields filled.
  std::unique_ptr<TemplateURLData> fallback_t_url =
      prepopulate_data_resolver().GetFallbackSearch();
  EXPECT_EQ(TemplateURLPrepopulateData::google.name,
            fallback_t_url->short_name());
  EXPECT_FALSE(fallback_t_url->suggestions_url.empty());
  EXPECT_FALSE(fallback_t_url->image_url.empty());
  EXPECT_FALSE(fallback_t_url->contextual_search_url.empty());
  EXPECT_FALSE(fallback_t_url->image_url_post_params.empty());
  // Expect at least 2 alternate_urls.
  // This caught a bug with static initialization of arrays, so leave this in.
  EXPECT_GT(fallback_t_url->alternate_urls.size(), 1u);
  for (size_t i = 0; i < fallback_t_url->alternate_urls.size(); ++i) {
    EXPECT_FALSE(fallback_t_url->alternate_urls[i].empty());
  }
  EXPECT_EQ(TemplateURLPrepopulateData::google.type,
            TemplateURL(*fallback_t_url).GetEngineType(SearchTermsData()));
}

// Verifies that all built-in search providers available across all countries
// use https urls.
TEST_F(TemplateURLPrepopulateDataTest, PrepopulatedAreHttps) {
  for (CountryId country_id : kAllCountryIds) {
    OverrideCountryId(country_id);

    std::vector<std::unique_ptr<TemplateURLData>> t_urls =
        prepopulate_data_resolver().GetPrepopulatedEngines();

    ASSERT_FALSE(t_urls.empty());
    for (const auto& t_url : t_urls) {
      CheckUrlIsEmptyOrSecure(t_url->url());
      CheckUrlIsEmptyOrSecure(t_url->image_url);
      CheckUrlIsEmptyOrSecure(t_url->image_translate_url);
      CheckUrlIsEmptyOrSecure(t_url->new_tab_url);
      CheckUrlIsEmptyOrSecure(t_url->contextual_search_url);
      CheckUrlIsEmptyOrSecure(t_url->suggestions_url);
      CheckUrlIsEmptyOrSecure(t_url->favicon_url.scheme());
      CheckUrlIsEmptyOrSecure(t_url->logo_url.scheme());
    }
  }
}

TEST_F(TemplateURLPrepopulateDataTest, GetEngineTypeBasic) {
  EXPECT_EQ(SEARCH_ENGINE_OTHER, GetEngineType("http://example.com/"));
  EXPECT_EQ(SEARCH_ENGINE_BING, GetEngineType("http://www.bing.com/"));
  EXPECT_EQ(SEARCH_ENGINE_OTHER, GetEngineType("http://search.atlas.cz/"));
  EXPECT_EQ(TemplateURLPrepopulateData::google.type,
            GetEngineType("http://www.google.com/"));
}

TEST_F(TemplateURLPrepopulateDataTest, GetEngineTypeAdvanced) {
  // Google URLs in different forms.
  const char* kGoogleURLs[] = {
    // Original with google:baseURL:
    "{google:baseURL}search?q={searchTerms}&{google:RLZ}"
    "{google:originalQueryForSuggestion}{google:searchFieldtrialParameter}"
    "sourceid=chrome&ie={inputEncoding}",
    // Custom with google.com and reordered query params:
    "http://google.com/search?{google:RLZ}{google:originalQueryForSuggestion}"
    "{google:searchFieldtrialParameter}"
    "sourceid=chrome&ie={inputEncoding}&q={searchTerms}",
    // Custom with a country TLD and almost no query params:
    "http://www.google.ru/search?q={searchTerms}"
  };
  for (const char* google_url : kGoogleURLs) {
    EXPECT_EQ(TemplateURLPrepopulateData::google.type,
              GetEngineType(google_url));
  }

  // Non-Google URLs.
  const char* kYahooURLs[] = {
      ("http://search.yahoo.com/search?"
       "ei={inputEncoding}&fr=crmas&p={searchTerms}"),
      "http://search.yahoo.com/search?p={searchTerms}",
      // Aggressively match types by checking just TLD+1.
      "http://someothersite.yahoo.com/",
  };
  for (const char* yahoo_url : kYahooURLs) {
    EXPECT_EQ(SEARCH_ENGINE_YAHOO, GetEngineType(yahoo_url));
  }

  // Search URL for which no prepopulated search provider exists.
  EXPECT_EQ(SEARCH_ENGINE_OTHER,
            GetEngineType("http://example.net/search?q={searchTerms}"));
  EXPECT_EQ(SEARCH_ENGINE_OTHER, GetEngineType("invalid:search:url"));

  // URL that doesn't look Google-related, but matches a Google base URL
  // specified on the command line.
  const std::string foo_url("http://www.foo.com/search?q={searchTerms}");
  EXPECT_EQ(SEARCH_ENGINE_OTHER, GetEngineType(foo_url));
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kGoogleBaseURL, "http://www.foo.com/");
  EXPECT_EQ(TemplateURLPrepopulateData::google.type, GetEngineType(foo_url));
}

TEST_F(TemplateURLPrepopulateDataTest, GetEngineTypeForAllPrepopulatedEngines) {
  using PrepopulatedEngine = TemplateURLPrepopulateData::PrepopulatedEngine;
  const auto all_engines =
      TemplateURLPrepopulateData::GetAllPrepopulatedEngines();
  for (const PrepopulatedEngine* engine : all_engines) {
    std::unique_ptr<TemplateURLData> data =
        TemplateURLDataFromPrepopulatedEngine(*engine);
    EXPECT_EQ(engine->type,
              TemplateURL(*data).GetEngineType(SearchTermsData()));
  }
}

TEST_F(TemplateURLPrepopulateDataTest, CheckSearchURLDetection) {
  using PrepopulatedEngine = TemplateURLPrepopulateData::PrepopulatedEngine;
  const auto all_engines =
      TemplateURLPrepopulateData::GetAllPrepopulatedEngines();
  for (const PrepopulatedEngine* engine : all_engines) {
    std::unique_ptr<TemplateURLData> data =
        TemplateURLDataFromPrepopulatedEngine(*engine);
    TemplateURL t_url(*data);
    SearchTermsData search_data;
    // Test that search term is successfully extracted from generated search
    // url.
    GURL search_url = t_url.GenerateSearchURL(search_data);
    EXPECT_TRUE(t_url.IsSearchURL(search_url, search_data))
        << "Search url is incorrectly detected for " << search_url;
  }
}

TEST_F(TemplateURLPrepopulateDataTest, HttpsUrls) {
  // Search engines that don't use HTTPS URLs.
  // Since Chrome and the Internet are trying to transition from HTTP to HTTPS,
  // please get approval from a PM before entering new HTTP exceptions here.
  std::set<int> exceptions{
      4,  6,  16, 17, 21, 27, 35, 36, 43, 44, 45, 50, 54, 55, 56, 60, 61,
      62, 63, 64, 65, 66, 68, 70, 74, 75, 76, 77, 78, 79, 80, 81, 85, 90,
  };
  using PrepopulatedEngine = TemplateURLPrepopulateData::PrepopulatedEngine;
  const auto all_engines =
      TemplateURLPrepopulateData::GetAllPrepopulatedEngines();
  for (const PrepopulatedEngine* engine : all_engines) {
    std::unique_ptr<TemplateURLData> data =
        TemplateURLDataFromPrepopulatedEngine(*engine);
    if (base::Contains(exceptions, data->prepopulate_id))
      continue;

    GURL logo_url = data->logo_url;
    EXPECT_TRUE(logo_url.is_empty() || logo_url.SchemeIsCryptographic())
        << logo_url;
    GURL doodle_url = data->doodle_url;
    EXPECT_TRUE(doodle_url.is_empty() || doodle_url.SchemeIsCryptographic())
        << doodle_url;
    EXPECT_TRUE(logo_url.is_empty() || doodle_url.is_empty())
        << "Only one of logo_url or doodle_url should be set.";

    GURL favicon_url = data->favicon_url;
    EXPECT_TRUE(favicon_url.is_empty() || favicon_url.SchemeIsCryptographic())
        << favicon_url;

    TemplateURL template_url(*data);

    // Intentionally don't check alternate URLs, because those are only used
    // for matching.
    CheckTemplateUrlRefIsCryptographic(template_url.url_ref());
    CheckTemplateUrlRefIsCryptographic(template_url.suggestions_url_ref());
    CheckTemplateUrlRefIsCryptographic(template_url.image_url_ref());
    CheckTemplateUrlRefIsCryptographic(template_url.new_tab_url_ref());
    CheckTemplateUrlRefIsCryptographic(
        template_url.contextual_search_url_ref());
  }
}

TEST_F(TemplateURLPrepopulateDataTest, FindGoogleAsFallback) {
  std::unique_ptr<TemplateURLData> fallback_url;

  // Google is first in US, so confirm index 0.
  CountryId us_country_id("US");
  OverrideCountryId(us_country_id);
  EXPECT_EQ(TemplateURLPrepopulateData::kRegionalSettings.find(us_country_id)
                ->second->search_engines[0]
                ->id,
            TemplateURLPrepopulateData::google.id);

  fallback_url = prepopulate_data_resolver().GetFallbackSearch();
  EXPECT_EQ(fallback_url->prepopulate_id,
            TemplateURLPrepopulateData::google.id);

  // Google is not first in CN; confirm it is found at index > 0.
  // If Google ever does reach top in China, this test will need to be adjusted:
  // check template_url_prepopulate_data.cc reference orders (engines_CN, etc.)
  // to find a suitable country and index.
  CountryId cn_country_id("CN");
  OverrideCountryId(cn_country_id);
  fallback_url = prepopulate_data_resolver().GetFallbackSearch();
  EXPECT_NE(TemplateURLPrepopulateData::kRegionalSettings.find(cn_country_id)
                ->second->search_engines[0]
                ->id,
            TemplateURLPrepopulateData::google.id);
  EXPECT_TRUE(fallback_url);
  EXPECT_EQ(fallback_url->prepopulate_id,
            TemplateURLPrepopulateData::google.id);
}

// Regression test for https://crbug.com/1500526.
TEST_F(TemplateURLPrepopulateDataTest, GetPrepopulatedEngineFromFullList) {
  // Ensure that we use the default set of search engines, which is google,
  // bing, yahoo.
  OverrideCountryId(CountryId());
  ASSERT_EQ(prepopulate_data_resolver().GetPrepopulatedEngines().size(), 3u);

  // `GetPrepopulatedEngine()` only looks in the profile country's prepopulated
  // list.
  EXPECT_FALSE(prepopulate_data_resolver().GetPrepopulatedEngine(
      TemplateURLPrepopulateData::ecosia.id));

  // Here we look in the full list.
  auto found_engine = prepopulate_data_resolver().GetEngineFromFullList(
      TemplateURLPrepopulateData::ecosia.id);
  EXPECT_TRUE(found_engine);
  auto expected_engine =
      TemplateURLDataFromPrepopulatedEngine(TemplateURLPrepopulateData::ecosia);
  ExpectSimilar(expected_engine.get(), found_engine.get());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(TemplateURLPrepopulateDataTest, GetLocalPrepopulatedEngines) {
  constexpr char sample_country[] = "US";
  OverrideCountryId(CountryId(sample_country));

  // For a given country, the output from `GetLocalPrepopulatedEngines`
  // should match the template URLs obtained from `GetPrepopulatedEngines`.
  auto expected_urls = prepopulate_data_resolver().GetPrepopulatedEngines();
  auto actual_urls = TemplateURLPrepopulateData::GetLocalPrepopulatedEngines(
      sample_country, *pref_service());

  ASSERT_EQ(actual_urls.size(), expected_urls.size());
  for (unsigned int i = 0; i < actual_urls.size(); ++i) {
    EXPECT_EQ(actual_urls[i]->prepopulate_id, expected_urls[i]->prepopulate_id);
    EXPECT_EQ(actual_urls[i]->keyword(), expected_urls[i]->keyword());
    EXPECT_EQ(actual_urls[i]->url(), expected_urls[i]->url());
  }

  EXPECT_THAT(TemplateURLPrepopulateData::GetLocalPrepopulatedEngines(
                  "NOT A COUNTRY", *pref_service()),
              testing::IsEmpty());
}
#endif  // BUILDFLAG(IS_ANDROID)

struct UpdateRequirementsTestParams {
  std::string test_case_name;
  std::string db_country;
  int db_version;
  std::string profile_country;
  std::optional<int> pref_override_version;
  std::optional<BuiltinKeywordsMetadata> expected_output;
};

std::ostream& operator<<(std::ostream& os,
                         const UpdateRequirementsTestParams& value) {
  os << "{db_country=" << value.db_country
     << ", db_version=" << value.db_version
     << ", profile_country=" << value.profile_country;

  if (value.pref_override_version.has_value()) {
    os << ", pref_override_version=" << value.pref_override_version.value();
  }

  os << ", expected_output=";
  if (value.expected_output.has_value()) {
    os << value.expected_output.value();
  } else {
    os << "nullopt";
  }

  return os << "}";
}

class TemplateURLPrepopulateDataUpdateRequirementsTest
    : public TemplateURLPrepopulateDataTest,
      public testing::WithParamInterface<UpdateRequirementsTestParams> {
 public:
  void SetUp() override {
    TemplateURLPrepopulateDataTest::SetUp();
    OverrideCountryCommandLine(GetParam().profile_country);

    if (GetParam().pref_override_version.has_value()) {
      pref_service()->SetInteger(prefs::kSearchProviderOverridesVersion,
                                 GetParam().pref_override_version.value());
    }
  }

  static auto Cases() {
    return ::testing::ValuesIn({
        UpdateRequirementsTestParams{
            .test_case_name = "UpToDateMetadata",
            .db_country = "DE",
            .db_version = kCurrentDataVersion,
            .profile_country = "DE",
            .expected_output = std::nullopt,  // Update not needed.
        },
        {
            .test_case_name = "DifferentCountry",
            .db_country = "DE",
            .db_version = kCurrentDataVersion,
            .profile_country = "FR",
            .expected_output =
                BuildMetadata(CountryId("FR"), kCurrentDataVersion),
        },
        {
            .test_case_name = "DbCountryMissing",
            .db_country = "",
            .db_version = kCurrentDataVersion,
            .profile_country = "FR",
            .expected_output = std::nullopt,  // Update suppressed.
        },
        {
            .test_case_name = "CountryOverride",
            .db_country = "DE",
            .db_version = kCurrentDataVersion,
            .profile_country = switches::kEeaListCountryOverride,
            .expected_output = BuildMetadata(CountryId(), kCurrentDataVersion),
        },
        {
            .test_case_name = "DbMoreRecent",
            .db_country = "DE",
            .db_version = kCurrentDataVersion + 1,
            .profile_country = "DE",
            .expected_output = std::nullopt,  // Update suppressed.
        },
        {
            .test_case_name = "DbOlder",
            .db_country = "DE",
            .db_version = kCurrentDataVersion - 1,
            .profile_country = "DE",
            .expected_output =
                BuildMetadata(CountryId("DE"), kCurrentDataVersion),
        },
        {
            .test_case_name = "PrefOverride",
            .db_country = "DE",
            .db_version = kCurrentDataVersion,
            .profile_country = "DE",
            .pref_override_version = kCurrentDataVersion + 42,
            .expected_output =
                BuildMetadata(CountryId("DE"), kCurrentDataVersion + 42),
        },
    });
  }

  static std::string ParamToTestSuffix(
      const ::testing::TestParamInfo<UpdateRequirementsTestParams>& info) {
    return info.param.test_case_name;
  }

  static BuiltinKeywordsMetadata BuildMetadata(CountryId country_id,
                                               int version) {
    return {
        .country_id = regional_capabilities::CountryIdHolder(country_id),
        .data_version = version,
    };
  }
};

TEST_P(TemplateURLPrepopulateDataUpdateRequirementsTest,
       ComputeDatabaseUpdateRequirements) {
  WDKeywordsResult::Metadata database_metadata;
  database_metadata.builtin_keyword_data_version = GetParam().db_version;
  database_metadata.builtin_keyword_country =
      GetParam().db_country.empty()
          ? std::nullopt
          : std::optional(regional_capabilities::CountryIdHolder(
                country_codes::CountryId(GetParam().db_country)));

  std::optional<BuiltinKeywordsMetadata> out =
      prepopulate_data_resolver().ComputeDatabaseUpdateRequirements(
          database_metadata);

  EXPECT_EQ(GetParam().expected_output, out);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    TemplateURLPrepopulateDataUpdateRequirementsTest,
    TemplateURLPrepopulateDataUpdateRequirementsTest::Cases(),
    TemplateURLPrepopulateDataUpdateRequirementsTest::ParamToTestSuffix);

// -- Choice screen randomization checks --------------------------------------

class TemplateURLPrepopulateDataListTest
    : public TemplateURLPrepopulateDataTest,
      public testing::WithParamInterface<CountryId> {
 public:
  // The data type for prepopulate IDs
  // (`TemplateURLPrepopulateData::PrepopulatedEngine::id`), declared explicitly
  // for readability.
  using prepopulate_id_t = int;

  static std::string ParamToTestSuffix(
      const ::testing::TestParamInfo<CountryId>& info) {
    return std::string(info.param.CountryCode());
  }

  TemplateURLPrepopulateDataListTest()
      : country_id_(GetParam()), country_code_(country_id_.CountryCode()) {}

  void SetUp() override {
    if (kSkippedCountries.contains(country_id_)) {
      GTEST_SKIP() << "Skipping, the Default set is used for country code "
                   << country_code_;
    }

    TemplateURLPrepopulateDataTest::SetUp();
    OverrideCountryId(country_id_);
    for (const auto& engine :
         TemplateURLPrepopulateData::kRegionalSettings.find(country_id_)
             ->second->search_engines) {
      id_to_engine_[engine->id] = engine;
    }
  }

 protected:
  void RunEeaEngineListIsRandomTest(int number_of_iterations,
                                    double max_allowed_deviation) {
    // `per_engine_and_index_observations` maps from search engine prepopulate
    // ID to the number of times we saw this engine at a given position in the
    // list of items on the choice screen (the index in the vector is the
    // observed index in the choice screen list).
    base::flat_map<prepopulate_id_t, std::vector<int>>
        per_engine_and_index_observations;
    for (const auto& entry : id_to_engine_) {
      // Fill the map with 0s.
      per_engine_and_index_observations[entry.first] =
          std::vector<int>(kEeaChoiceScreenItemCount, 0);
    }

    // Gather observations, loading the search engines `number_of_iterations`
    // times and recording the returned positions.
    for (int current_run = 0; current_run < number_of_iterations;
         ++current_run) {
      // Simulate being a fresh new profile, where the shuffle seed is not set.
      pref_service()->ClearPref(
          prefs::kDefaultSearchProviderChoiceScreenRandomShuffleSeed);

      std::vector<std::unique_ptr<TemplateURLData>> actual_list =
          prepopulate_data_resolver().GetPrepopulatedEngines();

      ASSERT_EQ(actual_list.size(), kEeaChoiceScreenItemCount);
      for (size_t index = 0; index < kEeaChoiceScreenItemCount; ++index) {
        per_engine_and_index_observations[actual_list[index]->prepopulate_id]
                                         [index]++;
      }
    }

    // variance in the appearance rate of the various engines for a given index.
    std::vector<double> per_index_max_deviation(kEeaChoiceScreenItemCount, 0);
    base::flat_map<prepopulate_id_t, double> per_engine_max_deviation;
    base::flat_map<prepopulate_id_t, std::vector<double>>
        per_engine_and_index_probabilities;
    for (auto& entry : per_engine_and_index_observations) {
      prepopulate_id_t engine_id = entry.first;
      auto& per_index_observations = entry.second;

      int total_engine_appearances = 0;
      std::vector<double> probabilities(kEeaChoiceScreenItemCount, 0);

      for (size_t index = 0; index < kEeaChoiceScreenItemCount; ++index) {
        total_engine_appearances += per_index_observations[index];

        double appearance_probability_at_index =
            static_cast<double>(per_index_observations[index]) /
            number_of_iterations;

        double deviation =
            appearance_probability_at_index - kExpectedItemProbability;
        EXPECT_LE(deviation, max_allowed_deviation);

        per_engine_max_deviation[engine_id] =
            std::max(std::abs(deviation), per_engine_max_deviation[engine_id]);
        per_index_max_deviation[index] =
            std::max(std::abs(deviation), per_index_max_deviation[index]);
        probabilities[index] = appearance_probability_at_index;
      }

      ASSERT_EQ(total_engine_appearances, number_of_iterations);
      per_engine_and_index_probabilities[engine_id] = probabilities;
    }

    // Log the values if needed.
    if (testing::Test::HasFailure() || VLOG_IS_ON(1)) {
      std::string table_string = AssembleRowString(
          /*header_text=*/"",
          /*cell_values=*/{"0", "1", "2", "3", "4", "5", "6", "7", "max_dev"});

      for (auto& id_and_engine : id_to_engine_) {
        table_string += base::StringPrintf(
            "\n%s| %.4f ",
            AssembleRowString(
                /*header_text=*/base::UTF16ToUTF8(
                    std::u16string(id_and_engine.second->name)),
                /*cell_values=*/per_engine_and_index_probabilities.at(
                    id_and_engine.first))
                .c_str(),
            per_engine_max_deviation.at(id_and_engine.first));
      }

      table_string += "\n" + AssembleRowString(/*header_text=*/"max deviation",
                                               per_index_max_deviation);

      if (testing::Test::HasFailure()) {
        LOG(ERROR) << "Failure in the search engine distributions:\n"
                   << table_string;
      } else {
        VLOG(1) << "Search engine distributions:\n" << table_string;
      }
    }
  }

 private:
  // TODO(b/341047036): Investigate how to not have to skip this here.
  static inline const std::set<CountryId> kSkippedCountries = {
      CountryId("BL"),  // St. Barthélemy
      CountryId("EA"),  // Ceuta & Melilla
      CountryId("IC"),  // Canary Islands
      CountryId("MF"),  // St. Martin
  };

  static constexpr size_t kEeaChoiceScreenItemCount = 8u;
  static constexpr double kExpectedItemProbability =
      1. / kEeaChoiceScreenItemCount;

  static constexpr char row_header_format[] = "%s %20s ";
  static constexpr char string_cell_format[] = "| %5s ";
  static constexpr char probability_cell_format[] = "| %.3f ";

  std::string AssembleRowString(std::string_view header_text,
                                std::vector<double> cell_values) {
    std::string row_string = base::StringPrintf(
        row_header_format, country_code_.c_str(), header_text.data());
    for (double& val : cell_values) {
      row_string += base::StringPrintf(probability_cell_format, val);
    }

    return row_string;
  }

  std::string AssembleRowString(std::string_view header_text,
                                std::vector<std::string_view> cell_values) {
    std::string row_string = base::StringPrintf(
        row_header_format, country_code_.c_str(), header_text.data());
    for (std::string_view& val : cell_values) {
      row_string += base::StringPrintf(string_cell_format, val.data());
    }

    return row_string;
  }

  const CountryId country_id_;
  const std::string country_code_;
  base::flat_map<int,
                 raw_ptr<const TemplateURLPrepopulateData::PrepopulatedEngine>>
      id_to_engine_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    TemplateURLPrepopulateDataListTest,
    ::testing::ValuesIn(regional_capabilities::kEeaChoiceCountriesIds.begin(),
                        regional_capabilities::kEeaChoiceCountriesIds.end()),
    TemplateURLPrepopulateDataListTest::ParamToTestSuffix);

// Quick version of the test intended to flag glaring issues as part of the
// automated test suites.
TEST_P(TemplateURLPrepopulateDataListTest,
       QuickEeaEngineListIsRandomPerCountry) {
  RunEeaEngineListIsRandomTest(2000, 0.05);
}

// This test is permanently disabled as it is slow. It must *not* be removed as
// it is necessary for compliance (see b/341066703). It is used manually to
// prove that the search engines are shuffled in a randomly distributed order
// when they are loaded from disk.
// To run this test, use the following command line:
//
//     $OUT_DIR/components_unittests --gtest_also_run_disabled_tests \
//         --gtest_filter="*ManualEeaEngineListIsRandomPerCountry/*" \
//         --vmodule="*unittest*=1*"
//
// Explanations:
// - Since the test is marked as disabled, we need to explicitly instruct
//   filters to not discard it.
// - By default the test logs the stats only if it picks up an error. If we
//   want to always get the logs for all the countries, we need to enable
//   verbose logging for this file. The stats tables are logged to STDERR, so
//   append `2> output.txt` if it needs to be gathered in a file.
TEST_P(TemplateURLPrepopulateDataListTest,
       DISABLED_ManualEeaEngineListIsRandomPerCountry) {
  RunEeaEngineListIsRandomTest(20000, 0.01);
}
