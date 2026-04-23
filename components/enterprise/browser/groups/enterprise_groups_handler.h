// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_GROUPS_ENTERPRISE_GROUPS_HANDLER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_GROUPS_ENTERPRISE_GROUPS_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"

class PrefService;

namespace policy {

// Base class for copying the available groups from `PolicyData` to preferences.
class EnterpriseGroupsHandlerBase : public CloudPolicyCore::Observer,
                                    public CloudPolicyService::Observer {
 public:
  explicit EnterpriseGroupsHandlerBase(CloudPolicyCore* cloud_policy_core);

  EnterpriseGroupsHandlerBase(const EnterpriseGroupsHandlerBase&) = delete;
  EnterpriseGroupsHandlerBase& operator=(const EnterpriseGroupsHandlerBase&) =
      delete;
  ~EnterpriseGroupsHandlerBase() override;

  // Initializes the handler. This will start observing `CloudPolicyCore` and
  // `CloudPolicyService` and update the groups if the policies are already
  // loaded.
  void Init();

  // Unconditionally clears the groups from preferences.
  void ClearGroups();

  // CloudPolicyCore::Observer:
  void OnCoreConnected(CloudPolicyCore* core) override;
  void OnRefreshSchedulerStarted(CloudPolicyCore* core) override;
  void OnCoreDisconnecting(CloudPolicyCore* core) override;

  // CloudPolicyService::Observer:
  void OnCloudPolicyServiceInitializationCompleted() override;
  void OnPolicyRefreshed(bool success) override;

  // Stop observing `CloudPolicyCore` and `CloudPolicyService`.
  void ResetObservations();

  // Returns whether the handler is observing `CloudPolicyCore`.
  bool IsObserving() const;

 private:
  void UpdateAvailableGroups();

  virtual void SaveGroupsToPrefs(base::ListValue groups) = 0;

  bool IsFeatureEnabled() const;

  raw_ptr<CloudPolicyCore> cloud_policy_core_;

  base::ScopedObservation<CloudPolicyCore, CloudPolicyCore::Observer>
      cloud_policy_core_observation_{this};
  base::ScopedObservation<CloudPolicyService, CloudPolicyService::Observer>
      cloud_policy_service_observation_{this};
};

// A class that handles copying the available groups from CBCM `PolicyData` to
// the local state preference.
class EnterpriseGroupsBrowserHandler : public EnterpriseGroupsHandlerBase {
 public:
  EnterpriseGroupsBrowserHandler(CloudPolicyCore* cloud_policy_core,
                                 PrefService* local_state);

  EnterpriseGroupsBrowserHandler(const EnterpriseGroupsBrowserHandler&) =
      delete;
  EnterpriseGroupsBrowserHandler& operator=(
      const EnterpriseGroupsBrowserHandler&) = delete;

  ~EnterpriseGroupsBrowserHandler() override;

 private:
  // EnterpriseGroupsHandlerBase:
  void SaveGroupsToPrefs(base::ListValue groups) override;

  raw_ptr<PrefService> local_state_;
};

// A class that handles copying the available groups from user `PolicyData`.
// The list of groups is saved to the local state preference, since it has to be
// available for the Variations code at browser startup.
// The class updates the list of groups only when the profile is activated.
// Idle profiles are still considered as valid as long as the profile exists.
class EnterpriseGroupsProfileHandler : public EnterpriseGroupsHandlerBase,
                                       public KeyedService {
 public:
  EnterpriseGroupsProfileHandler(CloudPolicyCore* cloud_policy_core,
                                 PrefService* local_state,
                                 const std::string& profile_key);

  EnterpriseGroupsProfileHandler(const EnterpriseGroupsProfileHandler&) =
      delete;
  EnterpriseGroupsProfileHandler& operator=(
      const EnterpriseGroupsProfileHandler&) = delete;

  ~EnterpriseGroupsProfileHandler() override;

  // Resets the observations and clears the groups from preferences.
  void ResetAndClearGroups();

  // KeyedService:
  void Shutdown() override;

 private:
  // EnterpriseGroupsHandlerBase:
  void SaveGroupsToPrefs(base::ListValue groups) override;

  raw_ptr<PrefService> local_state_;

  // This key must be consistent with
  // `ProfileAttributesStorage::StorageKeyFromProfilePath`.
  const std::string profile_key_;
};

}  // namespace policy

#endif  // COMPONENTS_ENTERPRISE_BROWSER_GROUPS_ENTERPRISE_GROUPS_HANDLER_H_
