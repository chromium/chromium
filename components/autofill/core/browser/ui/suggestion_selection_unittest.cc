// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/ui/suggestion_selection.h"

#include <algorithm>
#include <iterator>

#include "base/guid.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace suggestion_selection {

using base::ASCIIToUTF16;
using testing::Each;
using testing::ElementsAre;
using testing::Field;
using testing::Not;
using testing::ResultOf;

namespace {
const std::string TEST_APP_LOCALE = "en-US";

template <typename T>
bool CompareElements(T* a, T* b) {
  return a->Compare(*b) < 0;
}

template <typename T>
bool ElementsEqual(T* a, T* b) {
  return a->Compare(*b) == 0;
}

// Verifies that two vectors have the same elements (according to T::Compare)
// while ignoring order. This is useful because multiple profiles or credit
// cards that are added to the SQLite DB within the same second will be returned
// in GUID (aka random) order.
template <typename T>
void ExpectSameElements(const std::vector<T*>& expectations,
                        const std::vector<T*>& results) {
  ASSERT_EQ(expectations.size(), results.size());

  std::vector<T*> expectations_copy = expectations;
  std::sort(expectations_copy.begin(), expectations_copy.end(),
            CompareElements<T>);
  std::vector<T*> results_copy = results;
  std::sort(results_copy.begin(), results_copy.end(), CompareElements<T>);

  EXPECT_EQ(std::mismatch(results_copy.begin(), results_copy.end(),
                          expectations_copy.begin(), ElementsEqual<T>)
                .first,
            results_copy.end());
}

}  // anonymous namespace

class SuggestionSelectionTest : public testing::Test {
 public:
  SuggestionSelectionTest()
      : app_locale_(TEST_APP_LOCALE), comparator_(TEST_APP_LOCALE) {}

 protected:
  std::unique_ptr<AutofillProfile> CreateProfileUniquePtr(
      const char* first_name,
      const char* last_name = "Morrison") {
    std::unique_ptr<AutofillProfile> profile_ptr =
        std::make_unique<AutofillProfile>(base::GenerateGUID(),
                                          test::kEmptyOrigin);
    test::SetProfileInfo(profile_ptr.get(), first_name, "Mitchell", last_name,
                         "johnwayne@me.xyz", "Fox",
                         "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                         "Hollywood", "CA", "91601", "US", "12345678910");
    return profile_ptr;
  }

  base::string16 GetCanonicalUtf16Content(const char* content) {
    return comparator_.NormalizeForComparison(ASCIIToUTF16(content));
  }

  std::vector<Suggestion> CreateSuggestions(
      const std::vector<AutofillProfile*>& profiles,
      const ServerFieldType& field_type) {
    std::vector<Suggestion> suggestions;
    std::transform(profiles.begin(), profiles.end(),
                   std::back_inserter(suggestions),
                   [field_type](const AutofillProfile* profile) {
                     return Suggestion(profile->GetRawInfo(field_type));
                   });

    return suggestions;
  }

  const std::string app_locale_;
  const AutofillProfileComparator comparator_;
};

TEST_F(SuggestionSelectionTest,
       GetPrefixMatchedSuggestions_GetMatchingProfile) {
  const std::unique_ptr<AutofillProfile> profile1 =
      CreateProfileUniquePtr("Marion");
  const std::unique_ptr<AutofillProfile> profile2 =
      CreateProfileUniquePtr("Bob");

  std::vector<AutofillProfile*> matched_profiles;
  auto suggestions = GetPrefixMatchedSuggestions(
      AutofillType(NAME_FIRST), ASCIIToUTF16("Mar"),
      GetCanonicalUtf16Content("Mar"), comparator_, false,
      {profile1.get(), profile2.get()}, &matched_profiles);

  ASSERT_EQ(1U, suggestions.size());
  ASSERT_EQ(1U, matched_profiles.size());
  EXPECT_THAT(suggestions,
              ElementsAre(Field(&Suggestion::value, ASCIIToUTF16("Marion"))));
}

TEST_F(SuggestionSelectionTest, GetPrefixMatchedSuggestions_NoMatchingProfile) {
  const std::unique_ptr<AutofillProfile> profile1 =
      CreateProfileUniquePtr("Bob");

  std::vector<AutofillProfile*> matched_profiles;
  auto suggestions =
      GetPrefixMatchedSuggestions(AutofillType(NAME_FIRST), ASCIIToUTF16("Mar"),
                                  GetCanonicalUtf16Content("Mar"), comparator_,
                                  false, {profile1.get()}, &matched_profiles);

  ASSERT_TRUE(matched_profiles.empty());
  ASSERT_TRUE(suggestions.empty());
}

TEST_F(SuggestionSelectionTest,
       GetPrefixMatchedSuggestions_EmptyProfilesInput) {
  std::vector<AutofillProfile*> matched_profiles;
  auto suggestions =
      GetPrefixMatchedSuggestions(AutofillType(NAME_FIRST), ASCIIToUTF16("Mar"),
                                  GetCanonicalUtf16Content("Mar"), comparator_,
                                  false, {}, &matched_profiles);

  ASSERT_TRUE(matched_profiles.empty());
  ASSERT_TRUE(suggestions.empty());
}

TEST_F(SuggestionSelectionTest, GetPrefixMatchedSuggestions_LimitProfiles) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_data;
  for (size_t i = 0; i < kMaxSuggestedProfilesCount; i++) {
    profiles_data.push_back(CreateProfileUniquePtr("Marion"));
  }

