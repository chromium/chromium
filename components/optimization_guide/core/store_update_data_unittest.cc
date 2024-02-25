// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/store_update_data.h"

#include <string>

#include "base/time/time.h"
#include "base/version.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/hint_cache.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

TEST(StoreUpdateDataTest, BuildComponentStoreUpdateData) {
  // Verify creating a Component Hint update package.
  base::Version v1("1.2.3.4");
  proto::Hint hint1;
  hint1.set_key("foo.org");
  hint1.set_key_representation(proto::HOST);
  proto::PageHint* page_hint1 = hint1.add_page_hints();
  page_hint1->set_page_pattern("slowpage");
  proto::Hint hint2;
  hint2.set_key("bar.com");
  hint2.set_key_representation(proto::HOST);
  proto::PageHint* page_hint2 = hint2.add_page_hints();
  page_hint2->set_page_pattern("slowpagealso");

  std::unique_ptr<StoreUpdateData> component_update =
      StoreUpdateData::CreateComponentStoreUpdateData(v1);
  component_update->MoveHintIntoUpdateData(std::move(hint1));
  component_update->MoveHintIntoUpdateData(std::move(hint2));
  EXPECT_TRUE(component_update->component_version().has_value());
  EXPECT_FALSE(component_update->update_time().has_value());
  EXPECT_EQ(v1, *component_update->component_version());
  // Verify there are 3 store entries: 1 for the metadata entry plus
  // the 2 added hint entries.
  EXPECT_EQ(3ul, component_update->TakeUpdateEntries()->size());
}

TEST(StoreUpdateDataTest, BuildFetchUpdateDataUsesDefaultCacheDuration) {
  // Verify creating a Fetched Hint update package.
  base::Time update_time = base::Time::Now();
  proto::Hint hint1;
  hint1.set_key("foo.org");
  hint1.set_key_representation(proto::HOST);
  proto::PageHint* page_hint1 = hint1.add_page_hints();
  page_hint1->set_page_pattern("slowpage");

  std::unique_ptr<StoreUpdateData> fetch_update =
      StoreUpdateData::CreateFetchedStoreUpdateData(update_time);
  fetch_update->MoveHintIntoUpdateData(std::move(hint1));
  EXPECT_FALSE(fetch_update->component_version().has_value());
  EXPECT_TRUE(fetch_update->update_time().has_value());
  EXPECT_EQ(update_time, *fetch_update->update_time());
  // Verify there are 2 store entries: 1 for the metadata entry plus
  // the 1 added hint entries.
  const auto update_entries = fetch_update->TakeUpdateEntries();
  EXPECT_EQ(2ul, update_entries->size());
  // Verify expiry time taken from hint rather than the default expiry time of
  // the store update data.
  for (const auto& entry : *update_entries) {
    proto::StoreEntry store_entry = entry.second;
    if (store_entry.entry_type() == proto::FETCHED_HINT) {
      base::Time expected_expiry_time =
          base::Time::Now() + features::StoredFetchedHintsFreshnessDuration();
      EXPECT_EQ(expected_expiry_time.ToDeltaSinceWindowsEpoch().InSeconds(),
                store_entry.expiry_time_secs());
      break;
    }
  }
}

TEST(StoreUpdateDataTest,
     BuildFetchUpdateDataUsesCacheDurationFromHintIfAvailable) {
  // Verify creating a Fetched Hint update package.
  int max_cache_duration_secs = 60;
  base::Time update_time = base::Time::Now();
  proto::Hint hint1;
  hint1.set_key("foo.org");
  hint1.set_key_representation(proto::HOST);
  hint1.mutable_max_cache_duration()->set_seconds(max_cache_duration_secs);
  proto::PageHint* page_hint1 = hint1.add_page_hints();
  page_hint1->set_page_pattern("slowpage");

  std::unique_ptr<StoreUpdateData> fetch_update =
      StoreUpdateData::CreateFetchedStoreUpdateData(update_time);
  fetch_update->MoveHintIntoUpdateData(std::move(hint1));
  EXPECT_FALSE(fetch_update->component_version().has_value());
  EXPECT_TRUE(fetch_update->update_time().has_value());
  EXPECT_EQ(update_time, *fetch_update->update_time());
  // Verify there are 2 store entries: 1 for the metadata entry plus
  // the 1 added hint entries.
  const auto update_entries = fetch_update->TakeUpdateEntries();
  EXPECT_EQ(2ul, update_entries->size());
  // Verify expiry time taken from hint rather than the default expiry time of
  // the store update data.
  for (const auto& entry : *update_entries) {
    proto::StoreEntry store_entry = entry.second;
    if (store_entry.entry_type() == proto::FETCHED_HINT) {
      base::Time expected_expiry_time =
          base::Time::Now() + base::Seconds(max_cache_duration_secs);
      EXPECT_EQ(expected_expiry_time.ToDeltaSinceWindowsEpoch().InSeconds(),
                store_entry.expiry_time_secs());
      break;
    }
  }
}

}  // namespace

}  // namespace optimization_guide
