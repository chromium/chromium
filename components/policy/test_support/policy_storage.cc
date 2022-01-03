// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/policy_storage.h"
#include "base/big_endian.h"
#include "crypto/sha2.h"

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

void PolicyStorage::SetPsmEntry(const std::string& brand_serial_id,
                                const PolicyStorage::PsmEntry& psm_entry) {
  psm_entries_[brand_serial_id] = psm_entry;
}

const PolicyStorage::PsmEntry* PolicyStorage::GetPsmEntry(
    const std::string& brand_serial_id) const {
  auto it = psm_entries_.find(brand_serial_id);
  if (it == psm_entries_.end())
    return nullptr;
  return &it->second;
}

void PolicyStorage::SetInitialEnrollmentState(
    const std::string& brand_serial_id,
    const PolicyStorage::InitialEnrollmentState& initial_enrollment_state) {
  initial_enrollment_states_[brand_serial_id] = initial_enrollment_state;
}

const PolicyStorage::InitialEnrollmentState*
PolicyStorage::GetInitialEnrollmentState(
    const std::string& brand_serial_id) const {
  auto it = initial_enrollment_states_.find(brand_serial_id);
  if (it == initial_enrollment_states_.end())
    return nullptr;
  return &it->second;
}

std::vector<std::string> PolicyStorage::GetMatchingSerialHashes(
    uint64_t modulus,
    uint64_t remainder) const {
  std::vector<std::string> hashes;
  for (const auto& [serial, enrollment_state] : initial_enrollment_states_) {
    uint64_t hash = 0UL;
    uint8_t hash_bytes[sizeof(hash)];
    crypto::SHA256HashString(serial, hash_bytes, sizeof(hash));
    base::ReadBigEndian(hash_bytes, &hash);
    if (hash % modulus == remainder) {
      hashes.emplace_back(reinterpret_cast<const char*>(hash_bytes),
                          sizeof(hash));
    }
  }
  return hashes;
}

}  // namespace policy
