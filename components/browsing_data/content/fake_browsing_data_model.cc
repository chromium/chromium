// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/fake_browsing_data_model.h"

FakeBrowsingDataModel::FakeBrowsingDataModel() : BrowsingDataModel(nullptr) {}
FakeBrowsingDataModel::~FakeBrowsingDataModel() = default;

void FakeBrowsingDataModel::RemoveBrowsingData(const std::string& primary_host,
                                               base::OnceClosure completed) {
  browsing_data_entries_.erase(primary_host);
}

void FakeBrowsingDataModel::PopulateFromDisk(
    base::OnceClosure finished_callback) {
  NOTREACHED();
}
