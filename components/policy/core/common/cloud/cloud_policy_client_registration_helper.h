// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_REGISTRATION_HELPER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_REGISTRATION_HELPER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/user_info_fetcher.h"
#include "components/policy/policy_export.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "google_apis/gaia/core_account_id.h"

namespace signin {
class IdentityManager;
}

namespace policy {

class ClientDataDelegate;

// Helper class that registers a CloudPolicyClient. It fetches an OAuth2 token
// for the DM service if needed, and checks with Gaia if the account has policy
// management enabled.
class POLICY_EXPORT CloudPolicyClientRegistrationHelper
    : public UserInfoFetcher::Delegate,
      public CloudPolicyClient::Observer {
 public:
  // |context| and |client| are not owned and must outlive this object.
  CloudPolicyClientRegistrationHelper(
      CloudPolicyClient* client,
      enterprise_management::DeviceRegisterRequest::Type registration_type);
  CloudPolicyClientRegistrationHelper(
      const CloudPolicyClientRegistrationHelper&) = delete;
  CloudPolicyClientRegistrationHelper& operator=(
      const CloudPolicyClientRegistrationHelper&) = delete;
  ~CloudPolicyClientRegistrationHelper() override;

  // Starts the client registration process. This version uses the
  // supplied IdentityManager to mint the new token for the userinfo
  // and DM services, using the |account_id|.
  // |callback| is invoked when the registration is complete.
  void StartRegistration(signin::IdentityManager* identity_manager,
                         const CoreAccountId& account_id,
                         base::OnceClosure callback);

  // Starts the device registration with an token enrollment process.
  // |callback| is invoked when the registration is complete.
  void StartRegistrationWithEnrollmentToken(
      const std::string& token,
      const std::string& client_id,
      const ClientDataDelegate& client_data_delegate,
      bool is_mandatory,
      base::OnceClosure callback);

  // Starts the client registration with an OIDC token enrollment process.
  // `oauth_token` and `id_token` pair is received and extracted from a valid
  // OIDC authentication redirection response. The `oauth_token` is from a 3P
  // IdP, different from a refresh_token or access_token from GAIA. `client_id`
  // is randomized if an empty string is provided. `state` contains details
  // relevant for OIDC profile enrollment. `callback` is invoked when
  // the registration is complete.
  // Slightly different from other methods, the callback is invoked inside the
  // policy client rather than in this class.
  void StartRegistrationWithOidcTokens(
      const std::string& oauth_token,
      const std::string& id_token,
      const std::string& client_id,
      const std::string& state,
      const base::TimeDelta& timeout_duration,
      CloudPolicyClient::ResultCallback callback);

 private:
  class IdentityManagerHelper;

  void OnTokenFetched(const std::string& oauth_access_token);

  // UserInfoFetcher::Delegate implementation:
  void OnGetUserInfoSuccess(const base::Value::Dict& response) override;
  void OnGetUserInfoFailure(const GoogleServiceAuthError& error) override;

  // CloudPolicyClient::Observer implementation:
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override;

  // Invoked when the registration request has been completed.
  void RequestCompleted();

  // Internal helper class that uses IdentityManager to fetch an OAuth
  // access token.
  std::unique_ptr<IdentityManagerHelper> identity_manager_helper_;

  // Helper class for fetching information from GAIA about the currently
  // signed-in user.
  std::unique_ptr<UserInfoFetcher> user_info_fetcher_;

  // Access token used to register the CloudPolicyClient and also access
  // GAIA to get information about the signed in user.
  std::string oauth_access_token_;

  raw_ptr<CloudPolicyClient> client_;
  enterprise_management::DeviceRegisterRequest::Type registration_type_;
  base::OnceClosure callback_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_REGISTRATION_HELPER_H_
