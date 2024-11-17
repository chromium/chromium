// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_prepopulate_data.h"

#include <stddef.h>

#include <memory>
#include <numeric>
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
#include "components/search_engines/eea_countries_ids.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/testing_search_terms_data.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace {

SearchEngineType GetEngineType(const std::string& url) {
  TemplateURLData data;
  data.SetURL(url);
  return TemplateURL(data).GetEngineType(SearchTermsData());
}

std::string GetHostFromTemplateURLData(const TemplateURLData& data) {
  return TemplateURL(data).url_ref().GetHost(SearchTermsData());
}

const int kAllCountryIds[] = {'A' << 8 | 'D', 'A' << 8 | 'E', 'A' << 8 | 'F',
                              'A' << 8 | 'G', 'A' << 8 | 'I', 'A' << 8 | 'L',
                              'A' << 8 | 'M', 'A' << 8 | 'N', 'A' << 8 | 'O',
                              'A' << 8 | 'Q', 'A' << 8 | 'R', 'A' << 8 | 'S',
                              'A' << 8 | 'T', 'A' << 8 | 'U', 'A' << 8 | 'W',
                              'A' << 8 | 'X', 'A' << 8 | 'Z', 'B' << 8 | 'A',
                              'B' << 8 | 'B', 'B' << 8 | 'D', 'B' << 8 | 'E',
                              'B' << 8 | 'F', 'B' << 8 | 'G', 'B' << 8 | 'H',
                              'B' << 8 | 'I', 'B' << 8 | 'J', 'B' << 8 | 'M',
                              'B' << 8 | 'N', 'B' << 8 | 'O', 'B' << 8 | 'R',
                              'B' << 8 | 'S', 'B' << 8 | 'T', 'B' << 8 | 'V',
                              'B' << 8 | 'W', 'B' << 8 | 'Y', 'B' << 8 | 'Z',
                              'C' << 8 | 'A', 'C' << 8 | 'C', 'C' << 8 | 'D',
                              'C' << 8 | 'F', 'C' << 8 | 'G', 'C' << 8 | 'H',
                              'C' << 8 | 'I', 'C' << 8 | 'K', 'C' << 8 | 'L',
                              'C' << 8 | 'M', 'C' << 8 | 'N', 'C' << 8 | 'O',
                              'C' << 8 | 'Q', 'C' << 8 | 'R', 'C' << 8 | 'U',
                              'C' << 8 | 'V', 'C' << 8 | 'X', 'C' << 8 | 'Y',
                              'C' << 8 | 'Z', 'D' << 8 | 'E', 'D' << 8 | 'J',
                              'D' << 8 | 'K', 'D' << 8 | 'M', 'D' << 8 | 'O',
                              'D' << 8 | 'Z', 'E' << 8 | 'C', 'E' << 8 | 'E',
                              'E' << 8 | 'G', 'E' << 8 | 'R', 'E' << 8 | 'S',
                              'E' << 8 | 'T', 'F' << 8 | 'I', 'F' << 8 | 'J',
                              'F' << 8 | 'K', 'F' << 8 | 'M', 'F' << 8 | 'O',
                              'F' << 8 | 'R', 'G' << 8 | 'A', 'G' << 8 | 'B',
                              'G' << 8 | 'D', 'G' << 8 | 'E', 'G' << 8 | 'F',
                              'G' << 8 | 'G', 'G' << 8 | 'H', 'G' << 8 | 'I',
                              'G' << 8 | 'L', 'G' << 8 | 'M', 'G' << 8 | 'N',
                              'G' << 8 | 'P', 'G' << 8 | 'Q', 'G' << 8 | 'R',
                              'G' << 8 | 'S', 'G' << 8 | 'T', 'G' << 8 | 'U',
                              'G' << 8 | 'W', 'G' << 8 | 'Y', 'H' << 8 | 'K',
                              'H' << 8 | 'M', 'H' << 8 | 'N', 'H' << 8 | 'R',
                              'H' << 8 | 'T', 'H' << 8 | 'U', 'I' << 8 | 'D',
                              'I' << 8 | 'E', 'I' << 8 | 'L', 'I' << 8 | 'M',
                              'I' << 8 | 'N', 'I' << 8 | 'O', 'I' << 8 | 'P',
                              'I' << 8 | 'Q', 'I' << 8 | 'R', 'I' << 8 | 'S',
                              'I' << 8 | 'T', 'J' << 8 | 'E', 'J' << 8 | 'M',
                              'J' << 8 | 'O', 'J' << 8 | 'P', 'K' << 8 | 'E',
                              'K' << 8 | 'G', 'K' << 8 | 'H', 'K' << 8 | 'I',
                              'K' << 8 | 'M', 'K' << 8 | 'N', 'K' << 8 | 'P',
                              'K' << 8 | 'R', 'K' << 8 | 'W', 'K' << 8 | 'Y',
                              'K' << 8 | 'Z', 'L' << 8 | 'A', 'L' << 8 | 'B',
                              'L' << 8 | 'C', 'L' << 8 | 'I', 'L' << 8 | 'K',
                              'L' << 8 | 'R', 'L' << 8 | 'S', 'L' << 8 | 'T',
                              'L' << 8 | 'U', 'L' << 8 | 'V', 'L' << 8 | 'Y',
                              'M' << 8 | 'A', 'M' << 8 | 'C', 'M' << 8 | 'D',
                              'M' << 8 | 'E', 'M' << 8 | 'G', 'M' << 8 | 'H',
                              'M' << 8 | 'K', 'M' << 8 | 'L', 'M' << 8 | 'M',
                              'M' << 8 | 'N', 'M' << 8 | 'O', 'M' << 8 | 'P',
                              'M' << 8 | 'Q', 'M' << 8 | 'R', 'M' << 8 | 'S',
                              'M' << 8 | 'T', 'M' << 8 | 'U', 'M' << 8 | 'V',
                              'M' << 8 | 'W', 'M' << 8 | 'X', 'M' << 8 | 'Y',
                              'M' << 8 | 'Z', 'N' << 8 | 'A', 'N' << 8 | 'C',
                              'N' << 8 | 'E', 'N' << 8 | 'F', 'N' << 8 | 'G',
                              'N' << 8 | 'I', 'N' << 8 | 'L', 'N' << 8 | 'O',
                              'N' << 8 | 'P', 'N' << 8 | 'R', 'N' << 8 | 'U',
                              'N' << 8 | 'Z', 'O' << 8 | 'M', 'P' << 8 | 'A',
                              'P' << 8 | 'E', 'P' << 8 | 'F', 'P' << 8 | 'G',
                              'P' << 8 | 'H', 'P' << 8 | 'K', 'P' << 8 | 'L',
                              'P' << 8 | 'M', 'P' << 8 | 'N', 'P' << 8 | 'R',
                              'P' << 8 | 'S', 'P' << 8 | 'T', 'P' << 8 | 'W',
                              'P' << 8 | 'Y', 'Q' << 8 | 'A', 'R' << 8 | 'E',
                              'R' << 8 | 'O', 'R' << 8 | 'S', 'R' << 8 | 'U',
                              'R' << 8 | 'W', 'S' << 8 | 'A', 'S' << 8 | 'B',
                              'S' << 8 | 'C', 'S' << 8 | 'D', 'S' << 8 | 'E',
                              'S' << 8 | 'G', 'S' << 8 | 'H', 'S' << 8 | 'I',
                              'S' << 8 | 'J', 'S' << 8 | 'K', 'S' << 8 | 'L',
                              'S' << 8 | 'M', 'S' << 8 | 'N', 'S' << 8 | 'O',
                              'S' << 8 | 'R', 'S' << 8 | 'T', 'S' << 8 | 'V',
                              'S' << 8 | 'Y', 'S' << 8 | 'Z', 'T' << 8 | 'C',
                              'T' << 8 | 'D', 'T' << 8 | 'F', 'T' << 8 | 'G',
                              'T' << 8 | 'H', 'T' << 8 | 'J', 'T' << 8 | 'K',
                              'T' << 8 | 'L', 'T' << 8 | 'M', 'T' << 8 | 'N',
                              'T' << 8 | 'O', 'T' << 8 | 'R', 'T' << 8 | 'T',
                              'T' << 8 | 'V', 'T' << 8 | 'W', 'T' << 8 | 'Z',
                              'U' << 8 | 'A', 'U' << 8 | 'G', 'U' << 8 | 'M',
                              'U' << 8 | 'S', 'U' << 8 | 'Y', 'U' << 8 | 'Z',
                              'V' << 8 | 'A', 'V' << 8 | 'C', 'V' << 8 | 'E',
                              'V' << 8 | 'G', 'V' << 8 | 'I', 'V' << 8 | 'N',
                              'V' << 8 | 'U', 'W' << 8 | 'F', 'W' << 8 | 'S',
                              'Y' << 8 | 'E', 'Y' << 8 | 'T', 'Z' << 8 | 'A',
                              'Z' << 8 | 'M', 'Z' << 8 | 'W', -1};

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
  search_engines::SearchEngineChoiceService* search_engine_choice_service() {
    return &search_engines_test_environment_.search_engine_choice_service();
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return &search_engines_test_environment_.pref_service();
  }

  void SetupForChoiceScreenDisplay() {
    // Pick any EEA country
    const int kFranceCountryId =
        country_codes::CountryCharsToCountryID('F', 'R');
    OverrideCountryId(kFranceCountryId);
  }

  void OverrideCountryId(int country_id) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kSearchEngineChoiceCountry)) {
      base::CommandLine::ForCurrentProcess()->RemoveSwitch(
          switches::kSearchEngineChoiceCountry);
    }

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry,
        country_codes::CountryIDToCountryString(country_id));
  }

 protected:
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
};