  // Add another profile to go above the limit.
  profiles_data.push_back(CreateProfileUniquePtr("Marie"));

  // Map all the pointers into an array that has the right type.
  std::vector<AutofillProfile*> profiles_pointers;
  std::transform(profiles_data.begin(), profiles_data.end(),
                 std::back_inserter(profiles_pointers),
                 [](const std::unique_ptr<AutofillProfile>& profile) {
                   return profile.get();
                 });

  std::vector<AutofillProfile*> matched_profiles;
  auto suggestions =
      GetPrefixMatchedSuggestions(AutofillType(NAME_FIRST), ASCIIToUTF16("Mar"),
                                  GetCanonicalUtf16Content("Mar"), comparator_,
                                  false, profiles_pointers, &matched_profiles);

  // Marie should not be found.
  ASSERT_EQ(kMaxSuggestedProfilesCount, suggestions.size());
  ASSERT_EQ(kMaxSuggestedProfilesCount, matched_profiles.size());

  EXPECT_THAT(suggestions,
              Each(Field(&Suggestion::value, Not(ASCIIToUTF16("Marie")))));

  EXPECT_THAT(matched_profiles,
              Each(ResultOf(
                  [](const AutofillProfile* profile_ptr) {
                    return profile_ptr->GetRawInfo(NAME_FIRST);
                  },
                  Not(ASCIIToUTF16("Marie")))));
}

TEST_F(SuggestionSelectionTest, GetPrefixMatchedSuggestions_SkipInvalid) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeatures(
      /*enabled_features=*/{features::kAutofillProfileServerValidation,
                            features::kAutofillProfileClientValidation},
      /*disabled_features=*/{});
  const std::unique_ptr<AutofillProfile> profile_server_invalid =
      CreateProfileUniquePtr("Marion");
  const std::unique_ptr<AutofillProfile> profile_client_invalid =
      CreateProfileUniquePtr("Bob");
  const std::unique_ptr<AutofillProfile> profile_valid =
      CreateProfileUniquePtr("Rose");
  const std::unique_ptr<AutofillProfile> profile_client_invalid_country_empty =
      CreateProfileUniquePtr("Lost");

  profile_server_invalid->SetValidityState(
      ADDRESS_HOME_STATE, AutofillProfile::INVALID, AutofillProfile::SERVER);
  profile_client_invalid->SetValidityState(
      ADDRESS_HOME_STATE, AutofillProfile::INVALID, AutofillProfile::CLIENT);
  profile_client_invalid_country_empty->SetValidityState(
      ADDRESS_HOME_STATE, AutofillProfile::INVALID, AutofillProfile::CLIENT);
  profile_client_invalid_country_empty->SetRawInfo(ADDRESS_HOME_COUNTRY,
                                                   ASCIIToUTF16(""));

  const std::vector<AutofillProfile*> profiles_data = {
      profile_server_invalid.get(), profile_client_invalid.get(),
      profile_valid.get(), profile_client_invalid_country_empty.get()};

  std::vector<AutofillProfile*> matched_profiles;
  auto suggestions = GetPrefixMatchedSuggestions(
      AutofillType(ADDRESS_HOME_STATE), ASCIIToUTF16("C"),
      GetCanonicalUtf16Content("C"), comparator_, false, profiles_data,
      &matched_profiles);

  ASSERT_EQ(2U, suggestions.size());
  ASSERT_EQ(2U, matched_profiles.size());
  EXPECT_THAT(suggestions,
              ElementsAre(Field(&Suggestion::value, ASCIIToUTF16("CA")),
                          Field(&Suggestion::value, ASCIIToUTF16("CA"))));

  std::vector<AutofillProfile*> expected_result;
  expected_result.push_back(profile_valid.get());
  expected_result.push_back(profile_client_invalid_country_empty.get());
  ExpectSameElements(matched_profiles, expected_result);
}

