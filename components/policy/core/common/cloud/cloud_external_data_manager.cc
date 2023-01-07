// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_external_data_manager.h"

#include "base/strings/strcat.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "crypto/sha2.h"

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

CloudExternalDataManager::MetadataKey::MetadataKey() = default;

CloudExternalDataManager::MetadataKey::MetadataKey(const std::string& policy)
    : policy(policy) {}

CloudExternalDataManager::MetadataKey::MetadataKey(
    const std::string& policy,
    const std::string& field_name)
    : policy(policy), field_name(field_name) {}

bool CloudExternalDataManager::MetadataKey::operator<(
    const MetadataKey& other) const {
  return policy < other.policy ||
         (policy == other.policy && field_name < other.field_name);
}

// Hashing to avoid future parsing of this string
std::string CloudExternalDataManager::MetadataKey::ToString() const {
  return base::StrCat(
      {crypto::SHA256HashString(policy), crypto::SHA256HashString(field_name)});
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
