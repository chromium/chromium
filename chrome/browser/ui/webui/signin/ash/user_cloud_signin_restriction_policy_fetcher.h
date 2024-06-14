// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_USER_CLOUD_SIGNIN_RESTRICTION_POLICY_FETCHER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_USER_CLOUD_SIGNIN_RESTRICTION_POLICY_FETCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/values.h"
#include "components/policy/core/common/cloud/user_info_fetcher.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace network {
class SimpleURLLoader;
}

namespace base {
class TimeTicks;
}

namespace ash {

// UserCloudSigninRestrictionPolicyFetcher handles requesting
// SecondaryGoogleAccountUsage policy value for Chrome OS.
// During this fetch, two extra requests need to be made:
//
//   1 - Requesting access token with the following scopes:
//       - GaiaConstants::kGoogleUserInfoEmail
//       - GaiaConstants::kGoogleUserInfoProfile
//   2 - Checking if the account is an Enterprise account
//
// After fetching the policy value, it will run `callback` with the fetched
// value, `Status::kSuccess` status and the hosted domain of the account. If any
// error occurs during the process of fetching the policy value, `callback` will
// run with `std::nullopt` and a proper error status value to inform about the
// error. If the account is not an Enterprise account, `callback` will run with
// `std::nullopt` and `Status::kUnsupportedAccountTypeError`.
//
// Note: This class is meant to be used in a one-shot fashion and cannot handle
// multiple requests at the same time.
//
// Example usage:
//
// std::unique_ptr<GaiaAccessTokenFetcher> access_token_fetcher = ...;
// base::OnceCallback<void(
// UserCloudSigninRestrictionPolicyFetcher::Status,
// std::optional<std::string>, const std::string&)> callback = ...;
//
// UserCloudSigninRestrictionPolicyFetcher
//  restriction_fetcher("alice@example.com", url_loader_factory);
// restriction_fetcher.GetSecondaryGoogleAccountUsage(
//     std::move(access_token_fetcher), callback);
// TODO(b/222695699): Refactor this class to share code with
// UserCloudSigninRestrictionPolicyFetcher.
class UserCloudSigninRestrictionPolicyFetcher
    : public policy::UserInfoFetcher::Delegate,
      public OAuth2AccessTokenConsumer {
 public:
  // Values for policy SecondaryGoogleAccountUsage.
  static const char kSecondaryGoogleAccountUsagePolicyValueAll[];
  static const char
      kSecondaryGoogleAccountUsagePolicyValuePrimaryAccountSignin[];

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum Status {
    kUnknownError = 0,
    kSuccess,
    kNetworkError,
    kHttpError,
    kParsingResponseError,
    kUnsupportedAccountTypeError,
    kGetTokenError,
    kGetUserInfoError,
    kMaxValue = kGetUserInfoError,
  };

  // Callback invoked when SecondaryGoogleAccountUsage policy value
  // is fetched. `policy` is the fetched policy value. `domain` is the
  // Enterprise account hosted domain.
  using PolicyInfoCallback =
      base::OnceCallback<void(Status status,
                              std::optional<std::string> policy,
                              const std::string& domain)>;

  // Callback invoked when SecondaryAccountAllowedInArc policy value
  // is fetched. `policy` is the fetched policy value.
  using PolicyInfoCallbackForArc =
      base::OnceCallback<void(Status status, std::optional<bool> policy)>;

  // `email` can be a raw email (abc.123.4@gmail.com) or a canonicalized email
  // (abc1234@gmail.com). It's used to skip API requests for domains such as
  // gmail.com and others since these type of users are known to be
  // non-enterprise. For more information check
  // signin::AccountManagedStatusFinder::IsEnterpriseUserBasedOnEmail.
  UserCloudSigninRestrictionPolicyFetcher(
      const std::string& email,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~UserCloudSigninRestrictionPolicyFetcher() override;

  UserCloudSigninRestrictionPolicyFetcher(
      const UserCloudSigninRestrictionPolicyFetcher&) = delete;
  UserCloudSigninRestrictionPolicyFetcher& operator=(
      const UserCloudSigninRestrictionPolicyFetcher&) = delete;

  // Fetches the value of the SecondaryGoogleAccountUsage policy and runs
  // `callback` with the fetched value and `Status::kSuccess` status.
  //
  // If the policy was not set, `callback` will run with `std::nullopt` and
  // `Status::kSuccess` status.
  // If there was an error in fetching the policy, `callback` will run with
  // `std::nullopt` and the proper error status.
  void GetSecondaryGoogleAccountUsage(
      std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher,
      PolicyInfoCallback callback);

  void GetSecondaryAccountAllowedInArcPolicy(
      std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher,
      PolicyInfoCallbackForArc callback);

  // Protected for testing.
 protected:
  // UserInfoFetcher::Delegate.
  void OnGetUserInfoSuccess(const base::Value::Dict& user_info) override;
  void OnGetUserInfoFailure(const GoogleServiceAuthError& error) override;

  // UserInfoFetcher::OAuth2AccessTokenConsumer.
  void OnGetTokenSuccess(const TokenResponse& token_response) override;
  void OnGetTokenFailure(const GoogleServiceAuthError& error) override;
  std::string GetConsumerName() const override;

  // Retrieves the policy value from `response_body` and runs `callback` with
  // the retrieved value and `Status::kSuccess` status if successful.
  // If there was an error in fetching the policy, it will runs `callback` with
  // `std::nullopt` and the proper error status.
  void OnSecondaryGoogleAccountUsageResult(
      std::unique_ptr<std::string> response_body);

 private:
  // Fetch access token with `GaiaConstants::kGoogleUserInfoEmail` and
  // `GaiaConstants::kGoogleUserInfoProfile` scopes to get the policy
  // restriction value for the account.
  // Virtual for testing.
  virtual void FetchAccessToken();
  // Fetch user info to check if the account is an Enterprise account or not.
  // Virtual for testing.
  virtual void FetchUserInfo();
  // Calls the SecureConnect API to get the SecondaryGoogleAccountUsage
  // policy using `access_token` for the authentication. Calls
  // `OnSecondaryGoogleAccountUsageResult` with the result from the API.
  void GetSecondaryGoogleAccountUsageInternal();

  void GetSecondaryAccountAllowedInArcInternal();
  bool IsFetchingArcPolicy() const;
  void FinalizeResult(Status status, std::optional<std::string> policy);

  std::string GetSecureConnectApiGetAccountSigninRestrictionUrl() const;

  std::string email_;
  std::string hosted_domain_;
  std::string access_token_;

  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher_;
  std::unique_ptr<policy::UserInfoFetcher> user_info_fetcher_;
  PolicyInfoCallback callback_;
  PolicyInfoCallbackForArc callback_for_arc_;
  base::TimeTicks policy_fetch_start_time_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_USER_CLOUD_SIGNIN_RESTRICTION_POLICY_FETCHER_H_