// Verifies the set of prepopulate data doesn't contain entries with duplicate
// ids.
TEST_F(TemplateURLPrepopulateDataTest, UniqueIDs) {
  for (int country_id : kAllCountryIds) {
    OverrideCountryId(country_id);
    std::vector<std::unique_ptr<TemplateURLData>> urls =
        TemplateURLPrepopulateData::GetPrepopulatedEngines(
            pref_service(), search_engine_choice_service());
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

  for (int country_id : kAllCountryIds) {
    OverrideCountryId(country_id);

    const size_t kNumberOfSearchEngines =
        TemplateURLPrepopulateData::GetPrepopulatedEngines(
            pref_service(), search_engine_choice_service())
            .size();

    if (search_engines::IsEeaChoiceCountry(country_id)) {
      EXPECT_GE(kNumberOfSearchEngines, kMinEea)
          << " for country "
          << country_codes::CountryIDToCountryString(country_id);
      EXPECT_LE(kNumberOfSearchEngines,
                TemplateURLPrepopulateData::kMaxEeaPrepopulatedEngines)
          << " for country "
          << country_codes::CountryIDToCountryString(country_id);
    } else {
      EXPECT_GE(kNumberOfSearchEngines, kMinRow)
          << " for country "
          << country_codes::CountryIDToCountryString(country_id);
      EXPECT_LE(kNumberOfSearchEngines,
                TemplateURLPrepopulateData::kMaxRowPrepopulatedEngines)
          << " for country "
          << country_codes::CountryIDToCountryString(country_id);
    }
  }
}

TEST_F(TemplateURLPrepopulateDataTest, EntriesPerCountryConsistency) {
  for (int country_id : kAllCountryIds) {
    if (!search_engines::IsEeaChoiceCountry(country_id)) {
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
        base::ToVector(TemplateURLPrepopulateData::GetPrepopulatedEngines(
                           pref_service(), search_engine_choice_service()),
                       [](const auto& t_url) { return t_url->url(); });

    // Pulled straight from the country -> engine mapping.
    auto expected_urls = base::ToVector(
        TemplateURLPrepopulateData::GetPrepopulationSetFromCountryIDForTesting(
            country_id),
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
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          pref_service(), search_engine_choice_service());
  std::vector<std::unique_ptr<TemplateURLData>> t_urls_2 =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          pref_service(), search_engine_choice_service());

  ASSERT_EQ(t_urls_1.size(), t_urls_2.size());
  for (size_t i = 0; i < t_urls_1.size(); i++) {
    // Each prepopulated engine has a unique prepopulate_id, so we simply
    // compare those.
    ASSERT_EQ(t_urls_1[i]->prepopulate_id, t_urls_2[i]->prepopulate_id);
  }
}

// Verifies that the the search engines are re-shuffled on Chrome update.
TEST_F(TemplateURLPrepopulateDataTest,
       SearchEnginesOrderChangesOnChromeUpdate) {
  SetupForChoiceScreenDisplay();

  std::vector<std::unique_ptr<TemplateURLData>> t_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          pref_service(), search_engine_choice_service());

  // Change the saved chrome milestone to something else.
  pref_service()->SetInteger(
      prefs::kDefaultSearchProviderChoiceScreenShuffleMilestone, 3);

  std::vector<std::unique_ptr<TemplateURLData>> t_urls_after_update =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          pref_service(), search_engine_choice_service());

  ASSERT_EQ(t_urls.size(), t_urls_after_update.size());
  bool is_order_same = true;
  for (size_t i = 0; i < t_urls.size(); i++) {
    // Each prepopulated engine has a unique prepopulate_id, so we simply
    // compare those.
    is_order_same &=
        t_urls[i]->prepopulate_id == t_urls_after_update[i]->prepopulate_id;
    if (!is_order_same) {
      break;
    }
  }
  ASSERT_FALSE(is_order_same);
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
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          pref_service(), search_engine_choice_service());

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

  t_urls = TemplateURLPrepopulateData::GetPrepopulatedEngines(
      pref_service(), search_engine_choice_service());
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

  t_urls = TemplateURLPrepopulateData::GetPrepopulatedEngines(
      pref_service(), search_engine_choice_service());
  EXPECT_EQ(2u, t_urls.size());
}

