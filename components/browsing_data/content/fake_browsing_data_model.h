// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback_forward.h"
#include "components/browsing_data/content/browsing_data_model.h"

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_FAKE_BROWSING_DATA_MODEL_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_FAKE_BROWSING_DATA_MODEL_H_

// An in-memory only version of the BrowsingDataModel. Uses real
// BrowsingDataModel logic for updating internal in-memory state, but overrides
// any function which would attempt to fetch from disk.
class FakeBrowsingDataModel : public BrowsingDataModel {
 public:
  FakeBrowsingDataModel();
  explicit FakeBrowsingDataModel(
      std::unique_ptr<BrowsingDataModel::Delegate> delegate);
  ~FakeBrowsingDataModel() override;

  void RemoveBrowsingData(const DataOwner& data_owner,
                          base::OnceClosure completed) override;

  void RemoveBrowsingDataEntriesFromDisk(
      const DataKeyEntries& browsing_data_entries,
      base::OnceClosure completed) override;

 private:
  void PopulateFromDisk(base::OnceClosure finished_callback) override;
};

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_FAKE_BROWSING_DATA_MODEL_H_