TEST_F(SuggestionSelectionTest, GetUniqueSuggestions_SingleDedupe) {
  // Give two suggestions with the same name, and no other field to compare.
  // Expect only one unique suggestion.
  const std::unique_ptr<AutofillProfile> profile1 =
      CreateProfileUniquePtr("Bob");
  const std::unique_ptr<AutofillProfile> profile2 =
      CreateProfileUniquePtr("Bob");

  auto profile_pointers = {profile1.get(), profile2.get()};

  std::vector<AutofillProfile*> unique_matched_profiles;
  auto unique_suggestions =
      GetUniqueSuggestions({}, comparator_, app_locale_, profile_pointers,
                           CreateSuggestions(profile_pointers, NAME_FIRST),
                           &unique_matched_profiles);

  ASSERT_EQ(1U, unique_suggestions.size());
  ASSERT_EQ(1U, unique_matched_profiles.size());
  EXPECT_THAT(unique_suggestions,
              ElementsAre(Field(&Suggestion::value, ASCIIToUTF16("Bob"))));
}

TEST_F(SuggestionSelectionTest, GetUniqueSuggestions_MultipleDedupe) {
  // Give two suggestions with the same name and one with a different, and
  // also last name field to compare.
  // Expect all profiles listed as unique suggestions.
  const std::unique_ptr<AutofillProfile> profile1 =
      CreateProfileUniquePtr("Bob", "Morrison");
  const std::unique_ptr<AutofillProfile> profile2 =
      CreateProfileUniquePtr("Bob", "Parker");
  const std::unique_ptr<AutofillProfile> profile3 =
      CreateProfileUniquePtr("Mary", "Parker");

  auto profile_pointers = {profile1.get(), profile2.get(), profile3.get()};

  std::vector<AutofillProfile*> unique_matched_profiles;
  auto unique_suggestions = GetUniqueSuggestions(
      {NAME_LAST}, comparator_, app_locale_, profile_pointers,
      CreateSuggestions(profile_pointers, NAME_FIRST),
      &unique_matched_profiles);

  ASSERT_EQ(3U, unique_suggestions.size());
  ASSERT_EQ(3U, unique_matched_profiles.size());

  EXPECT_THAT(unique_suggestions,
              ElementsAre(Field(&Suggestion::value, ASCIIToUTF16("Bob")),
                          Field(&Suggestion::value, ASCIIToUTF16("Bob")),
                          Field(&Suggestion::value, ASCIIToUTF16("Mary"))));
}

TEST_F(SuggestionSelectionTest, GetUniqueSuggestions_DedupeLimit) {
  // Test limit of suggestions.
  std::vector<std::unique_ptr<AutofillProfile>> profiles_data;
  for (size_t i = 0; i < kMaxUniqueSuggestionsCount + 1; i++) {
    profiles_data.push_back(CreateProfileUniquePtr(
        base::StringPrintf("Bob %zu", i).c_str(), "Doe"));
  }

  // Map all the pointers into an array that has the right type.
  std::vector<AutofillProfile*> profiles_pointers;
  std::transform(profiles_data.begin(), profiles_data.end(),
                 std::back_inserter(profiles_pointers),
                 [](const std::unique_ptr<AutofillProfile>& profile) {
                   return profile.get();
                 });

  std::vector<AutofillProfile*> unique_matched_profiles;
  auto unique_suggestions = GetUniqueSuggestions(
      {NAME_LAST}, comparator_, app_locale_, profiles_pointers,
      CreateSuggestions(profiles_pointers, NAME_FIRST),
      &unique_matched_profiles);

  ASSERT_EQ(kMaxUniqueSuggestionsCount, unique_suggestions.size());
  ASSERT_EQ(kMaxUniqueSuggestionsCount, unique_matched_profiles.size());

  // All profiles are different.
  for (size_t i = 0; i < unique_suggestions.size(); i++) {
    ASSERT_EQ(ASCIIToUTF16(base::StringPrintf("Bob %zu", i)),
              unique_suggestions[i].value);
  }
}

