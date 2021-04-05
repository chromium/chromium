// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/suggestions_store.h"

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_preferences::TestingPrefServiceSyncable;

namespace suggestions {

namespace {

const char kTestTitle[] = "Foo site";
const char kTestUrl[] = "http://foo.com/";

void AddSuggestion(SuggestionsProfile* suggestions,
                   const char* title,
                   const char* url,
                   int64_t expiry_ts) {
  ChromeSuggestion* suggestion = suggestions->add_suggestions();
  suggestion->set_url(title);
  suggestion->set_title(url);
  suggestion->set_expiry_ts(expiry_ts);
}

SuggestionsProfile CreateTestSuggestions() {
  SuggestionsProfile suggestions;
  suggestions.set_timestamp(123);
  ChromeSuggestion* suggestion = suggestions.add_suggestions();
  suggestion->set_url(kTestTitle);
  suggestion->set_title(kTestUrl);
  return suggestions;
}

SuggestionsProfile CreateTestSuggestionsProfileWithExpiry(
    base::Time current_time,
    int expired_count,
    int valid_count) {
  int64_t current_time_usec =
      (current_time - base::Time::UnixEpoch()).ToInternalValue();
  int64_t offset_usec = 5 * base::Time::kMicrosecondsPerMinute;

  SuggestionsProfile suggestions;
  for (int i = 1; i <= valid_count; i++)
    AddSuggestion(&suggestions, kTestTitle, kTestUrl,
                  current_time_usec + offset_usec * i);
  for (int i = 1; i <= expired_count; i++)
    AddSuggestion(&suggestions, kTestTitle, kTestUrl,
                  current_time_usec - offset_usec * i);

  return suggestions;
}

void ValidateSuggestions(const SuggestionsProfile& expected,
                         const SuggestionsProfile& actual) {
  EXPECT_EQ(expected.suggestions_size(), actual.suggestions_size());
  for (int i = 0; i < expected.suggestions_size(); ++i) {
    EXPECT_EQ(expected.suggestions(i).url(), actual.suggestions(i).url());
    EXPECT_EQ(expected.suggestions(i).title(), actual.suggestions(i).title());
    EXPECT_EQ(expected.suggestions(i).expiry_ts(),
              actual.suggestions(i).expiry_ts());
    EXPECT_EQ(expected.suggestions(i).favicon_url(),
              actual.suggestions(i).favicon_url());
  }
}

}  // namespace

class SuggestionsStoreTest : public testing::Test {
 public:
  SuggestionsStoreTest()
      : pref_service_(new sync_preferences::TestingPrefServiceSyncable) {}

  void SetUp() override {
    SuggestionsStore::RegisterProfilePrefs(pref_service_->registry());
    suggestions_store_ =
        std::make_unique<SuggestionsStore>(pref_service_.get());

    test_clock_.SetNow(base::Time::FromInternalValue(13063394337546738));
    suggestions_store_->SetClockForTesting(&test_clock_);
  }

 protected:
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<SuggestionsStore> suggestions_store_;
  base::SimpleTestClock test_clock_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsStoreTest);
};

// Tests LoadSuggestions function to filter expired suggestions.
TEST_F(SuggestionsStoreTest, LoadAllExpired) {
  SuggestionsProfile suggestions =
      CreateTestSuggestionsProfileWithExpiry(test_clock_.Now(), 5, 0);
  SuggestionsProfile filtered_suggestions;

  // Store and load. Expired suggestions should not be loaded.
  EXPECT_TRUE(suggestions_store_->StoreSuggestions(suggestions));
  EXPECT_FALSE(suggestions_store_->LoadSuggestions(&filtered_suggestions));
  EXPECT_EQ(0, filtered_suggestions.suggestions_size());
}

// Tests LoadSuggestions function to filter expired suggestions.
TEST_F(SuggestionsStoreTest, LoadValidAndExpired) {
  SuggestionsProfile suggestions =
      CreateTestSuggestionsProfileWithExpiry(test_clock_.Now(), 5, 3);
  SuggestionsProfile filtered_suggestions;

  // Store and load. Expired suggestions should not be loaded.
  EXPECT_TRUE(suggestions_store_->StoreSuggestions(suggestions));
  EXPECT_TRUE(suggestions_store_->LoadSuggestions(&filtered_suggestions));
  EXPECT_EQ(3, filtered_suggestions.suggestions_size());
}

// Tests LoadSuggestions function to filter expired suggestions.
TEST_F(SuggestionsStoreTest, CheckStoreAfterLoadExpired) {
  SuggestionsProfile suggestions =
      CreateTestSuggestionsProfileWithExpiry(test_clock_.Now(), 5, 3);
  SuggestionsProfile filtered_suggestions;

  // Store and load. Expired suggestions should not be loaded.
  EXPECT_TRUE(suggestions_store_->StoreSuggestions(suggestions));
  EXPECT_TRUE(suggestions_store_->LoadSuggestions(&filtered_suggestions));

  SuggestionsProfile loaded_suggestions;
  EXPECT_TRUE(suggestions_store_->LoadSuggestions(&loaded_suggestions));
  EXPECT_EQ(3, loaded_suggestions.suggestions_size());
  ValidateSuggestions(filtered_suggestions, loaded_suggestions);
}

TEST_F(SuggestionsStoreTest, LoadStoreClear) {
  const SuggestionsProfile suggestions = CreateTestSuggestions();
  const SuggestionsProfile empty_suggestions;
  SuggestionsProfile recovered_suggestions;

  // Attempt to load when prefs are empty.
  EXPECT_FALSE(suggestions_store_->LoadSuggestions(&recovered_suggestions));
  ValidateSuggestions(empty_suggestions, recovered_suggestions);

  // Store then reload.
  EXPECT_TRUE(suggestions_store_->StoreSuggestions(suggestions));
  EXPECT_TRUE(suggestions_store_->LoadSuggestions(&recovered_suggestions));
  ValidateSuggestions(suggestions, recovered_suggestions);

  // Clear.
  suggestions_store_->ClearSuggestions();
  EXPECT_FALSE(suggestions_store_->LoadSuggestions(&recovered_suggestions));
  ValidateSuggestions(empty_suggestions, recovered_suggestions);
}

}  // namespace suggestions
