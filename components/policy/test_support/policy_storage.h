// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_POLICY_STORAGE_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_POLICY_STORAGE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "components/policy/proto/device_management_backend.pb.h"
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

  // Returns the serialized proto associated with |policy_type| and optional
  // |entity_id|. Returns empty string if there is no such association.
  std::string GetPolicyPayload(
      const std::string& policy_type,
      const std::string& entity_id = std::string()) const;
  std::vector<std::string> GetEntityIdsForType(const std::string& policy_type);

  // Associates the serialized proto stored in |policy_payload| with
  // |policy_type| and optional |entity_id|.
  void SetPolicyPayload(const std::string& policy_type,
                        const std::string& policy_payload);
  void SetPolicyPayload(const std::string& policy_type,
                        const std::string& entity_id,
                        const std::string& policy_payload);

  // Returns the raw payload to be served by an external endpoint and associated
  // with |policy_type| and optional |entity_id|. Returns empty string if there
  // is no such association.
  std::string GetExternalPolicyPayload(const std::string& policy_type,
                                       const std::string& entity_id);

  // Associates the |raw_payload| to be served via an external endpoint with
  // |policy_type| and optional |entity_id|.
  void SetExternalPolicyPayload(const std::string& policy_type,
                                const std::string& entity_id,
                                const std::string& raw_payload);

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

  bool has_kiosk_license() const { return has_kiosk_license_; }
  void set_has_kiosk_license(bool has_kiosk_license) {
    has_kiosk_license_ = has_kiosk_license;
  }

  bool has_enterprise_license() const { return has_enterprise_license_; }
  void set_has_enterprise_license(bool has_enterprise_license) {
    has_enterprise_license_ = has_enterprise_license;
  }

  const std::string& service_account_identity() const {
    return service_account_identity_;
  }
  void set_service_account_identity(
      const std::string& service_account_identity) {
    service_account_identity_ = service_account_identity;
  }

  const base::flat_set<std::string>& managed_users() const {
    return managed_users_;
  }
  void add_managed_user(const std::string& managed_user) {
    managed_users_.insert(managed_user);
  }

  const std::vector<std::string>& device_affiliation_ids() const {
    return device_affiliation_ids_;
  }
  void add_device_affiliation_id(const std::string& device_affiliation_id) {
    device_affiliation_ids_.emplace_back(device_affiliation_id);
  }

  const std::vector<std::string>& user_affiliation_ids() const {
    return user_affiliation_ids_;
  }
  void add_user_affiliation_id(const std::string& user_affiliation_id) {
    user_affiliation_ids_.emplace_back(user_affiliation_id);
  }

  const std::string& directory_api_id() const { return directory_api_id_; }
  void set_directory_api_id(const std::string& directory_api_id) {
    directory_api_id_ = directory_api_id;
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

  const std::optional<enterprise_management::PolicyData::MarketSegment>
  market_segment() const {
    return market_segment_;
  }
  void set_market_segment(
      enterprise_management::PolicyData::MarketSegment segment) {
    market_segment_ = segment;
  }

  const std::optional<enterprise_management::PolicyData::MetricsLogSegment>
  metrics_log_segment() const {
    return metrics_log_segment_;
  }
  void set_metrics_log_segment(
      enterprise_management::PolicyData::MetricsLogSegment segment) {
    metrics_log_segment_ = segment;
  }

  base::Time timestamp() const { return timestamp_; }
  void set_timestamp(const base::Time& timestamp) { timestamp_ = timestamp; }

  bool allow_set_device_attributes() { return allow_set_device_attributes_; }
  void set_allow_set_device_attributes(bool allow_set_device_attributes) {
    allow_set_device_attributes_ = allow_set_device_attributes;
  }

  struct DeviceState {
    std::string management_domain;
    enterprise_management::DeviceStateRetrievalResponse::RestoreMode
        restore_mode;
  };

  const DeviceState& device_state() { return device_state_; }
  void set_device_state(const DeviceState& device_state) {
    device_state_ = device_state;
  }

  struct PsmEntry {
    int psm_execution_result;
    int64_t psm_determination_timestamp;
  };

  void SetPsmEntry(const std::string& brand_serial_id,
                   const PsmEntry& psm_entry);

  const PsmEntry* GetPsmEntry(const std::string& brand_serial_id) const;

  struct InitialEnrollmentState {
    enterprise_management::DeviceInitialEnrollmentStateResponse::
        InitialEnrollmentMode initial_enrollment_mode;
    std::string management_domain;
  };

  void SetInitialEnrollmentState(
      const std::string& brand_serial_id,
      const InitialEnrollmentState& initial_enrollment_state);

  const InitialEnrollmentState* GetInitialEnrollmentState(
      const std::string& brand_serial_id) const;

  // Returns hashes for brand serial IDs whose initial enrollment state is
  // registered on the server. Only hashes, which, when divied by |modulus|,
  // result in the specified |remainder|, are returned.
  std::vector<std::string> GetMatchingSerialHashes(uint64_t modulus,
                                                   uint64_t remainder) const;

  void set_error_detail(
      enterprise_management::DeviceManagementErrorDetail error_detail) {
    error_detail_ = error_detail;
  }

  enterprise_management::DeviceManagementErrorDetail error_detail() const {
    return error_detail_;
  }

  bool enrollment_required() const { return enrollment_required_; }
  void set_enrollment_required(bool enrollment_required) {
    enrollment_required_ = enrollment_required;
  }

 private:
  // Maps policy keys to a serialized proto representing the policies to be
  // applied for the type (e.g. CloudPolicySettings, ChromeDeviceSettingsProto).
  base::flat_map<std::string, std::string> policy_payloads_;

  // Maps policy keys to a raw policy data served via an external endpoint.
  base::flat_map<std::string, std::string> external_policy_payloads_;

  std::unique_ptr<SignatureProvider> signature_provider_;

  std::string robot_api_auth_code_;

  std::string service_account_identity_;

  base::flat_set<std::string> managed_users_;

  std::vector<std::string> device_affiliation_ids_;

  std::vector<std::string> user_affiliation_ids_;

  std::string directory_api_id_;

  std::string policy_user_;

  std::string policy_invalidation_topic_;

  std::optional<enterprise_management::PolicyData::MarketSegment>
      market_segment_;
  std::optional<enterprise_management::PolicyData::MetricsLogSegment>
      metrics_log_segment_;

  base::Time timestamp_;

  bool allow_set_device_attributes_ = true;

  DeviceState device_state_;

  bool has_kiosk_license_ = true;

  bool has_enterprise_license_ = true;

  bool enrollment_required_ = false;

  // Maps brand serial ID to PsmEntry.
  base::flat_map<std::string, PsmEntry> psm_entries_;

  // Maps brand serial ID to InitialEnrollmentState.
  base::flat_map<std::string, InitialEnrollmentState>
      initial_enrollment_states_;

  // Determines whether the DMToken should be invalidated or deleted during
  // browser unenrollment.
  enterprise_management::DeviceManagementErrorDetail error_detail_ =
      enterprise_management::CBCM_DELETION_POLICY_PREFERENCE_INVALIDATE_TOKEN;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_POLICY_STORAGE_H_