TEST_F(SuggestionSelectionTest, GetUniqueSuggestions_PruneSuggestions) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(features::kAutofillPruneSuggestions);

  // Test limit of suggestions when the feature is enabled.
  std::vector<std::unique_ptr<AutofillProfile>> profiles_data;
  for (size_t i = 0; i < kMaxPrunedUniqueSuggestionsCount + 1; i++) {
    profiles_data.push_back(CreateProfileUniquePtr(
        base::StringPrintf("Bob %zu", i).c_str(), "Doe"));
  }

  // Map all the pointers into an array that has the right type.
  std::vector<AutofillProfile*> profiles_pointers;
  std::transform(profiles_data.begin(), profiles_data.end(),
                 std::back_inserter(profiles_pointers),
                 [](const std::unique_ptr<AutofillProfile>& profile) {
                   return profile.get();
                 });

  std::vector<AutofillProfile*> unique_matched_profiles;
  auto unique_suggestions = GetUniqueSuggestions(
      {NAME_LAST}, comparator_, app_locale_, profiles_pointers,
      CreateSuggestions(profiles_pointers, NAME_FIRST),
      &unique_matched_profiles);

  ASSERT_EQ(kMaxPrunedUniqueSuggestionsCount, unique_suggestions.size());
  ASSERT_EQ(kMaxPrunedUniqueSuggestionsCount, unique_matched_profiles.size());

  // All profiles are different.
  for (size_t i = 0; i < unique_suggestions.size(); i++) {
    ASSERT_EQ(ASCIIToUTF16(base::StringPrintf("Bob %zu", i)),
              unique_suggestions[i].value);
  }
}

TEST_F(SuggestionSelectionTest, GetUniqueSuggestions_EmptyMatchingProfiles) {
  std::vector<AutofillProfile*> unique_matched_profiles;
  auto unique_suggestions = GetUniqueSuggestions(
      {NAME_LAST}, comparator_, app_locale_, {}, {}, &unique_matched_profiles);

  ASSERT_EQ(0U, unique_matched_profiles.size());
  ASSERT_EQ(0U, unique_suggestions.size());
}