TEST_F(TemplateURLPrepopulateDataTest, ClearProvidersFromPrefs) {
  OverrideCountryId(country_codes::kCountryIDUnknown);
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
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          pref_service(), search_engine_choice_service());
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
      TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
          pref_service(), search_engine_choice_service());
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
  OverrideCountryId(country_codes::CountryCharsToCountryID('U', 'S'));
  std::vector<std::unique_ptr<TemplateURLData>> t_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          pref_service(), search_engine_choice_service());

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
      TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
          pref_service(), search_engine_choice_service());
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
  for (int country_id : kAllCountryIds) {
    OverrideCountryId(country_id);

    std::vector<std::unique_ptr<TemplateURLData>> t_urls =
        TemplateURLPrepopulateData::GetPrepopulatedEngines(
            pref_service(), search_engine_choice_service());

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
  EXPECT_EQ(SEARCH_ENGINE_ASK, GetEngineType("http://www.ask.com/"));
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

  // URLs for engines not present in country-specific lists.
  EXPECT_EQ(SEARCH_ENGINE_NIGMA,
            GetEngineType("http://nigma.ru/?s={searchTerms}&arg1=value1"));
  // Also test matching against alternate URLs (and TLD+1 matching).
  EXPECT_EQ(SEARCH_ENGINE_SOFTONIC,
            GetEngineType("http://test.softonic.com.br/?{searchTerms}"));

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
  int us_country_id = country_codes::CountryCharsToCountryID('U', 'S');
  OverrideCountryId(us_country_id);
  EXPECT_EQ(
      TemplateURLPrepopulateData::GetPrepopulationSetFromCountryIDForTesting(
          us_country_id)[0]
          ->id,
      TemplateURLPrepopulateData::google.id);

  fallback_url = TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
      pref_service(), search_engine_choice_service());
  EXPECT_EQ(fallback_url->prepopulate_id,
            TemplateURLPrepopulateData::google.id);

  // Google is not first in CN; confirm it is found at index > 0.
  // If Google ever does reach top in China, this test will need to be adjusted:
  // check template_url_prepopulate_data.cc reference orders (engines_CN, etc.)
  // to find a suitable country and index.
  int cn_country_id = country_codes::CountryCharsToCountryID('C', 'N');
  OverrideCountryId(cn_country_id);
  fallback_url = TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
      pref_service(), search_engine_choice_service());
  EXPECT_NE(
      TemplateURLPrepopulateData::GetPrepopulationSetFromCountryIDForTesting(
          cn_country_id)[0]
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
  OverrideCountryId(country_codes::kCountryIDUnknown);
  ASSERT_EQ(TemplateURLPrepopulateData::GetPrepopulatedEngines(
                pref_service(), search_engine_choice_service())
                .size(),
            3u);

  // `GetPrepopulatedEngine()` only looks in the profile country's prepopulated
  // list.
  EXPECT_FALSE(TemplateURLPrepopulateData::GetPrepopulatedEngine(
      pref_service(), search_engine_choice_service(),
      TemplateURLPrepopulateData::ecosia.id));

  // Here we look in the full list.
  auto found_engine =
      TemplateURLPrepopulateData::GetPrepopulatedEngineFromFullList(
          pref_service(), search_engine_choice_service(),
          TemplateURLPrepopulateData::ecosia.id);
  EXPECT_TRUE(found_engine);
  auto expected_engine =
      TemplateURLDataFromPrepopulatedEngine(TemplateURLPrepopulateData::ecosia);
  ExpectSimilar(expected_engine.get(), found_engine.get());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(TemplateURLPrepopulateDataTest, GetLocalPrepopulatedEngines) {
  constexpr char sample_country[] = "US";
  OverrideCountryId(country_codes::CountryCharsToCountryID(sample_country[0],
                                                           sample_country[1]));

  // For a given country, the output from `GetLocalPrepopulatedEngines`
  // should match the template URLs obtained from `GetPrepopulatedEngines`.
  auto expected_urls = TemplateURLPrepopulateData::GetPrepopulatedEngines(
      pref_service(), search_engine_choice_service());
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

// -- Choice screen randomization checks --------------------------------------

class TemplateURLPrepopulateDataListTest
    : public TemplateURLPrepopulateDataTest,
      public testing::WithParamInterface<int> {
 public:
  // The data type for prepopulate IDs
  // (`TemplateURLPrepopulateData::PrepopulatedEngine::id`), declared explicitly
  // for readability.
  using prepopulate_id_t = int;

  static std::string ParamToTestSuffix(
      const ::testing::TestParamInfo<int>& info) {
    return country_codes::CountryIDToCountryString(info.param);
  }

  TemplateURLPrepopulateDataListTest()
      : country_id_(GetParam()),
        country_code_(country_codes::CountryIDToCountryString(country_id_)) {}

  void SetUp() override {
    if (kSkippedCountries.contains(country_id_)) {
      GTEST_SKIP() << "Skipping, the Default set is used for country code "
                   << country_code_;
    }

    TemplateURLPrepopulateDataTest::SetUp();
    OverrideCountryId(country_id_);
    for (const auto& engine :
         TemplateURLPrepopulateData::GetPrepopulationSetFromCountryIDForTesting(
             country_id_)) {
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
          TemplateURLPrepopulateData::GetPrepopulatedEngines(
              pref_service(), search_engine_choice_service());

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
  static inline const std::set<int> kSkippedCountries = {
      country_codes::CountryCharsToCountryID('B', 'L'),  // St. Barthélemy
      country_codes::CountryCharsToCountryID('E', 'A'),  // Ceuta & Melilla
      country_codes::CountryCharsToCountryID('I', 'C'),  // Canary Islands
      country_codes::CountryCharsToCountryID('M', 'F'),  // St. Martin
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

  const int country_id_;
  const std::string country_code_;
  base::flat_map<int,
                 raw_ptr<const TemplateURLPrepopulateData::PrepopulatedEngine>>
      id_to_engine_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    TemplateURLPrepopulateDataListTest,
    ::testing::ValuesIn(search_engines::kEeaChoiceCountriesIds.begin(),
                        search_engines::kEeaChoiceCountriesIds.end()),
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
