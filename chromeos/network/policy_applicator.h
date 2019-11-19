// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_POLICY_APPLICATOR_H_
#define CHROMEOS_NETWORK_POLICY_APPLICATOR_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chromeos/network/network_profile.h"

namespace chromeos {
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
    virtual ~ConfigurationHandler() {}
    // Write the new configuration with the properties |shill_properties| to
    // Shill. This configuration comes from a policy. Any conflicting or
    // existing configuration for the same network will have been removed
    // before. |callback| will be called after the configuration update has been
    // reflected in NetworkStateHandler, or on error.
    virtual void CreateConfigurationFromPolicy(
        const base::DictionaryValue& shill_properties,
        base::OnceClosure callback) = 0;

    // before. |callback| will be called after the configuration update has been
    // reflected in NetworkStateHandler, or on error.
    virtual void UpdateExistingConfigurationWithPropertiesFromPolicy(
        const base::DictionaryValue& existing_properties,
        const base::DictionaryValue& new_properties,
        base::OnceClosure callback) = 0;

    // Called after all policies for |profile| were applied. At this point, the
    // list of networks should be updated.
    virtual void OnPoliciesApplied(const NetworkProfile& profile) = 0;

   private:
    DISALLOW_ASSIGN(ConfigurationHandler);
  };

  using GuidToPolicyMap =
      std::map<std::string, std::unique_ptr<base::DictionaryValue>>;

  // |handler| must outlive this object.
  // |modified_policy_guids| must not be nullptr and will be empty afterwards.
  PolicyApplicator(const NetworkProfile& profile,
                   const GuidToPolicyMap& all_policies,
                   const base::DictionaryValue& global_network_config,
                   ConfigurationHandler* handler,
                   std::set<std::string>* modified_policy_guids);

  ~PolicyApplicator();

  void Run();

 private:
  // Called with the properties of the profile |profile_|. Requests the
  // properties of each entry, which are processed by GetEntryCallback.
  void GetProfilePropertiesCallback(
      const base::DictionaryValue& profile_properties);
  void GetProfilePropertiesError(const std::string& error_name,
                                 const std::string& error_message);

  // Called with the properties of the profile entry |entry|. Checks whether the
  // entry was previously managed, whether a current policy applies and then
  // either updates, deletes or not touches the entry.
  void GetEntryCallback(const std::string& entry,
                        const base::DictionaryValue& entry_properties);
  void GetEntryError(const std::string& entry,
                     const std::string& error_name,
                     const std::string& error_message);

  // Applies |new_policy| for |entry|.
  // |entry_properties| are the current properties for the entry. |ui_data| is
  // the NetworkUIData extracted from |entry_properties| and is passed so it
  // doesn't have to be re-extracted. |old_guid| is the current GUID of the
  // entry and may be empty.
  // |callback| will be called when policy application for |entry| has finished.
  void ApplyNewPolicy(const std::string& entry,
                      const base::Value& entry_properties,
                      std::unique_ptr<NetworkUIData> ui_data,
                      const std::string& old_guid,
                      const std::string& new_guid,
                      const base::Value& new_policy,
                      base::OnceClosure callback);

  // Applies the global network policy (if any) on |entry|,
  // |entry_properties|}  are the current properties for the entry.
  // |callback| will be called when policy application for |entry| has finished
  // or immediately if no global network policy is present.
  void ApplyGlobalPolicyOnUnmanagedEntry(
      const std::string& entry,
      const base::DictionaryValue& entry_properties,
      base::OnceClosure callback);

  // Sends Shill the command to delete profile entry |entry| from |profile_|.
  // |callback| will be called when the profile entry has been deleted in shill.
  void DeleteEntry(const std::string& entry, base::OnceClosure callback);

  // Applies |shill_dictionary| in shill. |policy_ is the ONC policy blob which
  // lead to the policy application. |callback| will be called when policy
  // application has finished, i.e. when the policy has been applied in shill
  // NetworkStateHandler in chrome has reflected the changes.
  void WriteNewShillConfiguration(base::Value shill_dictionary,
                                  base::Value policy,
                                  base::OnceClosure callback);

  // Removes |entry| from the list of pending profile entries.
  // If all entries were processed, applies the remaining policies and notifies
  // |handler_|.
  void ProfileEntryFinished(const std::string& entry);

  // Creates new entries for all remaining policies, i.e. for which no matching
  // Profile entry was found.
  // This should only be called if all profile entries were processed.
  void ApplyRemainingPolicies();

  // This is called when the remaining policy application for |entry| scheduled
  // by ApplyRemainingPolicies has finished.
  void RemainingPolicyApplied(const std::string& entry);

  // Called after all policies are applied or an error occurred. Notifies
  // |handler_|.
  void NotifyConfigurationHandlerAndFinish();

  std::set<std::string> remaining_policy_guids_;
  std::set<std::string> pending_get_entry_calls_;
  ConfigurationHandler* handler_;
  NetworkProfile profile_;
  GuidToPolicyMap all_policies_;
  base::DictionaryValue global_network_config_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PolicyApplicator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PolicyApplicator);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_POLICY_APPLICATOR_H_
