// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/policy_storage.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "crypto/sha2.h"

namespace policy {

namespace {

const char kPolicyKeySeparator[] = "/";

std::string GetPolicyKey(const std::string& policy_type,
                         const std::string& entity_id) {
  if (entity_id.empty())
    return policy_type;
  return base::StrCat({policy_type, kPolicyKeySeparator, entity_id});
}

}  // namespace

PolicyStorage::PolicyStorage()
    : signature_provider_(std::make_unique<SignatureProvider>()) {}

PolicyStorage::PolicyStorage(PolicyStorage&& policy_storage) = default;

PolicyStorage& PolicyStorage::operator=(PolicyStorage&& policy_storage) =
    default;

PolicyStorage::~PolicyStorage() = default;

std::string PolicyStorage::GetPolicyPayload(
    const std::string& policy_type,
    const std::string& entity_id) const {
  auto it = policy_payloads_.find(GetPolicyKey(policy_type, entity_id));
  return it == policy_payloads_.end() ? std::string() : it->second;
}

std::vector<std::string> PolicyStorage::GetEntityIdsForType(
    const std::string& policy_type) {
  std::string prefix = base::StrCat({policy_type, kPolicyKeySeparator});
  std::vector<std::string> ids;
  const size_t prefix_length = prefix.length();
  for (const auto& [policy_key, payload] : policy_payloads_) {
    if (base::StartsWith(policy_key, prefix))
      ids.push_back(policy_key.substr(prefix_length));
  }
  return ids;
}

void PolicyStorage::SetPolicyPayload(const std::string& policy_type,
                                     const std::string& policy_payload) {
  SetPolicyPayload(policy_type, std::string(), policy_payload);
}

void PolicyStorage::SetPolicyPayload(const std::string& policy_type,
                                     const std::string& entity_id,
                                     const std::string& policy_payload) {
  policy_payloads_[GetPolicyKey(policy_type, entity_id)] = policy_payload;
}

std::string PolicyStorage::GetExternalPolicyPayload(
    const std::string& policy_type,
    const std::string& entity_id) {
  std::string policy_key = GetPolicyKey(policy_type, entity_id);
  return external_policy_payloads_.contains(policy_key)
             ? external_policy_payloads_.at(policy_key)
             : std::string();
}

void PolicyStorage::SetExternalPolicyPayload(
    const std::string& policy_type,
    const std::string& entity_id,
    const std::string& policy_payload) {
  external_policy_payloads_[GetPolicyKey(policy_type, entity_id)] =
      policy_payload;
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
    hash = base::U64FromBigEndian(hash_bytes);
    if (hash % modulus == remainder) {
      hashes.emplace_back(reinterpret_cast<const char*>(hash_bytes),
                          sizeof(hash));
    }
  }
  return hashes;
}

}  // namespace policy
