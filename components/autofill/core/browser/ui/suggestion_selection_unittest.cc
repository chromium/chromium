// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/ui/suggestion_selection.h"

#include <iterator>

#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/to_vector.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::suggestion_selection {

using base::ASCIIToUTF16;
using testing::Each;
using testing::ElementsAre;
using testing::Field;
using testing::Not;
using testing::ResultOf;

namespace {
const std::string TEST_APP_LOCALE = "en-US";
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
        std::make_unique<AutofillProfile>();
    test::SetProfileInfo(profile_ptr.get(), first_name, "Mitchell", last_name,
                         "johnwayne@me.xyz", "Fox",
                         "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                         "Hollywood", "CA", "91601", "US", "12345678910");
    return profile_ptr;
  }

  std::u16string GetCanonicalUtf16Content(const char* content) {
    return AutofillProfileComparator::NormalizeForComparison(
        ASCIIToUTF16(content));
  }

  std::vector<Suggestion> CreateSuggestions(
      const std::vector<AutofillProfile*>& profiles,
      const ServerFieldType& field_type) {
    return base::test::ToVector(
        profiles, [field_type](const AutofillProfile* profile) {
          return Suggestion(profile->GetRawInfo(field_type));
        });
  }

  const std::string app_locale_;
  const AutofillProfileComparator comparator_;
};

TEST_F(SuggestionSelectionTest, GetPrefixMatchedProfiles_GetMatchingProfile) {
  const std::unique_ptr<AutofillProfile> profile1 =
      CreateProfileUniquePtr("Marion");
  const std::unique_ptr<AutofillProfile> profile2 =
      CreateProfileUniquePtr("Bob");

  std::vector<AutofillProfile*> matched_profiles = GetPrefixMatchedProfiles(
      AutofillType(NAME_FIRST), u"Mar", GetCanonicalUtf16Content("Mar"),
      comparator_.app_locale(), false, {profile1.get(), profile2.get()});

  ASSERT_EQ(1U, matched_profiles.size());
  EXPECT_EQ(profile1.get(), matched_profiles[0]);
}

TEST_F(SuggestionSelectionTest, GetPrefixMatchedProfiles_NoMatchingProfile) {
  const std::unique_ptr<AutofillProfile> profile1 =
      CreateProfileUniquePtr("Bob");

  std::vector<AutofillProfile*> matched_profiles = GetPrefixMatchedProfiles(
      AutofillType(NAME_FIRST), u"Mar", GetCanonicalUtf16Content("Mar"),
      comparator_.app_locale(), false, {profile1.get()});

  ASSERT_TRUE(matched_profiles.empty());
}

TEST_F(SuggestionSelectionTest, GetPrefixMatchedProfiles_EmptyProfilesInput) {
  std::vector<AutofillProfile*> matched_profiles = GetPrefixMatchedProfiles(
      AutofillType(NAME_FIRST), u"Mar", GetCanonicalUtf16Content("Mar"),
      comparator_.app_locale(), false, {});

  ASSERT_TRUE(matched_profiles.empty());
}

TEST_F(SuggestionSelectionTest, GetPrefixMatchedProfiles_LimitProfiles) {
  std::vector<std::unique_ptr<AutofillProfile>> profiles_data;
  for (size_t i = 0; i < kMaxSuggestedProfilesCount; i++) {
    profiles_data.push_back(CreateProfileUniquePtr("Marion"));
  }

  // Add another profile to go above the limit.
  profiles_data.push_back(CreateProfileUniquePtr("Marie"));

  // Map all the pointers into an array that has the right type.
  std::vector<AutofillProfile*> profiles_pointers = base::test::ToVector(
      profiles_data, &std::unique_ptr<AutofillProfile>::get);

  std::vector<AutofillProfile*> matched_profiles = GetPrefixMatchedProfiles(
      AutofillType(NAME_FIRST), u"Mar", GetCanonicalUtf16Content("Mar"),
      comparator_.app_locale(), false, profiles_pointers);

  // Marie should not be found.
  ASSERT_EQ(kMaxSuggestedProfilesCount, matched_profiles.size());

  EXPECT_THAT(matched_profiles,
              Each(ResultOf(
                  [](const AutofillProfile* profile_ptr) {
                    return profile_ptr->GetRawInfo(NAME_FIRST);
                  },
                  Not(u"Marie"))));
}

TEST_F(SuggestionSelectionTest, GetUniqueSuggestions_SingleDedupe) {
  // Give two suggestions with the same name, and no other field to compare.
  // Expect only one unique suggestion.
  const std::unique_ptr<AutofillProfile> profile1 =
      CreateProfileUniquePtr("Bob");
  const std::unique_ptr<AutofillProfile> profile2 =
      CreateProfileUniquePtr("Bob");

  auto profile_pointers = {profile1.get(), profile2.get()};

  std::vector<AutofillProfile*> unique_matched_profiles =
      DeduplicatedProfilesForSuggestions(AutofillType(NAME_FIRST), {},
                                         comparator_, profile_pointers);

  ASSERT_EQ(1U, unique_matched_profiles.size());
  EXPECT_EQ(profile1.get(), unique_matched_profiles[0]);
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

  std::vector<AutofillProfile*> unique_matched_profiles =
      DeduplicatedProfilesForSuggestions(AutofillType(NAME_FIRST), {NAME_LAST},
                                         comparator_, profile_pointers);

  ASSERT_EQ(3U, unique_matched_profiles.size());

  EXPECT_THAT(unique_matched_profiles,
              ElementsAre(profile1.get(), profile2.get(), profile3.get()));
}

