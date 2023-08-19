// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_MODEL_TEST_UTIL_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_MODEL_TEST_UTIL_H_

#include "components/browsing_data/content/browsing_data_model.h"

namespace browsing_data_model_test_util {

// For testing purposes, it's useful to have a non-view version of a
// BrowsingDataEntry, so they can be put in vectors etc.
struct BrowsingDataEntry {
  BrowsingDataEntry(const BrowsingDataModel::DataOwner& data_owner,
                    BrowsingDataModel::DataKey data_key,
                    BrowsingDataModel::DataDetails data_details);
  explicit BrowsingDataEntry(
      const BrowsingDataModel::BrowsingDataEntryView& view);
  ~BrowsingDataEntry();
  BrowsingDataEntry(const BrowsingDataEntry& other);
  bool operator==(const BrowsingDataEntry& other) const;

  std::string ToDebugString() const;

  BrowsingDataModel::DataOwner data_owner;
  BrowsingDataModel::DataKey data_key;
  BrowsingDataModel::DataDetails data_details;
};

// Check that the entries returned by `model` are a permutation of those in
// `expected_entries`, e.g. lists are equal without considering order.
void ValidateBrowsingDataEntries(
    BrowsingDataModel* model,
    const std::vector<BrowsingDataEntry>& expected_entries);

// Check that the entries returned by `model` are matching `expected_entries`,
// i.e. lists are equal and storage size is more than 0.
void ValidateBrowsingDataEntriesNonZeroUsage(
    BrowsingDataModel* model,
    const std::vector<BrowsingDataEntry>& expected_entries);

}  // namespace browsing_data_model_test_util

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_BROWSING_DATA_MODEL_TEST_UTIL_H_
