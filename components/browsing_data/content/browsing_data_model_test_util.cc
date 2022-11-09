// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/browsing_data_model_test_util.h"

#include "components/browsing_data/content/browsing_data_model.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browsing_data_model_test_util {

BrowsingDataEntry::BrowsingDataEntry(
    const std::string& primary_host,
    BrowsingDataModel::DataKey data_key,
    BrowsingDataModel::DataDetails data_details)
    : primary_host(primary_host),
      data_key(data_key),
      data_details(data_details) {}

BrowsingDataEntry::BrowsingDataEntry(
    const BrowsingDataModel::BrowsingDataEntryView& view)
    : primary_host(*view.primary_host),
      data_key(*view.data_key),
      data_details(*view.data_details) {}

BrowsingDataEntry::~BrowsingDataEntry() = default;
BrowsingDataEntry::BrowsingDataEntry(const BrowsingDataEntry& other) = default;

bool BrowsingDataEntry::operator==(const BrowsingDataEntry& other) const {
  return primary_host == other.primary_host && data_key == other.data_key &&
         data_details == other.data_details;
}

void ValidateBrowsingDataEntries(
    BrowsingDataModel* model,
    const std::vector<BrowsingDataEntry>& expected_entries) {
  std::vector<BrowsingDataEntry> model_entries;

  for (const auto& entry : *model)
    model_entries.emplace_back(entry);

  EXPECT_THAT(expected_entries,
              testing::UnorderedElementsAreArray(model_entries));
}

}  // namespace browsing_data_model_test_util
