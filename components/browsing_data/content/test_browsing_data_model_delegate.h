// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_TEST_BROWSING_DATA_MODEL_DELEGATE_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_TEST_BROWSING_DATA_MODEL_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "components/browsing_data/content/browsing_data_model.h"

namespace browsing_data {

class TestBrowsingDataModelDelegate final : public BrowsingDataModel::Delegate {
 public:
  enum class StorageType {
    kTestDelegateType =
        static_cast<int>(BrowsingDataModel::StorageType::kLastType) + 1,
    kTestDelegateTypePartitioned,
  };

  TestBrowsingDataModelDelegate();
  ~TestBrowsingDataModelDelegate() override;

  // BrowsingDataModel::Delegate:
  void GetAllDataKeys(
      base::OnceCallback<void(std::vector<DelegateEntry>)> callback) override;
  void RemoveDataKey(const BrowsingDataModel::DataKey& data_key,
                     BrowsingDataModel::StorageTypeSet storage_types,
                     base::OnceClosure callback) override;
  std::optional<BrowsingDataModel::DataOwner> GetDataOwner(
      const BrowsingDataModel::DataKey& data_key,
      BrowsingDataModel::StorageType storage_type) const override;
  std::optional<bool> IsStorageTypeCookieLike(
      BrowsingDataModel::StorageType storage_type) const override;
  std::optional<bool> IsBlockedByThirdPartyCookieBlocking(
      const BrowsingDataModel::DataKey& data_key,
      BrowsingDataModel::StorageType storage_type) const override;
  bool IsCookieDeletionDisabled(const GURL& url) override;
  base::WeakPtr<BrowsingDataModel::Delegate> AsWeakPtr() override;

 private:
  std::map<BrowsingDataModel::DataKey, BrowsingDataModel::StorageTypeSet>
      delegated_entries;
  base::WeakPtrFactory<TestBrowsingDataModelDelegate> weak_ptr_factory_{this};
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_TEST_BROWSING_DATA_MODEL_DELEGATE_H_
