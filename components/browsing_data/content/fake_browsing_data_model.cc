// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/fake_browsing_data_model.h"

FakeBrowsingDataModel::FakeBrowsingDataModel()
    : FakeBrowsingDataModel(/*delegate=*/nullptr) {}
FakeBrowsingDataModel::FakeBrowsingDataModel(
    std::unique_ptr<BrowsingDataModel::Delegate> delegate)
    : BrowsingDataModel(nullptr, std::move(delegate)) {}
FakeBrowsingDataModel::~FakeBrowsingDataModel() = default;

void FakeBrowsingDataModel::RemoveBrowsingData(const DataOwner& data_owner,
                                               base::OnceClosure completed) {
  browsing_data_entries_.erase(data_owner);
}

void FakeBrowsingDataModel::RemoveBrowsingDataEntriesFromDisk(
    const DataKeyEntries& browsing_data_entries,
    base::OnceClosure completed) {
  // Fake browsing data model only works with in-memory entries.
  std::move(completed).Run();
}

void FakeBrowsingDataModel::PopulateFromDisk(
    base::OnceClosure finished_callback) {
  NOTREACHED_IN_MIGRATION();
}
