// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/browsing_data_model_test_util.h"

#include "components/browsing_data/content/browsing_data_model.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browsing_data_model_test_util {

BrowsingDataEntry::BrowsingDataEntry(
    const BrowsingDataModel::DataOwner& data_owner,
    BrowsingDataModel::DataKey data_key,
    BrowsingDataModel::DataDetails data_details)
    : data_owner(data_owner), data_key(data_key), data_details(data_details) {}

BrowsingDataEntry::BrowsingDataEntry(
    const BrowsingDataModel::BrowsingDataEntryView& view)
    : data_owner(*view.data_owner),
      data_key(*view.data_key),
      data_details(*view.data_details) {}

BrowsingDataEntry::~BrowsingDataEntry() = default;
BrowsingDataEntry::BrowsingDataEntry(const BrowsingDataEntry& other) = default;

bool BrowsingDataEntry::operator==(const BrowsingDataEntry& other) const {
  return data_owner == other.data_owner && data_key == other.data_key &&
         data_details == other.data_details;
}

void ValidateBrowsingDataEntries(
    BrowsingDataModel* model,
    const std::vector<BrowsingDataEntry>& expected_entries) {
  std::vector<BrowsingDataEntry> model_entries;

  for (const auto& entry : *model) {
    model_entries.emplace_back(entry);
  }

  EXPECT_THAT(model_entries,
              testing::UnorderedElementsAreArray(expected_entries));
}

void ValidateBrowsingDataEntriesIgnoreUsage(
    BrowsingDataModel* model,
    const std::vector<BrowsingDataEntry>& expected_entries) {
  std::vector<BrowsingDataEntry> model_entries;
  for (const auto& entry : *model) {
    model_entries.emplace_back(entry);
  }
  EXPECT_EQ(model_entries.size(), expected_entries.size());

  for (size_t i = 0; i < expected_entries.size(); i++) {
    EXPECT_EQ(expected_entries[i].data_owner, model_entries[i].data_owner);
    EXPECT_EQ(expected_entries[i].data_key, model_entries[i].data_key);
    EXPECT_EQ(expected_entries[i].data_details.storage_types,
              model_entries[i].data_details.storage_types);
    EXPECT_EQ(expected_entries[i].data_details.cookie_count,
              model_entries[i].data_details.cookie_count);
  }
}
}  // namespace browsing_data_model_test_util