TEST_F(SuggestionSelectionTest, RemoveProfilesNotUsedSinceTimestamp) {
  const char kAddressesSuppressedHistogramName[] =
      "Autofill.AddressesSuppressedForDisuse";
  base::Time kCurrentTime;
  bool result =
      base::Time::FromUTCString("2017-01-02T00:00:01Z", &kCurrentTime);
  ASSERT_TRUE(result);
  constexpr size_t kNumProfiles = 10;
  constexpr base::TimeDelta k30Days = base::TimeDelta::FromDays(30);
  constexpr base::TimeDelta k5DaysBuffer = base::TimeDelta::FromDays(5);

  // Set up the profile vectors with last use dates ranging from |kCurrentTime|
  // to 270 days ago, in 30 day increments.  Note that the profiles are sorted
  // by decreasing last use date.
  std::vector<std::unique_ptr<AutofillProfile>> all_profile_data;
  for (size_t i = 0; i < kNumProfiles; ++i) {
    all_profile_data.push_back(std::make_unique<AutofillProfile>(
        base::GenerateGUID(), "https://example.com"));
    all_profile_data[i]->set_use_date(kCurrentTime - (i * k30Days));
  }

  // Map all the pointers into an array that has the right type.
  std::vector<AutofillProfile*> all_profile_ptrs;
  std::transform(all_profile_data.begin(), all_profile_data.end(),
                 std::back_inserter(all_profile_ptrs),
                 [](const std::unique_ptr<AutofillProfile>& profile) {
                   return profile.get();
                 });

  // Verify that disused profiles get removed from the end. Note that the last
  // four profiles have use dates more than 175 days ago.
  {
    const int kNbSuggestions = 6;
    const base::Time k175DaysAgo =
        kCurrentTime - (k30Days * kNbSuggestions) + k5DaysBuffer;
    // Create a working copy of the profile pointers.
    std::vector<AutofillProfile*> profiles(all_profile_ptrs);

    // The first 6 have use dates more recent than 175 days ago.
    std::vector<AutofillProfile*> expected_profiles(
        profiles.begin(), profiles.begin() + kNbSuggestions);

    // Filter the profiles while capturing histograms.
    base::HistogramTester histogram_tester;
    suggestion_selection::RemoveProfilesNotUsedSinceTimestamp(k175DaysAgo,
                                                              &profiles);

    // Validate that we get the expected filtered profiles and histograms.
    EXPECT_EQ(expected_profiles, profiles);
    histogram_tester.ExpectTotalCount(kAddressesSuppressedHistogramName, 1);
    histogram_tester.ExpectBucketCount(kAddressesSuppressedHistogramName,
                                       kNumProfiles - kNbSuggestions, 1);
  }

  // Reverse the profile order and verify that disused profiles get removed
  // from the beginning. Note that the first five profiles, post reversal, have
  // use dates more then 145 days ago.
  {
    const int kNbSuggestions = 5;
    const base::Time k145DaysAgo =
        kCurrentTime - (k30Days * kNbSuggestions) + k5DaysBuffer;
    // Create a reversed working copy of the profile pointers.
    std::vector<AutofillProfile*> profiles(all_profile_ptrs.rbegin(),
                                           all_profile_ptrs.rend());

    // The last 5 profiles have use dates more recent than 145 days ago.
    std::vector<AutofillProfile*> expected_profiles(
        profiles.begin() + kNbSuggestions, profiles.end());

    // Filter the profiles while capturing histograms.
    base::HistogramTester histogram_tester;
    suggestion_selection::RemoveProfilesNotUsedSinceTimestamp(k145DaysAgo,
                                                              &profiles);

    // Validate that we get the expected filtered profiles and histograms.
    EXPECT_EQ(expected_profiles, profiles);
    histogram_tester.ExpectTotalCount(kAddressesSuppressedHistogramName, 1);
    histogram_tester.ExpectBucketCount(kAddressesSuppressedHistogramName,
                                       kNumProfiles - kNbSuggestions, 1);
  }

  // Randomize the profile order and validate that the filtered list retains
  // that order. Note that the six profiles have use dates more then 115 days
  // ago.
  {
    const int kNbSuggestions = 4;
    const base::Time k115DaysAgo =
        kCurrentTime - (k30Days * kNbSuggestions) + k5DaysBuffer;

    // Created a shuffled master copy of the profile pointers.
    std::vector<AutofillProfile*> shuffled_profiles(all_profile_ptrs);
    base::RandomShuffle(shuffled_profiles.begin(), shuffled_profiles.end());

    // Copy the shuffled profile pointer collections to use as the working set.
    std::vector<AutofillProfile*> profiles_recently_used(shuffled_profiles);

    // Filter the profiles while capturing histograms.
    base::HistogramTester histogram_tester;
    suggestion_selection::RemoveProfilesNotUsedSinceTimestamp(
        k115DaysAgo, &profiles_recently_used);

    // Validate that we have the right profiles. Iterate of the shuffled master
    // copy and the filtered copy at the same time making sure that the elements
    // in the filtered copy occur in the same order as the shuffled master.
    // Along the way, validate that the elements in and out of the filtered copy
    // have appropriate use dates.
    // TODO(crbug.com/908456): Refactor the loops to use STL and Gmock.
    EXPECT_EQ(4u, profiles_recently_used.size());
    auto it = shuffled_profiles.begin();
    for (const AutofillProfile* profile : profiles_recently_used) {
      for (; it != shuffled_profiles.end() && (*it) != profile; ++it) {
        EXPECT_LT((*it)->use_date(), k115DaysAgo);
      }
      ASSERT_TRUE(it != shuffled_profiles.end());
      EXPECT_GT(profile->use_date(), k115DaysAgo);
      ++it;
    }
    for (; it != shuffled_profiles.end(); ++it) {
      EXPECT_LT((*it)->use_date(), k115DaysAgo);
    }

    // Validate the histograms.
    histogram_tester.ExpectTotalCount(kAddressesSuppressedHistogramName, 1);
    histogram_tester.ExpectBucketCount(kAddressesSuppressedHistogramName, 6, 1);
  }

  // Verify all profiles are removed if they're all disused.
  {
    // Create a working copy of the profile pointers.
    std::vector<AutofillProfile*> profiles(all_profile_ptrs);

    // Filter the profiles while capturing histograms.
    base::HistogramTester histogram_tester;
    suggestion_selection::RemoveProfilesNotUsedSinceTimestamp(
        kCurrentTime + base::TimeDelta::FromDays(1), &profiles);

    // Validate that we get the expected filtered profiles and histograms.
    EXPECT_TRUE(profiles.empty());
    histogram_tester.ExpectTotalCount(kAddressesSuppressedHistogramName, 1);
    histogram_tester.ExpectBucketCount(kAddressesSuppressedHistogramName,
                                       kNumProfiles, 1);
  }

  // Verify all profiles are retained if they're sufficiently recently used.
  {
    // Create a working copy of the profile pointers.
    std::vector<AutofillProfile*> profiles(all_profile_ptrs);

    // Filter the profiles while capturing histograms.
    base::HistogramTester histogram_tester;
    suggestion_selection::RemoveProfilesNotUsedSinceTimestamp(
        kCurrentTime -
            base::TimeDelta::FromDays(2 * kNumProfiles * k30Days.InDays()),
        &profiles);

    // Validate that we get the expected filtered profiles and histograms.
    EXPECT_EQ(all_profile_ptrs, profiles);
    histogram_tester.ExpectTotalCount(kAddressesSuppressedHistogramName, 1);
    histogram_tester.ExpectBucketCount(kAddressesSuppressedHistogramName, 0, 1);
  }
}

