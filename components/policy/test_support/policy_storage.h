// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_POLICY_STORAGE_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_POLICY_STORAGE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/time/time.h"
#include "components/policy/test_support/signature_provider.h"

namespace policy {

class SignatureProvider;

// Stores preferences about policies to be applied to registered browsers.
class PolicyStorage {
 public:
  PolicyStorage();
  PolicyStorage(PolicyStorage&& policy_storage);
  PolicyStorage& operator=(PolicyStorage&& policy_storage);
  virtual ~PolicyStorage();

  // Returns the serialized proto associated with |policy_type|. Returns empty
  // string if there is no such association.
  std::string GetPolicyPayload(const std::string& policy_type) const;
  // Associates the serialized proto stored in |policy_payload| with
  // |policy_type|.
  void SetPolicyPayload(const std::string& policy_type,
                        const std::string& policy_payload);

  SignatureProvider* signature_provider() const {
    return signature_provider_.get();
  }
  void set_signature_provider(
      std::unique_ptr<SignatureProvider> signature_provider) {
    signature_provider_ = std::move(signature_provider);
  }

  const std::string& robot_api_auth_code() const {
    return robot_api_auth_code_;
  }
  void set_robot_api_auth_code(const std::string& robot_api_auth_code) {
    robot_api_auth_code_ = robot_api_auth_code;
  }

  const std::string& service_account_identity() const {
    return service_account_identity_;
  }
  void set_service_account_identity(
      const std::string& service_account_identity) {
    service_account_identity_ = service_account_identity;
  }

  const std::set<std::string>& managed_users() const { return managed_users_; }
  void add_managed_user(const std::string& managed_user) {
    managed_users_.insert(managed_user);
  }

  std::string policy_user() const { return policy_user_; }
  void set_policy_user(const std::string& policy_user) {
    policy_user_ = policy_user;
  }

  const std::string& policy_invalidation_topic() const {
    return policy_invalidation_topic_;
  }
  void set_policy_invalidation_topic(
      const std::string& policy_invalidation_topic) {
    policy_invalidation_topic_ = policy_invalidation_topic;
  }

  base::Time timestamp() const { return timestamp_; }
  void set_timestamp(const base::Time& timestamp) { timestamp_ = timestamp; }

 private:
  // Maps policy types to a serialized proto representing the policies to be
  // applied for the type (e.g. CloudPolicySettings,
  // ChromeDeviceSettingsProto).
  std::map<std::string, std::string> policy_payloads_;

  std::unique_ptr<SignatureProvider> signature_provider_;

  std::string robot_api_auth_code_;

  std::string service_account_identity_;

  std::set<std::string> managed_users_;

  std::string policy_user_;

  std::string policy_invalidation_topic_;

  base::Time timestamp_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_POLICY_STORAGE_H_
