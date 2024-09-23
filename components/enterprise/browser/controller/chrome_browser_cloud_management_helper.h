// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_CONTROLLER_CHROME_BROWSER_CLOUD_MANAGEMENT_HELPER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_CONTROLLER_CHROME_BROWSER_CLOUD_MANAGEMENT_HELPER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

class ClientDataDelegate;
class CloudPolicyClient;
class CloudPolicyClientRegistrationHelper;
class MachineLevelUserCloudPolicyManager;
class DeviceManagementService;

// A helper class that register device with the enrollment token and client id.
class ChromeBrowserCloudManagementRegistrar {
 public:
  ChromeBrowserCloudManagementRegistrar(
      DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ChromeBrowserCloudManagementRegistrar(
      const ChromeBrowserCloudManagementRegistrar&) = delete;
  ChromeBrowserCloudManagementRegistrar& operator=(
      const ChromeBrowserCloudManagementRegistrar&) = delete;

  ~ChromeBrowserCloudManagementRegistrar();

  // The callback invoked once policy registration is complete. Passed
  // |dm_token| and |client_id| parameters are empty if policy registration
  // failed.
  using CloudManagementRegistrationCallback =
      base::OnceCallback<void(const std::string& dm_token,
                              const std::string& client_id)>;

  // Registers a CloudPolicyClient for fetching machine level user policy.
  void RegisterForCloudManagementWithEnrollmentToken(
      const std::string& enrollment_token,
      const std::string& client_id,
      const ClientDataDelegate& client_data_delegate,
      CloudManagementRegistrationCallback callback);

 private:
  void CallCloudManagementRegistrationCallback(
      std::unique_ptr<CloudPolicyClient> client,
      CloudManagementRegistrationCallback callback);

  std::unique_ptr<CloudPolicyClientRegistrationHelper> registration_helper_;
  raw_ptr<DeviceManagementService> device_management_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

// A helper class that setup registration and fetch policy.
class MachineLevelUserCloudPolicyFetcher : public CloudPolicyService::Observer {
 public:
  MachineLevelUserCloudPolicyFetcher(
      MachineLevelUserCloudPolicyManager* policy_manager,
      PrefService* local_state,
      DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  MachineLevelUserCloudPolicyFetcher(
      const MachineLevelUserCloudPolicyFetcher&) = delete;
  MachineLevelUserCloudPolicyFetcher& operator=(
      const MachineLevelUserCloudPolicyFetcher&) = delete;

  ~MachineLevelUserCloudPolicyFetcher() override;

  // Initialize the cloud policy client and policy store then fetch
  // the policy based on the |dm_token|. It should be called only once.
  void SetupRegistrationAndFetchPolicy(const DMToken& dm_token,
                                       const std::string& client_id);

  // Add or remove |observer| to/from the CloudPolicyClient embedded in
  // |policy_manager_|.
  void AddClientObserver(CloudPolicyClient::Observer* observer);
  void RemoveClientObserver(CloudPolicyClient::Observer* observer);

  // Shuts down |policy_manager_| (removes and stops refreshing the cached cloud
  // policy).
  void Disconnect();

  // CloudPolicyService::Observer:
  void OnCloudPolicyServiceInitializationCompleted() override;
  std::string_view name() const override;

 private:
  void InitializeManager(std::unique_ptr<CloudPolicyClient> client);
  // Fetch policy if device is enrolled.
  void TryToFetchPolicy();

  raw_ptr<MachineLevelUserCloudPolicyManager> policy_manager_;
  raw_ptr<PrefService> local_state_;
  raw_ptr<DeviceManagementService> device_management_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace policy

#endif  // COMPONENTS_ENTERPRISE_BROWSER_CONTROLLER_CHROME_BROWSER_CLOUD_MANAGEMENT_HELPER_H_
