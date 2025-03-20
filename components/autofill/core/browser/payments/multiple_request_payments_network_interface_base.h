// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MULTIPLE_REQUEST_PAYMENTS_NETWORK_INTERFACE_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MULTIPLE_REQUEST_PAYMENTS_NETWORK_INTERFACE_BASE_H_

#include <memory>
#include <string>
#include <variant>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/payments/payments_access_token_fetcher.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace network {
struct ResourceRequest;
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace autofill::payments {

using PaymentsRpcResult = PaymentsAutofillClient::PaymentsRpcResult;
using RequestId = base::StrongAlias<struct RequestIdTag, std::string>;

class PaymentsRequest;

// MultipleRequestPaymentsNetworkInterfaceBase issues Payments RPCs and manages
// responses and failure conditions. Multiple requests may be active at a time.
class MultipleRequestPaymentsNetworkInterfaceBase {
 public:
  // Class that is responsible for managing one request inside the
  // `MultipleRequestPaymentsNetworkInterfaceBase`.
  class RequestOperation {
   public:
    RequestOperation(std::unique_ptr<PaymentsRequest> request,
                     MultipleRequestPaymentsNetworkInterfaceBase&
                         payments_network_interface);
    RequestOperation(const RequestOperation&) = delete;
    RequestOperation& operator=(const RequestOperation&) = delete;
    ~RequestOperation();

    const RequestId& StartOperation();

    void OnSimpleLoaderCompleteInternalForTesting(int response_code,
                                                  const std::string& data) {
      OnSimpleLoaderCompleteInternal(response_code, data);
    }

   private:
    // Function invoked when access token is fetched.
    void AccessTokenFetchFinished(
        const std::variant<GoogleServiceAuthError, std::string>& result);

    // Helper function to complete the request with the access token and start
    // the request.
    void SetAccessTokenAndStartRequest(const std::string& access_token);

    // Helper function to create `resource_request_` to be used in tne
    // SetAccessTokenAndStartRequest().
    [[nodiscard]] std::unique_ptr<network::ResourceRequest>
    InitializeResourceRequest();

    // Callback from `simple_url_loader_`.
    void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);
    void OnSimpleLoaderCompleteInternal(int response_code,
                                        const std::string& data);

    // Invoked when the operation is finished with the `result`. Will notify
    // the `payments_network_interface_`.
    void ReportOperationResult(
        PaymentsAutofillClient::PaymentsRpcResult result);

    // The request in this operation.
    std::unique_ptr<PaymentsRequest> request_;

    // The PaymentsNetworkInterface that owns and handles `this` operation.
    const raw_ref<MultipleRequestPaymentsNetworkInterfaceBase>
        payments_network_interface_;

    // The unique id for `this`. Generated in the constructor. It is shared
    // with the class which initiates the request.
    RequestId request_operation_id_;

    // The access token fetcher to fetch latest access token.
    PaymentsAccessTokenFetcher token_fetcher_;

    // The URL loader being used to issue the `request`.
    std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

    // True if `request_` has already retried due to an HTTP 401 response from
    // the server.
    bool has_retried_authorization_ = false;

    base::WeakPtrFactory<
        MultipleRequestPaymentsNetworkInterfaceBase::RequestOperation>
        weak_ptr_factory_{this};
  };

  // `url_loader_factory` is reference counted so it has no lifetime or
  // ownership requirements. `identity_manager`  must outlive `this`.
  // `is_off_the_record` denotes incognito mode.
  MultipleRequestPaymentsNetworkInterfaceBase(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager& identity_manager,
      bool is_off_the_record = false);

  MultipleRequestPaymentsNetworkInterfaceBase(
      const MultipleRequestPaymentsNetworkInterfaceBase&) = delete;
  MultipleRequestPaymentsNetworkInterfaceBase& operator=(
      const MultipleRequestPaymentsNetworkInterfaceBase&) = delete;

  virtual ~MultipleRequestPaymentsNetworkInterfaceBase();

  // TODO: crbug.com/362785295 - Maybe add logic to prefetch the access token if
  // necessary.

  // Initiates a RequestOperation using the info in `request` and start the
  // operation, ensuring that an access token is available before sending the
  // request. Takes ownership of `request`.
  RequestId IssueRequest(std::unique_ptr<PaymentsRequest> request);

  // Cancels all current requests and resets the
  // MultipleRequestPaymentsNetworkInterfaceBase.
  void CancelRequests();

  // Cancels only the request with `id`.
  void CancelRequestWithId(const RequestId& id);

  bool is_off_the_record() const { return is_off_the_record_; }

  signin::IdentityManager& identity_manager() const {
    return identity_manager_.get();
  }

  network::SharedURLLoaderFactory* url_loader_factory() const {
    return url_loader_factory_.get();
  }

  // Caller of this function should not modify the operations map directly.
  const std::unordered_map<RequestId, std::unique_ptr<RequestOperation>>&
  operations_for_testing() const {
    return operations_;
  }

 private:
  // Function invoked when a request (operation) is finished.
  void OnRequestFinished(RequestId& id);

  // The URL loader factory for the request.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  const raw_ref<signin::IdentityManager> identity_manager_;

  // Denotes incognito mode.
  bool is_off_the_record_;

  // The map holding reference to all owned, active request operations.
  std::unordered_map<RequestId, std::unique_ptr<RequestOperation>> operations_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_MULTIPLE_REQUEST_PAYMENTS_NETWORK_INTERFACE_BASE_H_
