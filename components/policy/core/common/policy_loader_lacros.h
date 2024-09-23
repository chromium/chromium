// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_LACROS_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_LACROS_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/policy/core/common/async_policy_loader.h"
#include "components/policy/core/common/policy_proto_decoders.h"
#include "components/policy/core/common/values_util.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

// A policy loader for Lacros. The data is taken from Ash and the validitity of
// data is trusted, since they have been validated by Ash.
class POLICY_EXPORT PolicyLoaderLacros
    : public AsyncPolicyLoader,
      public chromeos::LacrosService::Observer {
 public:
  // Creates the policy loader, saving the task_runner internally. Later
  // task_runner is used to have in sequence the process of policy parsing and
  // validation. The |per_profile| parameter specifies which policy should be
  // installed.
  PolicyLoaderLacros(scoped_refptr<base::SequencedTaskRunner> task_runner,
                     PolicyPerProfileFilter per_profile);
  // Not copyable or movable
  PolicyLoaderLacros(const PolicyLoaderLacros&) = delete;
  PolicyLoaderLacros& operator=(const PolicyLoaderLacros&) = delete;
  ~PolicyLoaderLacros() override;

  // AsyncPolicyLoader implementation.
  // Verifies that it runs on correct thread. Detaches from the sequence checker
  // which allows all other methods to check that they are executed on the same
  // sequence. |sequence_checker_| is used for that.
  void InitOnBackgroundThread() override;
  // Loads the policy data from LacrosInitParams and populates it in the bundle
  // that is returned.
  PolicyBundle Load() override;

  // Return the policy data object as received from Ash. Returns nullptr if
  // initial load was not done yet.
  enterprise_management::PolicyData* GetPolicyData();

  // chromeos::LacrosService::Observer implementation.
  // Update and reload the policy with new data.
  void OnPolicyUpdated(
      const std::vector<uint8_t>& policy_fetch_response) override;

  // chromeos::LacrosService::Observer implementation.
  // Update the latest policy fetch attempt timestamp.
  void OnPolicyFetchAttempt() override;

  // chromeos::LacrosService::Observer implementation.
  void OnComponentPolicyUpdated(
      const policy::ComponentPolicyMap& component_policy) override;

  // Returns the current device account policies for components.
  const PolicyBundle* component_policy() const {
    return component_policy_.get();
  }

  // Return if the main user is a device local account (i.e. Kiosk, MGS) user.
  static bool IsDeviceLocalAccountUser();

  // Returns if the main user is managed or not.
  // TODO(crbug.com/40788404): Remove once Lacros handles all profiles the same
  // way.
  static bool IsMainUserManaged();

  // Return if the main user is affiliated or not.
  static bool IsMainUserAffiliated();

  // Returns the policy data corresponding to the main user to be used by
  // Enterprise Connector policies.
  // TODO(crbug.com/40788404): Remove once Lacros handles all profiles the same
  // way.
  static const enterprise_management::PolicyData* main_user_policy_data();
  static void set_main_user_policy_data_for_testing(
      const enterprise_management::PolicyData& policy_data);

  static const std::vector<std::string> device_affiliation_ids();
  static const std::string device_dm_token();

  base::Time last_fetch_timestamp() { return last_fetch_timestamp_; }

 private:
  void SetComponentPolicy(const policy::ComponentPolicyMap& component_policy);

  // The filter for policy data to install.
  const PolicyPerProfileFilter per_profile_;

  // Serialized blob of PolicyFetchResponse object received from the server.
  std::optional<std::vector<uint8_t>> policy_fetch_response_;

  // The component policy of the device account.
  std::unique_ptr<PolicyBundle> component_policy_;

  // The parsed policy objects received from Ash.
  std::unique_ptr<enterprise_management::PolicyData> policy_data_;

  // Timestamp at which last policy fetch was attempted.
  base::Time last_fetch_timestamp_;

  // Checks that the method is called on the right sequence.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOADER_LACROS_H_
