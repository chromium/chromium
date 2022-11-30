// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_USER_POLICIES_MANAGER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_USER_POLICIES_MANAGER_H_

#include <string>

#include "base/component_export.h"
#include "base/win/windows_types.h"
#include "chrome/credential_provider/extension/task_manager.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/user_policies.h"
#include "url/gurl.h"

namespace credential_provider {

// Manager used to fetch user policies from GCPW backends.
class COMPONENT_EXPORT(GCPW_POLICIES) UserPoliciesManager {
 public:
  // Get the user policies manager instance.
  static UserPoliciesManager* Get();

  // Provides the GCPW extension with a TaskCreator which can be used to create
  // a task for fetching user policies.
  static extension::TaskCreator GetFetchPoliciesTaskCreator();

  // Return true if cloud policies feature is enabled.
  bool CloudPoliciesEnabled() const;

  // Fetch the policies for the user from GCPW backend with |sid| using
  // |access_token| for authentication and authorization and saves it in file
  // storage replacing any previously fetched versions.
  virtual HRESULT FetchAndStoreCloudUserPolicies(
      const std::wstring& sid,
      const std::string& access_token);

  // Fetch the policies for the user-device |context| provided by the GCPW
  // extension service from the GCPW backend and saves it in file storage
  // replacing any previously fetched versions.
  virtual HRESULT FetchAndStoreCloudUserPolicies(
      const extension::UserDeviceContext& context);

  // Get the URL of GCPW service for HTTP request for fetching user policies
  // when the caller has a valid OAuth token for authentication.
  GURL GetGcpwServiceUserPoliciesUrl(const std::wstring& sid);

  // Get the URL of GCPW service for HTTP request for fetching user policies
  // when the caller only has a DM token.
  GURL GetGcpwServiceUserPoliciesUrl(const std::wstring& sid,
                                     const std::wstring& device_resource_id,
                                     const std::wstring& dm_token);

  // Retrieves the policies for the user with |sid| from local storage. Returns
  // the default user policy if policy not fetched or on any error.
  virtual bool GetUserPolicies(const std::wstring& sid,
                               UserPolicies* user_policies) const;

  // Returns true if the policies are missing for the user with |sid| or if
  // they haven't been refreshed recently.
  virtual bool IsUserPolicyStaleOrMissing(const std::wstring& sid) const;

  // For testing only return the status of the last policy fetch.
  HRESULT GetLastFetchStatusForTesting() const;

  // For testing manually control if the cloud policies feature is enabled.
  void SetCloudPoliciesEnabledForTesting(bool value);

  // Set fakes for cloud policies unit tests.
  void SetFakesForTesting(FakesForTesting* fakes);

 protected:
  // Returns the storage used for the instance pointer.
  static UserPoliciesManager** GetInstanceStorage();

  // Fetch the user policies using the given backend url and access token if
  // specified.
  HRESULT FetchAndStorePolicies(const std::wstring& sid,
                                GURL user_policies_url,
                                const std::string& access_token);

  UserPoliciesManager();
  virtual ~UserPoliciesManager();

  HRESULT fetch_status_;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_USER_POLICIES_MANAGER_H_
