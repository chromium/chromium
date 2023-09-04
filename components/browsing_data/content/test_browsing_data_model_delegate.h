// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_TEST_BROWSING_DATA_MODEL_DELEGATE_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_TEST_BROWSING_DATA_MODEL_DELEGATE_H_

#include "components/browsing_data/content/browsing_data_model.h"

namespace browsing_data {

class TestBrowsingDataModelDelegate : public BrowsingDataModel::Delegate {
 public:
  enum class StorageType {
    kTestDelegateType = (int)BrowsingDataModel::StorageType::kLastType + 1,
    kTestDelegateTypePartitioned,
  };

  TestBrowsingDataModelDelegate();
  ~TestBrowsingDataModelDelegate() override;

  // BrowsingDataModel::Delegate:
  void GetAllDataKeys(
      base::OnceCallback<void(std::vector<DelegateEntry>)> callback) override;
  void RemoveDataKey(BrowsingDataModel::DataKey data_key,
                     BrowsingDataModel::StorageTypeSet storage_types,
                     base::OnceClosure callback) override;
  absl::optional<BrowsingDataModel::DataOwner> GetDataOwner(
      BrowsingDataModel::DataKey data_key,
      BrowsingDataModel::StorageType storage_type) const override;
  absl::optional<bool> IsBlockedByThirdPartyCookieBlocking(
      BrowsingDataModel::StorageType storage_type) const override;

 private:
  std::map<BrowsingDataModel::DataKey, BrowsingDataModel::StorageTypeSet>
      delegated_entries;
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_TEST_BROWSING_DATA_MODEL_DELEGATE_H_
