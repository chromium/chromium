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
      const std::vector<const AutofillProfile*>& profiles,
      const ServerFieldType& field_type) {
    return base::test::ToVector(
        profiles, [field_type](const AutofillProfile* profile) {
          return Suggestion(profile->GetRawInfo(field_type));
        });
  }

  const std::string app_locale_;
  const AutofillProfileComparator comparator_;
};

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
                                                              profiles);

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
                                                              profiles);

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
        k115DaysAgo, profiles_recently_used);

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
        kCurrentTime + base::Days(1), profiles);

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
        profiles);

    // Validate that we get the expected filtered profiles and histograms.
    EXPECT_EQ(all_profile_ptrs, profiles);
    histogram_tester.ExpectTotalCount(kAddressesSuppressedHistogramName, 1);
    histogram_tester.ExpectBucketCount(kAddressesSuppressedHistogramName, 0, 1);
  }
}

}  // namespace autofill::suggestion_selection
