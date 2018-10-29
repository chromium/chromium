// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/blacklist_store.h"

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_preferences::TestingPrefServiceSyncable;

namespace suggestions {

namespace {

const char kTestUrlA[] = "http://aaa.com/";
const char kTestUrlB[] = "http://bbb.com/";
const char kTestUrlC[] = "http://ccc.com/";
const char kTestUrlD[] = "http://ddd.com/";

SuggestionsProfile CreateSuggestions(std::set<std::string> urls) {
  SuggestionsProfile suggestions;
  for (auto it = urls.begin(); it != urls.end(); ++it) {
    ChromeSuggestion* suggestion = suggestions.add_suggestions();
    suggestion->set_url(*it);
  }
  suggestions.set_timestamp(123);
  return suggestions;
}

void ValidateSuggestions(const SuggestionsProfile& expected,
                         const SuggestionsProfile& actual) {
  ASSERT_EQ(expected.suggestions_size(), actual.suggestions_size());
  for (int i = 0; i < expected.suggestions_size(); ++i) {
    EXPECT_EQ(expected.suggestions(i).url(), actual.suggestions(i).url());
    EXPECT_EQ(expected.suggestions(i).title(), actual.suggestions(i).title());
    EXPECT_EQ(expected.suggestions(i).favicon_url(),
              actual.suggestions(i).favicon_url());
    EXPECT_EQ(expected.suggestions(i).thumbnail(),
              actual.suggestions(i).thumbnail());
  }
}

}  // namespace

class BlacklistStoreTest : public testing::Test {
 public:
  BlacklistStoreTest()
      : pref_service_(new sync_preferences::TestingPrefServiceSyncable) {}

  void SetUp() override {
    BlacklistStore::RegisterProfilePrefs(pref_service()->registry());
  }

  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return pref_service_.get();
  }

 private:
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;

  DISALLOW_COPY_AND_ASSIGN(BlacklistStoreTest);
};

// Tests adding, removing to the blacklist and filtering.
TEST_F(BlacklistStoreTest, BasicInteractions) {
  BlacklistStore blacklist_store(pref_service());

  // Create suggestions with A, B and C. C and D will be added to the blacklist.
  std::set<std::string> suggested_urls;
  suggested_urls.insert(kTestUrlA);
  suggested_urls.insert(kTestUrlB);
  const SuggestionsProfile suggestions_filtered =
      CreateSuggestions(suggested_urls);
  suggested_urls.insert(kTestUrlC);
  const SuggestionsProfile original_suggestions =
      CreateSuggestions(suggested_urls);
  SuggestionsProfile suggestions;

  // Filter with an empty blacklist.
  suggestions.CopyFrom(original_suggestions);
  blacklist_store.FilterSuggestions(&suggestions);
  ValidateSuggestions(original_suggestions, suggestions);

  // Add C and D to the blacklist and filter.
  suggestions.CopyFrom(original_suggestions);
  EXPECT_TRUE(blacklist_store.BlacklistUrl(GURL(kTestUrlC)));
  EXPECT_TRUE(blacklist_store.BlacklistUrl(GURL(kTestUrlD)));
  blacklist_store.FilterSuggestions(&suggestions);
  ValidateSuggestions(suggestions_filtered, suggestions);

  // Remove C from the blacklist and filter.
  suggestions.CopyFrom(original_suggestions);
  EXPECT_TRUE(blacklist_store.RemoveUrl(GURL(kTestUrlC)));
  blacklist_store.FilterSuggestions(&suggestions);
  ValidateSuggestions(original_suggestions, suggestions);
}

TEST_F(BlacklistStoreTest, BlacklistTwiceSuceeds) {
  BlacklistStore blacklist_store(pref_service());
  EXPECT_TRUE(blacklist_store.BlacklistUrl(GURL(kTestUrlA)));
  EXPECT_TRUE(blacklist_store.BlacklistUrl(GURL(kTestUrlA)));
}

TEST_F(BlacklistStoreTest, RemoveUnknownUrlFails) {
  BlacklistStore blacklist_store(pref_service());
  EXPECT_FALSE(blacklist_store.RemoveUrl(GURL(kTestUrlA)));
}

