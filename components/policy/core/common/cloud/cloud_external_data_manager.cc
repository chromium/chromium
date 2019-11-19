// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_external_data_manager.h"

#include "components/policy/core/common/cloud/cloud_policy_store.h"

namespace policy {

CloudExternalDataManager::MetadataEntry::MetadataEntry() {
}

CloudExternalDataManager::MetadataEntry::MetadataEntry(const std::string& url,
                                                       const std::string& hash)
    : url(url),
      hash(hash) {
}

bool CloudExternalDataManager::MetadataEntry::operator!=(
    const MetadataEntry& other) const {
  return url != other.url || hash != other.hash;
}

CloudExternalDataManager::CloudExternalDataManager() : policy_store_(nullptr) {}

CloudExternalDataManager::~CloudExternalDataManager() {
}

void CloudExternalDataManager::SetPolicyStore(CloudPolicyStore* policy_store) {
  weak_factory_.InvalidateWeakPtrs();
  policy_store_ = policy_store;
  if (policy_store_)
    policy_store_->SetExternalDataManager(weak_factory_.GetWeakPtr());
}

}  // namespace policy
