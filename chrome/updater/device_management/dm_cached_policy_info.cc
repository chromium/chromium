// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management/dm_cached_policy_info.h"

#include "components/policy/proto/device_management_backend.pb.h"

namespace updater {

CachedPolicyInfo::CachedPolicyInfo() : key_version_(-1), timestamp_(0) {}
CachedPolicyInfo::~CachedPolicyInfo() = default;

bool CachedPolicyInfo::Populate(const std::string& raw_response) {
  ::enterprise_management::PolicyFetchResponse response;
  ::enterprise_management::PolicyData policy_data;
  ::enterprise_management::PublicKeyVerificationData verification_data;
  if (raw_response.empty() || !response.ParseFromString(raw_response) ||
      !policy_data.ParseFromString(response.policy_data()) ||
      !policy_data.has_timestamp() ||
      !response.has_new_public_key_verification_data() ||
      !verification_data.ParseFromString(
          response.new_public_key_verification_data())) {
    return false;
  }

  key_ = verification_data.new_public_key();
  if (verification_data.has_new_public_key_version())
    key_version_ = verification_data.new_public_key_version();
  timestamp_ = policy_data.timestamp();
  return true;
}

}  // namespace updater