TEST_F(BlacklistStoreTest, TestGetTimeUntilReadyForUpload) {
  // Tests assumes completion within 1 hour.
  base::TimeDelta upload_delay = base::TimeDelta::FromHours(1);
  base::TimeDelta no_delay = base::TimeDelta::FromHours(0);
  std::unique_ptr<BlacklistStore> blacklist_store(
      new BlacklistStore(pref_service(), upload_delay));
  base::TimeDelta candidate_delta;

  // Blacklist is empty.
  EXPECT_FALSE(blacklist_store->GetTimeUntilReadyForUpload(&candidate_delta));
  EXPECT_FALSE(blacklist_store->GetTimeUntilURLReadyForUpload(
      GURL(kTestUrlA), &candidate_delta));

  // Blacklist contains kTestUrlA.
  EXPECT_TRUE(blacklist_store->BlacklistUrl(GURL(kTestUrlA)));
  candidate_delta = upload_delay + base::TimeDelta::FromDays(1);
  EXPECT_TRUE(blacklist_store->GetTimeUntilReadyForUpload(&candidate_delta));
  EXPECT_LE(candidate_delta, upload_delay);
  EXPECT_GE(candidate_delta, no_delay);
  candidate_delta = upload_delay + base::TimeDelta::FromDays(1);
  EXPECT_TRUE(blacklist_store->GetTimeUntilURLReadyForUpload(
      GURL(kTestUrlA), &candidate_delta));
  EXPECT_LE(candidate_delta, upload_delay);
  EXPECT_GE(candidate_delta, no_delay);
  EXPECT_FALSE(blacklist_store->GetTimeUntilURLReadyForUpload(
    GURL(kTestUrlB), &candidate_delta));

  // There should be no candidate for upload since the upload delay is 1 day.
  // Note: this is a test that relies on timing.
  GURL retrieved;
  EXPECT_FALSE(blacklist_store->GetCandidateForUpload(&retrieved));

  // Same, but with an upload delay of 0.
  blacklist_store.reset(new BlacklistStore(pref_service(), no_delay));
  EXPECT_TRUE(blacklist_store->BlacklistUrl(GURL(kTestUrlA)));
  candidate_delta = no_delay + base::TimeDelta::FromDays(1);
  EXPECT_TRUE(blacklist_store->GetTimeUntilReadyForUpload(&candidate_delta));
  EXPECT_EQ(candidate_delta, no_delay);
  candidate_delta = no_delay + base::TimeDelta::FromDays(1);
  EXPECT_TRUE(blacklist_store->GetTimeUntilURLReadyForUpload(
      GURL(kTestUrlA), &candidate_delta));
  EXPECT_EQ(candidate_delta, no_delay);
}

TEST_F(BlacklistStoreTest, GetCandidateForUpload) {
  BlacklistStore blacklist_store(pref_service(), base::TimeDelta::FromDays(0));
  // Empty blacklist.
  GURL retrieved;
  EXPECT_FALSE(blacklist_store.GetCandidateForUpload(&retrieved));

  // Blacklist A and B. Expect to retrieve A or B.
  EXPECT_TRUE(blacklist_store.BlacklistUrl(GURL(kTestUrlA)));
  EXPECT_TRUE(blacklist_store.BlacklistUrl(GURL(kTestUrlB)));
  EXPECT_TRUE(blacklist_store.GetCandidateForUpload(&retrieved));
  std::string retrieved_string = retrieved.spec();
  EXPECT_TRUE(retrieved_string == std::string(kTestUrlA) ||
              retrieved_string == std::string(kTestUrlB));
}

TEST_F(BlacklistStoreTest, LogsBlacklistSize) {
  base::HistogramTester histogram_tester;

  // Create a first store - blacklist is empty at this point.
  std::unique_ptr<BlacklistStore> blacklist_store(
      new BlacklistStore(pref_service()));
  histogram_tester.ExpectTotalCount("Suggestions.LocalBlacklistSize", 1);
  histogram_tester.ExpectUniqueSample("Suggestions.LocalBlacklistSize", 0, 1);

  // Add some content to the blacklist.
  EXPECT_TRUE(blacklist_store->BlacklistUrl(GURL(kTestUrlA)));
  EXPECT_TRUE(blacklist_store->BlacklistUrl(GURL(kTestUrlB)));

  // Create a new BlacklistStore and verify the counts.
  blacklist_store.reset(new BlacklistStore(pref_service()));
  histogram_tester.ExpectTotalCount("Suggestions.LocalBlacklistSize", 2);
  histogram_tester.ExpectBucketCount("Suggestions.LocalBlacklistSize", 0, 1);
  histogram_tester.ExpectBucketCount("Suggestions.LocalBlacklistSize", 2, 1);
}

}  // namespace suggestions
