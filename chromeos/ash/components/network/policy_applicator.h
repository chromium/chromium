// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_POLICY_APPLICATOR_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_POLICY_APPLICATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_profile.h"

namespace ash {

class ManagedCellularPrefHandler;
class NetworkUIData;

// This class compares (entry point is Run()) |modified_policies| with the
// existing entries in the provided Shill profile |profile|. It fetches all
// entries in parallel (GetProfilePropertiesCallback), compares each entry with
// the current policies (GetEntryCallback) and adds all missing policies
// (~PolicyApplicator).
class PolicyApplicator {
 public:
  class ConfigurationHandler {
   public:
    ConfigurationHandler& operator=(const ConfigurationHandler&) = delete;

    virtual ~ConfigurationHandler() = default;
    // Write the new configuration with the properties `shill_properties` to
    // Shill. This configuration comes from a policy. Any conflicting or
    // existing configuration for the same network will have been removed
    // before.
    // `callback` does not necessarily signal success and is only used for
    // completion - it will be called after the configuration update has been
    // reflected in NetworkStateHandler or when an error has occurred.
    virtual void CreateConfigurationFromPolicy(
        const base::Value::Dict& shill_properties,
        base::OnceClosure callback) = 0;

    // Modifies the properties of an already-configured network.
    // `existing_properties` is used to find the network.
    // `callback` does not necessarily signal success and is only used for
    // completion - it will be called after the configuration update has been
    // reflected in NetworkStateHandler or when an error has occurred.
    virtual void UpdateExistingConfigurationWithPropertiesFromPolicy(
        const base::Value::Dict& existing_properties,
        const base::Value::Dict& new_properties,
        base::OnceClosure callback) = 0;

    // Called after all policies for |profile| were applied except for new
    // cellular policies.
    // The set of new cellular policy guids is passed in
    // `new_cellular_policy_guids`.
    // At this point, the list of networks should be updated.
    virtual void OnPoliciesApplied(
        const NetworkProfile& profile,
        const base::flat_set<std::string>& new_cellular_policy_guids) = 0;
  };

  struct Options {
    Options();
    Options(Options&& other);
    Options& operator=(Options&& other);
    Options(const Options&) = delete;
    Options& operator=(const Options&) = delete;
    ~Options();

    // Merges this `Options` instance with `other`, ORing the requested
    // operation flags.
    void Merge(const Options& other);

    // When this is set to true, all managed configurations with at least one
    // "Recommended" field will be reset to only contain the managed settings.
    bool reset_recommended_managed_configs = false;

    // When this is set to true, all unmanaged configurations will be removed.
    bool remove_unmanaged_configs = false;
  };

  // `handler` and `managed_cellular_pref_handler` must outlive this object.
  PolicyApplicator(const NetworkProfile& profile,
                   base::flat_map<std::string, base::Value::Dict> all_policies,
                   base::Value::Dict global_network_config,
                   ConfigurationHandler* handler,
                   ManagedCellularPrefHandler* managed_cellular_pref_handler,
                   base::flat_set<std::string> modified_policy_guids,
                   Options options);

  PolicyApplicator(const PolicyApplicator&) = delete;
  PolicyApplicator& operator=(const PolicyApplicator&) = delete;

  ~PolicyApplicator();

  void Run();

 private:
  // Called with the properties of the profile |profile_|. Requests the
  // properties of each entry, which are processed by GetEntryCallback.
  void GetProfilePropertiesCallback(base::Value::Dict profile_properties);
  void GetProfilePropertiesError(const std::string& error_name,
                                 const std::string& error_message);

  // Called with the properties of the profile entry |entry_identifier|. Checks
  // whether the entry was previously managed, whether a current policy applies
  // and then either updates, deletes or not touches the entry.
  void GetEntryCallback(const std::string& entry_identifier,
                        base::Value::Dict entry_properties);
  void GetEntryError(const std::string& entry_identifier,
                     const std::string& error_name,
                     const std::string& error_message);

  // Applies |new_policy| for |entry_identifier|.
  // |entry_properties| are the current properties for the entry. |ui_data| is
  // the NetworkUIData extracted from |entry_properties| and is passed so it
  // doesn't have to be re-extracted. |old_guid| is the current GUID of the
  // entry and may be empty.
  // |callback| will be called when policy application for |entry_identifier|
  // has finished.
  void ApplyOncPolicy(const std::string& entry_identifier,
                      const base::Value::Dict& entry_properties,
                      std::unique_ptr<NetworkUIData> ui_data,
                      const std::string& old_guid,
                      const std::string& new_guid,
                      const base::Value::Dict& new_policy,
                      base::OnceClosure callback);

  // Applies the global network policy (if any) on |entry_identifier|,
  // |entry_properties|}  are the current properties for the entry.
  // |callback| will be called when policy application for |entry_identifier|
  // has finished or immediately if no global network policy is present.
  void ApplyGlobalPolicyOnUnmanagedEntry(
      const std::string& entry_identifier,
      const base::Value::Dict& entry_properties,
      base::OnceClosure callback);

  // Sends Shill the command to delete profile entry |entry_identifier| from
  // |profile_|. |callback| will be called when the profile entry has been
  // deleted in shill.
  void DeleteEntry(const std::string& entry_identifier,
                   base::OnceClosure callback);

  // Applies |shill_dictionary| in shill. |policy_ is the ONC policy blob which
  // lead to the policy application. |callback| will be called when policy
  // application has finished, i.e. when the policy has been applied in shill
  // NetworkStateHandler in chrome has reflected the changes.
  void WriteNewShillConfiguration(base::Value::Dict shill_dictionary,
                                  base::Value::Dict policy,
                                  base::OnceClosure callback);

  // Removes |entry_identifier| from the list of pending profile entries.
  // If all entries were processed, applies the remaining policies and notifies
  // |handler_|.
  void ProfileEntryFinished(const std::string& entry_identifier);

  // Creates new entries for all remaining policies, i.e. for which no matching
  // Profile entry was found.
  // This should only be called if all profile entries were processed.
  void ApplyRemainingPolicies();

  // This is called when the remaining policy application for |guid| scheduled
  // by ApplyRemainingPolicies has finished.
  void RemainingPolicyApplied(const std::string& guid);

  // Called after all policies are applied or an error occurred. Notifies
  // |handler_|.
  void NotifyConfigurationHandlerAndFinish();

  const raw_ptr<ConfigurationHandler> handler_;
  raw_ptr<ManagedCellularPrefHandler> managed_cellular_pref_handler_ = nullptr;
  NetworkProfile profile_;
  base::flat_map<std::string, base::Value::Dict> all_policies_;
  base::Value::Dict global_network_config_;
  const Options options_;

  base::flat_set<std::string> remaining_policy_guids_;
  base::flat_set<std::string> pending_get_entry_calls_;

  // Contains GUIDs of new cellular policies so they can be reported back to
  // the caller.
  base::flat_set<std::string> new_cellular_policy_guids_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PolicyApplicator> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_POLICY_APPLICATOR_H_
