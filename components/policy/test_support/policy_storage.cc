// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/policy_storage.h"

namespace policy {

PolicyStorage::PolicyStorage()
    : signature_provider_(std::make_unique<SignatureProvider>()) {}

PolicyStorage::PolicyStorage(PolicyStorage&& policy_storage) = default;

PolicyStorage& PolicyStorage::operator=(PolicyStorage&& policy_storage) =
    default;

PolicyStorage::~PolicyStorage() = default;

std::string PolicyStorage::GetPolicyPayload(
    const std::string& policy_type) const {
  auto it = policy_payloads_.find(policy_type);
  return it == policy_payloads_.end() ? std::string() : it->second;
}

void PolicyStorage::SetPolicyPayload(const std::string& policy_type,
                                     const std::string& policy_payload) {
  policy_payloads_[policy_type] = policy_payload;
}

}  // namespace policy
