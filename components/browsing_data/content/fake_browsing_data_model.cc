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

void FakeBrowsingDataModel::RemovePartitionedBrowsingData(
    const DataOwner& data_owner,
    const net::SchemefulSite& top_level_site,
    base::OnceClosure /*completed*/) {
  // This duplicates the in-memory deletion code used in the real model.
  // TODO(crbug.com/1484482): Separate in-memory & on-disk deletion logic,
  // and have the fake use the real in-memory logic.
  DataKeyEntries affected_data_key_entries;

  GetAffectedDataKeyEntriesForRemovePartitionedBrowsingData(
      data_owner, top_level_site, affected_data_key_entries);

  auto& data_owner_entries = browsing_data_entries_[data_owner];
  for (auto& entry : affected_data_key_entries) {
    data_owner_entries.erase(entry.first);
  }
  if (data_owner_entries.empty()) {
    browsing_data_entries_.erase(data_owner);
  }
}

void FakeBrowsingDataModel::RemoveUnpartitionedBrowsingData(
    const DataOwner& data_owner,
    base::OnceClosure completed) {
  // This duplicates the in-memory deletion code used in the real model.
  // TODO(crbug.com/1484482): Separate in-memory & on-disk deletion logic,
  // and have the fake use the real in-memory logic.
  DataKeyEntries affected_data_key_entries;
  for (const auto& entry : browsing_data_entries_[data_owner]) {
    auto* storage_key = absl::get_if<blink::StorageKey>(&entry.first);
    if (!storage_key) {
      affected_data_key_entries.insert(entry);
      continue;
    }

    if (storage_key->IsFirstPartyContext()) {
      affected_data_key_entries.insert(entry);
      continue;
    }
  }

  auto& data_owner_entries = browsing_data_entries_[data_owner];
  for (auto& entry : affected_data_key_entries) {
    data_owner_entries.erase(entry.first);
  }
  if (data_owner_entries.empty()) {
    browsing_data_entries_.erase(data_owner);
  }
}

void FakeBrowsingDataModel::PopulateFromDisk(
    base::OnceClosure finished_callback) {
  NOTREACHED();
}
