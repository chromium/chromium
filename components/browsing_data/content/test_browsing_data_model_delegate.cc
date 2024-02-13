// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/test_browsing_data_model_delegate.h"

namespace browsing_data {
TestBrowsingDataModelDelegate::TestBrowsingDataModelDelegate() = default;
TestBrowsingDataModelDelegate::~TestBrowsingDataModelDelegate() = default;

void TestBrowsingDataModelDelegate::GetAllDataKeys(
    base::OnceCallback<void(std::vector<DelegateEntry>)> callback) {
  auto testOrigin = url::Origin::Create(GURL("https://a.test"));
  std::vector<DelegateEntry> data_keys = {
      DelegateEntry(testOrigin,
                    static_cast<BrowsingDataModel::StorageType>(
                        StorageType::kTestDelegateType),
                    0)};
  delegated_entries.insert({testOrigin,
                            {static_cast<BrowsingDataModel::StorageType>(
                                StorageType::kTestDelegateType)}});
  std::move(callback).Run(data_keys);
}

void TestBrowsingDataModelDelegate::RemoveDataKey(
    const BrowsingDataModel::DataKey& data_key,
    BrowsingDataModel::StorageTypeSet storage_types,
    base::OnceClosure callback) {
  if (delegated_entries.contains(data_key)) {
    DCHECK(storage_types.Has(static_cast<BrowsingDataModel::StorageType>(
        StorageType::kTestDelegateType)));
    delegated_entries.erase(data_key);
  }
  std::move(callback).Run();
}

std::optional<BrowsingDataModel::DataOwner>
TestBrowsingDataModelDelegate::GetDataOwner(
    const BrowsingDataModel::DataKey& data_key,
    BrowsingDataModel::StorageType storage_type) const {
  if (static_cast<StorageType>(storage_type) ==
          StorageType::kTestDelegateType &&
      absl::holds_alternative<url::Origin>(data_key)) {
    return absl::get<url::Origin>(data_key).host();
  }
  return std::nullopt;
}

std::optional<bool> TestBrowsingDataModelDelegate::IsStorageTypeCookieLike(
    BrowsingDataModel::StorageType storage_type) const {
  switch (
      static_cast<TestBrowsingDataModelDelegate::StorageType>(storage_type)) {
    case StorageType::kTestDelegateType:
      return true;
    case StorageType::kTestDelegateTypePartitioned:
      return false;
    default:
      return std::nullopt;
  }
}

std::optional<bool>
TestBrowsingDataModelDelegate::IsBlockedByThirdPartyCookieBlocking(
    const BrowsingDataModel::DataKey& data_key,
    BrowsingDataModel::StorageType storage_type) const {
  return IsStorageTypeCookieLike(storage_type);
}

bool TestBrowsingDataModelDelegate::IsCookieDeletionDisabled(const GURL& url) {
  return false;
}

base::WeakPtr<BrowsingDataModel::Delegate>
TestBrowsingDataModelDelegate::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace browsing_data