TEST_F(SuggestionSelectionTest,
       PrepareSuggestions_DiscardDuplicateSuggestions) {
  std::vector<Suggestion> suggestions{Suggestion(ASCIIToUTF16("Jon Snow")),
                                      Suggestion(ASCIIToUTF16("Jon Snow")),
                                      Suggestion(ASCIIToUTF16("Jon Snow")),
                                      Suggestion(ASCIIToUTF16("Jon Snow"))};

  const std::vector<base::string16> labels{
      ASCIIToUTF16("2 Beyond-the-Wall Rd"), ASCIIToUTF16("1 Winterfell Ln"),
      ASCIIToUTF16("2 Beyond-the-Wall Rd"),
      ASCIIToUTF16("2 Beyond-the-Wall Rd.")};

  PrepareSuggestions(labels, &suggestions, comparator_);

  // Suggestions are sorted from highest to lowest rank, so check that
  // duplicates with a lower rank are removed.
  EXPECT_THAT(
      suggestions,
      ElementsAre(
          AllOf(
              Field(&Suggestion::value, ASCIIToUTF16("Jon Snow")),
              Field(&Suggestion::label, ASCIIToUTF16("2 Beyond-the-Wall Rd"))),
          AllOf(Field(&Suggestion::value, ASCIIToUTF16("Jon Snow")),
                Field(&Suggestion::label, ASCIIToUTF16("1 Winterfell Ln")))));
}

TEST_F(SuggestionSelectionTest,
       PrepareSuggestions_KeepNonDuplicateSuggestions) {
  std::vector<Suggestion> suggestions{Suggestion(ASCIIToUTF16("Sansa")),
                                      Suggestion(ASCIIToUTF16("Sansa")),
                                      Suggestion(ASCIIToUTF16("Brienne"))};

  const std::vector<base::string16> labels{ASCIIToUTF16("1 Winterfell Ln"),
                                           ASCIIToUTF16(""),
                                           ASCIIToUTF16("1 Winterfell Ln")};

  PrepareSuggestions(labels, &suggestions, comparator_);

  EXPECT_THAT(
      suggestions,
      ElementsAre(
          AllOf(Field(&Suggestion::value, ASCIIToUTF16("Sansa")),
                Field(&Suggestion::label, ASCIIToUTF16("1 Winterfell Ln"))),
          AllOf(Field(&Suggestion::value, ASCIIToUTF16("Sansa")),
                Field(&Suggestion::label, ASCIIToUTF16(""))),
          AllOf(Field(&Suggestion::value, ASCIIToUTF16("Brienne")),
                Field(&Suggestion::label, ASCIIToUTF16("1 Winterfell Ln")))));
}

TEST_F(SuggestionSelectionTest, PrepareSuggestions_SameStringInValueAndLabel) {
  std::vector<Suggestion> suggestions{
      Suggestion(base::UTF8ToUTF16("4 Mañana Road"))};

  const std::vector<base::string16> labels{ASCIIToUTF16("4 manana road")};

  PrepareSuggestions(labels, &suggestions, comparator_);
  EXPECT_THAT(suggestions,
              ElementsAre(AllOf(
                  Field(&Suggestion::value, base::UTF8ToUTF16("4 Mañana Road")),
                  Field(&Suggestion::label, base::string16()))));
}

}  // namespace suggestion_selection
}  // namespace autofill
