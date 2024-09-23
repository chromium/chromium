// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_NETWORK_INTERFACE_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_NETWORK_INTERFACE_BASE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/origin.h"

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
class IdentityManager;
}  // namespace signin

namespace network {
struct ResourceRequest;
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace autofill {

class AccountInfoGetter;

namespace payments {

class PaymentsRequest;

// PaymentsNetworkInterfaceBase issues Payments RPCs and manages responses and
// failure conditions. Only one request may be active at a time. Initiating a
// new request will cancel a pending request.
class PaymentsNetworkInterfaceBase {
 public:
  // Cancels and clears the current `request_`.
  void CancelRequest();

  // Exposed for testing.
  void set_url_loader_factory_for_testing(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  void set_access_token_for_testing(std::string access_token);

 protected:
  friend class PaymentsNetworkInterfaceTestBase;

  // `url_loader_factory` is reference counted so it has no lifetime or
  // ownership requirements. `identity_manager` and `account_info_getter` must
  // all outlive `this`. Either delegate might be nullptr. `is_off_the_record`
  // denotes incognito mode.
  PaymentsNetworkInterfaceBase(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      AccountInfoGetter* account_info_getter,
      bool is_off_the_record = false);

  PaymentsNetworkInterfaceBase(const PaymentsNetworkInterfaceBase&) = delete;
  PaymentsNetworkInterfaceBase& operator=(const PaymentsNetworkInterfaceBase&) =
      delete;

  virtual ~PaymentsNetworkInterfaceBase();

  // Initiates a Payments request using the state in `request`, ensuring that an
  // OAuth token is available first. Takes ownership of `request`.
  void IssueRequest(std::unique_ptr<PaymentsRequest> request);

  // Creates `resource_request_` to be used later in
  // SetOAuth2TokenAndStartRequest().
  void InitializeResourceRequest();

  // Callback from `simple_url_loader_`.
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);
  void OnSimpleLoaderCompleteInternal(int response_code,
                                      const std::string& data);

  // Callback that handles a completed access token request.
  void AccessTokenFetchFinished(GoogleServiceAuthError error,
                                signin::AccessTokenInfo access_token_info);

  // Handles a completed access token request in the case of failure.
  void AccessTokenError(const GoogleServiceAuthError& error);

  // Initiates a new OAuth2 token request.
  void StartTokenFetch(bool invalidate_old);

  // Creates `simple_url_loader_`, adds the token to it, and starts the request.
  void SetOAuth2TokenAndStartRequest();

  // The URL loader factory for the request.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Provided in constructor; not owned by PaymentsNetworkInterface.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // Provided in constructor; not owned by PaymentsNetworkInterface.
  const raw_ptr<AccountInfoGetter> account_info_getter_;

  // The current request.
  std::unique_ptr<PaymentsRequest> request_;

  // The resource request being used to issue the current request.
  std::unique_ptr<network::ResourceRequest> resource_request_;

  // The URL loader being used to issue the current request.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // The OAuth2 token fetcher for any account.
  std::unique_ptr<signin::AccessTokenFetcher> token_fetcher_;

  // The OAuth2 token, or empty if not fetched.
  std::string access_token_;

  // Denotes incognito mode.
  // TODO(crbug.com/40888896): Remove this variable, as it should not be the
  // PaymentsNetworkInterface's responsibility to check if the user is off the
  // record. The sole responsibility of the PaymentsNetworkInterface is to send
  // requests to the Google Payments server.
  bool is_off_the_record_;

  // True if `request_` has already retried due to a 401 response from the
  // server.
  bool has_retried_authorization_;

  base::WeakPtrFactory<PaymentsNetworkInterfaceBase> weak_ptr_factory_{this};
};

}  // namespace payments
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_NETWORK_INTERFACE_BASE_H_