TEST_F(SuggestionSelectionTest, GetUniqueSuggestions_DedupeLimit) {
  // Test limit of suggestions.
  std::vector<std::unique_ptr<AutofillProfile>> profiles_data;
  for (size_t i = 0; i < kMaxUniqueSuggestedProfilesCount + 1; i++) {
    profiles_data.push_back(CreateProfileUniquePtr(
        base::StringPrintf("Bob %zu", i).c_str(), "Doe"));
  }

  // Map all the pointers into an array that has the right type.
  std::vector<AutofillProfile*> profiles_pointers;
  base::ranges::transform(profiles_data, std::back_inserter(profiles_pointers),
                          &std::unique_ptr<AutofillProfile>::get);

  std::vector<AutofillProfile*> unique_matched_profiles =
      DeduplicatedProfilesForSuggestions(AutofillType(NAME_FIRST), {NAME_LAST},
                                         comparator_, profiles_pointers);

  ASSERT_EQ(kMaxUniqueSuggestedProfilesCount, unique_matched_profiles.size());

  // All profiles are different.
  for (size_t i = 0; i < unique_matched_profiles.size(); i++) {
    ASSERT_EQ(profiles_pointers[i], unique_matched_profiles[i]);
  }
}

TEST_F(SuggestionSelectionTest, GetUniqueSuggestions_EmptyMatchingProfiles) {
  ASSERT_EQ(0U, DeduplicatedProfilesForSuggestions(AutofillType(NAME_FIRST),
                                                   {NAME_LAST}, comparator_, {})
                    .size());
}

// Tests that `kAccount` profiles are preferred over `kLocalOrSyncable` profile
// in case of a duplicate.
TEST_F(SuggestionSelectionTest, GetUniqueSuggestions_kAccount) {
  // Create two profiles that only differ by their source.
  const auto account_profile = CreateProfileUniquePtr("FirstName");
  account_profile->set_source_for_testing(AutofillProfile::Source::kAccount);
  const auto local_profile = CreateProfileUniquePtr("FirstName");
  local_profile->set_source_for_testing(
      AutofillProfile::Source::kLocalOrSyncable);
  // Place `account_profile` behind `local_profile`.
  std::vector<AutofillProfile*> profiles = {local_profile.get(),
                                            account_profile.get()};

  std::vector<AutofillProfile*> unique_matched_profiles =
      DeduplicatedProfilesForSuggestions(AutofillType(NAME_FIRST), {},
                                         comparator_, profiles);
  // Usually, duplicates are resolved in favour of the earlier profile. Expect
  // that this is not the case when profiles of different sources are involved.
  EXPECT_THAT(unique_matched_profiles, ElementsAre(account_profile.get()));
}

TEST_F(SuggestionSelectionTest, RemoveProfilesNotUsedSinceTimestamp) {
  const char kAddressesSuppressedHistogramName[] =
      "Autofill.AddressesSuppressedForDisuse";
  base::Time kCurrentTime;
  bool result =
      base::Time::FromUTCString("2017-01-02T00:00:01Z", &kCurrentTime);
  ASSERT_TRUE(result);
  constexpr size_t kNumProfiles = 10;
  constexpr base::TimeDelta k30Days = base::Days(30);
  constexpr base::TimeDelta k5DaysBuffer = base::Days(5);

  // Set up the profile vectors with last use dates ranging from |kCurrentTime|
  // to 270 days ago, in 30 day increments.  Note that the profiles are sorted
  // by decreasing last use date.
  std::vector<std::unique_ptr<AutofillProfile>> all_profile_data;
  for (size_t i = 0; i < kNumProfiles; ++i) {
    all_profile_data.push_back(std::make_unique<AutofillProfile>());
    all_profile_data[i]->set_use_date(kCurrentTime - (i * k30Days));
  }

  // Map all the pointers into an array that has the right type.
  std::vector<AutofillProfile*> all_profile_ptrs;
  base::ranges::transform(all_profile_data,
                          std::back_inserter(all_profile_ptrs),
                          &std::unique_ptr<AutofillProfile>::get);

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
        kCurrentTime + base::Days(1), &profiles);

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
        kCurrentTime - base::Days(2 * kNumProfiles * k30Days.InDays()),
        &profiles);

    // Validate that we get the expected filtered profiles and histograms.
    EXPECT_EQ(all_profile_ptrs, profiles);
    histogram_tester.ExpectTotalCount(kAddressesSuppressedHistogramName, 1);
    histogram_tester.ExpectBucketCount(kAddressesSuppressedHistogramName, 0, 1);
  }
}

}  // namespace autofill::suggestion_selection
