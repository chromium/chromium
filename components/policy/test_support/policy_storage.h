// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_POLICY_STORAGE_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_POLICY_STORAGE_H_

#include <map>
#include <string>

namespace policy {

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

  std::string robot_api_auth_code() const { return robot_api_auth_code_; }
  void set_robot_api_auth_code(const std::string& robot_api_auth_code) {
    robot_api_auth_code_ = robot_api_auth_code;
  }

  std::string service_account_identity() const {
    return service_account_identity_;
  }
  void set_service_account_identity(
      const std::string& service_account_identity) {
    service_account_identity_ = service_account_identity;
  }

 private:
  // Maps policy types to a serialized proto representing the policies to be
  // applied for the type (e.g. CloudPolicySettings,
  // ChromeDeviceSettingsProto).
  std::map<std::string, std::string> policy_payloads_;

  std::string robot_api_auth_code_;

  std::string service_account_identity_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_POLICY_STORAGE_H_
