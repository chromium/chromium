// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/user_actions_collector.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/user_actions_store.pb.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/prefs.h"
#include "components/feed/core/v2/public/common_enums.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace feed {
namespace {

class UserActionsCollectorTest : public testing::Test {
 public:
  UserActionsCollectorTest() {
    feature_list_.InitAndEnableFeature(kPersonalizeFeedUnsignedUsers);
    feed::RegisterProfilePrefs(profile_prefs_.registry());
    user_actions_collector_ =
        std::make_unique<UserActionsCollector>(&profile_prefs_);
  }
  UserActionsCollectorTest(UserActionsCollectorTest&) = delete;
  UserActionsCollectorTest& operator=(const UserActionsCollectorTest&) = delete;
  ~UserActionsCollectorTest() override = default;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingPrefServiceSimple& profile_prefs() { return profile_prefs_; }

  base::HistogramTester& histogram() { return histogram_; }

  void ResetCollector() {
    user_actions_collector_ =
        std::make_unique<UserActionsCollector>(&profile_prefs_);
  }

  void MoveClockForwardBy(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  TestingPrefServiceSimple profile_prefs_;
  base::HistogramTester histogram_;
  std::unique_ptr<UserActionsCollector> user_actions_collector_;
};

TEST_F(UserActionsCollectorTest, AddAndRetrieveOneEntry) {
  base::HistogramTester histogram_tester;
  user_actions_collector_->UpdateUserProfileOnLinkClick(
      GURL("https://foo.com/bar"), {3, 5});
  histogram_tester.ExpectUniqueSample(
      "ContentSuggestions.Feed.UnsignedUserPersonalization.LinkClicked", 1, 1);
  ResetCollector();
  histogram_tester.ExpectUniqueSample(
      "ContentSuggestions.Feed.UnsignedUserPersonalization."
      "CountValuesDuringStoreInitialization",
      1, 1);
}

TEST_F(UserActionsCollectorTest, AddAndRetrieveOne64BitEntry) {
  base::HistogramTester histogram_tester;
  const std::vector<int64_t> mids = {3, 5, INT64_MAX, INT64_MAX - 1};
  user_actions_collector_->UpdateUserProfileOnLinkClick(
      GURL("https://foo.com/bar"), mids);
  histogram_tester.ExpectUniqueSample(
      "ContentSuggestions.Feed.UnsignedUserPersonalization.LinkClicked", 1, 1);
  ResetCollector();
  histogram_tester.ExpectUniqueSample(
      "ContentSuggestions.Feed.UnsignedUserPersonalization."
      "CountValuesDuringStoreInitialization",
      1, 1);

  const base::Value::List& list_value =
      user_actions_collector_->visit_metadata_string_list_pref_for_testing();
  ASSERT_EQ(1u, list_value.size());

  const auto& entry = list_value.front();

  std::string base64_decoded;
  ASSERT_TRUE(base::Base64Decode(entry.GetString(), &base64_decoded));
  feedunsignedpersonalizationstore::VisitMetadata entry_proto;
  ASSERT_TRUE(entry_proto.ParseFromString(base64_decoded));

  ASSERT_EQ(static_cast<int>(mids.size()), entry_proto.entity_mids().size());

  // Verify that all the mids are present in the retrieved proto.
  for (const int expected_mid : mids) {
    bool mid_found = false;
    for (const int mid : entry_proto.entity_mids()) {
      if (expected_mid == mid) {
        mid_found = true;
        break;
      }
    }
    EXPECT_TRUE(mid_found);
  }
  EXPECT_EQ("https://foo.com", entry_proto.origin());
}

// Verify that multiple entries are persisted, and extra entries are dropped.
TEST_F(UserActionsCollectorTest, AddAndRetrieveMultipleEntries) {
  base::HistogramTester histogram_tester;

  // Add more entries than the size of the store.
  const size_t count_entries = GetFeedConfig().max_url_entries_in_cache * 2;

  for (size_t i = 0; i < count_entries; ++i) {
    user_actions_collector_->UpdateUserProfileOnLinkClick(
        GURL("https://foo.com/bar"), {3, 5});
  }
  histogram_tester.ExpectUniqueSample(
      "ContentSuggestions.Feed.UnsignedUserPersonalization.LinkClicked", 1,
      count_entries);
  ResetCollector();

  // Only GetFeedConfig().max_url_entries_in_cache should be read.
  histogram_tester.ExpectUniqueSample(
      "ContentSuggestions.Feed.UnsignedUserPersonalization."
      "CountValuesDuringStoreInitialization",
      GetFeedConfig().max_url_entries_in_cache, 1);
}

// Verifu that invalid entries are not persisted.
TEST_F(UserActionsCollectorTest, InvalidEntry) {
  base::HistogramTester histogram_tester;

  // Invalid because of empty URL.
  user_actions_collector_->UpdateUserProfileOnLinkClick(GURL(""), {3, 5});

  // Invalid because MIDs are missing.
  user_actions_collector_->UpdateUserProfileOnLinkClick(
      GURL("https://foo.com/bar"), {});

  histogram_tester.ExpectTotalCount(
      "ContentSuggestions.Feed.UnsignedUserPersonalization.LinkClicked", 0);
  ResetCollector();
  histogram_tester.ExpectUniqueSample(
      "ContentSuggestions.Feed.UnsignedUserPersonalization."
      "CountValuesDuringStoreInitialization",
      0, 1);
}

// Verify that the entries that are too old are dropped.
TEST_F(UserActionsCollectorTest, TooOldEntry) {
  {
    base::HistogramTester histogram_tester;

    user_actions_collector_->UpdateUserProfileOnLinkClick(
        GURL("https://foo.com/bar"), {3, 5});
    histogram_tester.ExpectTotalCount(
        "ContentSuggestions.Feed.UnsignedUserPersonalization.LinkClicked", 1);

    ResetCollector();
    histogram_tester.ExpectUniqueSample(
        "ContentSuggestions.Feed.UnsignedUserPersonalization."
        "CountValuesDuringStoreInitialization",
        1, 1);
  }

  {
    // Entry is now 2 days old.
    base::HistogramTester histogram_tester;
    MoveClockForwardBy(base::Days(2));
    ResetCollector();
    histogram_tester.ExpectUniqueSample(
        "ContentSuggestions.Feed.UnsignedUserPersonalization."
        "CountValuesDuringStoreInitialization",
        1, 1);
  }

  {
    // Entry is now 22 days old.
    base::HistogramTester histogram_tester;
    MoveClockForwardBy(base::Days(20));
    ResetCollector();
    histogram_tester.ExpectUniqueSample(
        "ContentSuggestions.Feed.UnsignedUserPersonalization."
        "CountValuesDuringStoreInitialization",
        1, 1);
  }

  {
    // Entry is now 32 days old, and should be dropped.
    base::HistogramTester histogram_tester;
    MoveClockForwardBy(base::Days(10));
    ResetCollector();
    histogram_tester.ExpectUniqueSample(
        "ContentSuggestions.Feed.UnsignedUserPersonalization."
        "CountValuesDuringStoreInitialization",
        0, 1);
  }
}

// Verify that the list is sorted by timestamp.
TEST_F(UserActionsCollectorTest, SortedList) {
  base::HistogramTester histogram_tester;

  // Add more entries than the size of the store.
  const int count_entries = GetFeedConfig().max_url_entries_in_cache;

  // Add |count_entries| number of entries 2 times. The first set of
  // |count_entries| should be skipped when reading back the store.
  // The mids are set in increasing order and verified later when
  // reading the entries back.
  for (int i = 0; i < count_entries; ++i) {
    user_actions_collector_->UpdateUserProfileOnLinkClick(
        GURL("https://foo.com/bar"), {i});
    MoveClockForwardBy(base::Seconds(1));
  }

  for (int i = 0; i < count_entries; ++i) {
    user_actions_collector_->UpdateUserProfileOnLinkClick(
        GURL("https://foo.com/bar"), {i + count_entries});
    MoveClockForwardBy(base::Seconds(1));
  }
  histogram_tester.ExpectUniqueSample(
      "ContentSuggestions.Feed.UnsignedUserPersonalization.LinkClicked", 1,
      count_entries * 2);

  ResetCollector();
  // Only GetFeedConfig().max_url_entries_in_cache should be read.
  histogram_tester.ExpectUniqueSample(
      "ContentSuggestions.Feed.UnsignedUserPersonalization."
      "CountValuesDuringStoreInitialization",
      GetFeedConfig().max_url_entries_in_cache, 1);

  const base::Value::List& list_value =
      user_actions_collector_->visit_metadata_string_list_pref_for_testing();

  // First |count_entries| entries (with mid from 0 to |count_entries|-1) should
  // be dropped.
  long expected_mid = count_entries;
  for (const base::Value& entry : list_value) {
    std::string base64_decoded;
    ASSERT_TRUE(base::Base64Decode(entry.GetString(), &base64_decoded));
    feedunsignedpersonalizationstore::VisitMetadata entry_proto;
    ASSERT_TRUE(entry_proto.ParseFromString(base64_decoded));

    ASSERT_EQ(1, entry_proto.entity_mids().size());
    EXPECT_EQ(expected_mid, static_cast<long>(entry_proto.entity_mids().at(0)));
    ++expected_mid;
    EXPECT_EQ("https://foo.com", entry_proto.origin());
  }
}

TEST_F(UserActionsCollectorTest, LimitCountOfMidsPerUrlEntry) {
  base::HistogramTester histogram_tester;

  // Add more entries than the size of the store.
  const int max_mid_entities_per_url_entry =
      GetFeedConfig().max_mid_entities_per_url_entry;

  std::vector<int64_t> entity_mids;
  // Add |max_mid_entities_per_url_entry| number of entries 2 times.
  for (int i = 0; i < 2 * max_mid_entities_per_url_entry; ++i) {
    entity_mids.push_back(i);
  }
  user_actions_collector_->UpdateUserProfileOnLinkClick(
      GURL("https://foo.com/bar"), entity_mids);

  ResetCollector();
  histogram_tester.ExpectUniqueSample(
      "ContentSuggestions.Feed.UnsignedUserPersonalization."
      "CountValuesDuringStoreInitialization",
      1, 1);

  const base::Value::List& list_value =
      user_actions_collector_->visit_metadata_string_list_pref_for_testing();

  ASSERT_EQ(1u, list_value.size());

  const auto& entry = list_value.front();

  std::string base64_decoded;
  ASSERT_TRUE(base::Base64Decode(entry.GetString(), &base64_decoded));
  feedunsignedpersonalizationstore::VisitMetadata entry_proto;
  ASSERT_TRUE(entry_proto.ParseFromString(base64_decoded));

  ASSERT_EQ(max_mid_entities_per_url_entry, entry_proto.entity_mids().size());

  int expected_mid = 0;

  for (const int mid : entry_proto.entity_mids()) {
    EXPECT_EQ(expected_mid++, mid);
  }
  EXPECT_EQ("https://foo.com", entry_proto.origin());
}

}  // namespace
}  // namespace feed
