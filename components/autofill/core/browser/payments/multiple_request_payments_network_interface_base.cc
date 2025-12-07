// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/multiple_request_payments_network_interface_base.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/payments/payments_access_token_fetcher.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/variations/net/variations_http_headers.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace autofill::payments {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("payments_autofill", R"(
        semantics {
          sender: "Payments"
          description:
            "This service communicates with Google Payments servers to "
            "save/retrieve the user's payment methods (e.g., credit cards, "
            "IBANs) as well as facilitate payments for Pix QR codes."
          trigger:
            "Requests are triggered by a user action, such as selecting a "
            "masked server card from Chromium's credit card autofill dropdown, "
            "submitting a form which has credit card information, or accepting "
            "the prompt to save a credit card to Payments servers."
          data:
            "In case of save, a protocol buffer containing relevant address, "
            "credit card and nickname information which should be saved in "
            "Google Payments servers, along with user credentials. In case of "
            "load, a protocol buffer containing the id of the credit card to "
            "unmask, an encrypted cvc value, an optional updated card "
            "expiration date, and user credentials. For virtual card numbers, "
            "the merchant domain and last 4 digits of cards are extracted from "
            "the web page. For virtual card verification, OTP and 3CSC etc "
            "are collected from the user and sent to Google Payments servers. "
            "For Pix, the QR code detected from the web page is sent to Google "
            "Payments servers. User agent is sent to Google Payments servers "
            "to determine the platform and return the corresponding response."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts: {
                email: "payments-autofill-team@google.com"
            }
          }
          user_data {
            type: USER_CONTENT
            type: CREDIT_CARD_DATA
            type: SENSITIVE_URL
            type: PROFILE_DATA
            type: WEB_CONTENT
            type: GAIA_ID
          }
          last_reviewed: "2025-02-24"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable this feature in Chromium settings by "
            "toggling 'Credit cards and addresses using Google Payments', "
            "under 'Advanced sync settings...'. This feature is enabled by "
            "default."
          chrome_policy {
            AutoFillEnabled {
              policy_options {mode: MANDATORY}
              AutoFillEnabled: false
            }
          }
        })");

GURL GetRequestUrl(const std::string& path) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch("sync-url")) {
    if (IsPaymentsProductionEnabled()) {
      LOG(ERROR) << "You are using production Payments but you specified a "
                    "--sync-url. You likely want to disable the sync sandbox "
                    "or switch to sandbox Payments. Both are controlled in "
                    "about:flags.";
    }
  } else if (!IsPaymentsProductionEnabled()) {
    LOG(ERROR) << "You are using sandbox Payments but you didn't specify a "
                  "--sync-url. You likely want to enable the sync sandbox "
                  "or switch to production Payments. Both are controlled in "
                  "about:flags.";
  }

  return GetBaseSecureUrl().Resolve(path);
}

}  // namespace

MultipleRequestPaymentsNetworkInterfaceBase::RequestOperation::RequestOperation(
    std::unique_ptr<PaymentsRequest> request,
    MultipleRequestPaymentsNetworkInterfaceBase& payments_network_interface)
    : request_(std::move(request)),
      payments_network_interface_(payments_network_interface),
      token_fetcher_(PaymentsAccessTokenFetcher(
          payments_network_interface.identity_manager())) {}

MultipleRequestPaymentsNetworkInterfaceBase::RequestOperation::
    ~RequestOperation() = default;

const RequestId& MultipleRequestPaymentsNetworkInterfaceBase::RequestOperation::
    StartOperation() {
  request_operation_id_ =
      RequestId(base::Uuid::GenerateRandomV4().AsLowercaseString());
  has_retried_authorization_ = false;
  token_fetcher_.GetAccessToken(
      /*invalidate_old=*/false,
      base::BindOnce(&MultipleRequestPaymentsNetworkInterfaceBase::
                         RequestOperation::AccessTokenFetchFinished,
                     weak_ptr_factory_.GetWeakPtr()));
  return request_operation_id_;
}

void MultipleRequestPaymentsNetworkInterfaceBase::RequestOperation::
    InvalidateOperation() {
  request_.reset();
}

void MultipleRequestPaymentsNetworkInterfaceBase::RequestOperation::
    AccessTokenFetchFinished(
        const std::variant<GoogleServiceAuthError, std::string>& result) {
  if (std::holds_alternative<GoogleServiceAuthError>(result)) {
    GoogleServiceAuthError error = std::get<GoogleServiceAuthError>(result);
    DVLOG(1) << "Unhandled access token error: " << error.ToString();
    if (simple_url_loader_) {
      simple_url_loader_.reset();
    }
    ReportOperationResult(PaymentsRpcResult::kPermanentFailure);
    return;
  }

  auto access_token = std::get<std::string>(result);
  SetAccessTokenAndStartRequest(access_token);
}

void MultipleRequestPaymentsNetworkInterfaceBase::RequestOperation::
    SetAccessTokenAndStartRequest(const std::string& access_token) {
  std::unique_ptr<network::ResourceRequest> resource_request =
      InitializeResourceRequest();
  // Set access token:
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      std::string("Bearer ") + access_token);

  // Start request:
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);
  simple_url_loader_->AttachStringForUpload(request_->GetRequestContent(),
                                            request_->GetRequestContentType());

  // Some request types specify a client-side timeout, in order to provide a
  // better user experience (e.g., avoid the user being unnecessarily blocked).
  //
  // The sandbox server is significantly slower than prod, so we never set a
  // client-side timeout when using sandbox, even if the request specifies one.
  if (request_->GetTimeout().has_value() && IsPaymentsProductionEnabled()) {
    CHECK(request_->GetTimeout()->is_positive());
    simple_url_loader_->SetTimeoutDuration(*request_->GetTimeout());
  }

  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      payments_network_interface_->url_loader_factory(),
      base::BindOnce(&MultipleRequestPaymentsNetworkInterfaceBase::
                         RequestOperation::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

std::unique_ptr<network::ResourceRequest>
MultipleRequestPaymentsNetworkInterfaceBase::RequestOperation::
    InitializeResourceRequest() {
  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  resource_request->url = GetRequestUrl(request_->GetRequestUrlPath());
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";

  // Add Chrome experiment state to the request headers.
  net::HttpRequestHeaders headers;
  // User is always signed-in to be able to upload card to Google Payments.
  variations::AppendVariationsHeader(
      resource_request->url,
      payments_network_interface_->is_off_the_record()
          ? variations::InIncognito::kYes
          : variations::InIncognito::kNo,
      variations::SignedIn::kYes, resource_request.get());
  return resource_request;
}

void MultipleRequestPaymentsNetworkInterfaceBase::RequestOperation::
    OnSimpleLoaderComplete(std::optional<std::string> response_body) {
  int response_code = -1;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
  } else if (simple_url_loader_->NetError() == net::ERR_TIMED_OUT) {
    response_code = net::ERR_TIMED_OUT;
  }

  OnSimpleLoaderCompleteInternal(response_code,
                                 std::move(response_body).value_or(""));
}

void MultipleRequestPaymentsNetworkInterfaceBase::RequestOperation::
    OnSimpleLoaderCompleteInternal(int response_code, const std::string& data) {
  DVLOG(2) << "Got data: " << data;

  PaymentsRpcResult result = PaymentsRpcResult::kSuccess;

  if (!request_) {
    payments_network_interface_->OnRequestFinished(request_operation_id_);
    return;
  }

  // Measure metrics on how often each type of request times out. We only want
  // to compare timeouts to otherwise successful results, to measure the effects
  // of client-side timeouts on successful saves.
  //
  // Note: This in theory could affect cases where we timed out when we would
  // have otherwise received HTTP_UNAUTHORIZED, but it's very unlikely that
  // HTTP_UNAUTHORIZED would take long enough to hit the client side timeout.
  if (request_->GetTimeout().has_value() &&
      (response_code == net::HTTP_OK || response_code == net::ERR_TIMED_OUT)) {
    base::UmaHistogramBoolean(
        base::StrCat({"Autofill.PaymentsNetworkInterface.",
                      request_->GetHistogramName(), ".ClientSideTimedOut"}),
        response_code == net::ERR_TIMED_OUT);
  }

  switch (response_code) {
    // Valid response.
    case net::HTTP_OK: {
      std::string error_code;
      std::string error_api_error_reason;
      std::optional<base::Value> message_value =
          base::JSONReader::Read(data, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
      if (message_value && message_value->is_dict()) {
        const auto* found_error_code =
            message_value->GetDict().FindStringByDottedPath("error.code");
        if (found_error_code) {
          error_code = *found_error_code;
        }

        const auto* found_error_reason =
            message_value->GetDict().FindStringByDottedPath(
                "error.api_error_reason");
        if (found_error_reason) {
          error_api_error_reason = *found_error_reason;
        }

        request_->ParseResponse(message_value->GetDict());
      }

      // Note that `error_api_error_reason` for virtual cards are mapped with
      // virtual card specific PaymentsRpcResults while those for card from
      // vendor(runtime retrieval) are mapped with generic temporary and
      // permanent PaymentsRpcResults.
      if (base::EqualsCaseInsensitiveASCII(error_api_error_reason,
                                           "virtual_card_temporary_error")) {
        result = PaymentsRpcResult::kVcnRetrievalTryAgainFailure;
      } else if (base::EqualsCaseInsensitiveASCII(
                     error_api_error_reason, "virtual_card_permanent_error")) {
        result = PaymentsRpcResult::kVcnRetrievalPermanentFailure;
      } else if (request_->IsRetryableFailure(error_code) ||
                 base::EqualsCaseInsensitiveASCII(
                     error_api_error_reason,
                     "card_from_vendor_temporary_error")) {
        result = PaymentsRpcResult::kTryAgainFailure;
      } else if (!error_code.empty() || !request_->IsResponseComplete() ||
                 base::EqualsCaseInsensitiveASCII(
                     error_api_error_reason,
                     "card_from_vendor_permanent_error")) {
        result = PaymentsRpcResult::kPermanentFailure;
      }

      break;
    }

    case net::HTTP_UNAUTHORIZED: {
      if (has_retried_authorization_) {
        result = PaymentsRpcResult::kPermanentFailure;
        break;
      }
      has_retried_authorization_ = true;

      token_fetcher_.GetAccessToken(
          /*invalidate_old=*/true,
          base::BindOnce(&MultipleRequestPaymentsNetworkInterfaceBase::
                             RequestOperation::AccessTokenFetchFinished,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }

    case net::HTTP_REQUEST_TIMEOUT: {
      result = PaymentsRpcResult::kNetworkError;
      break;
    }

    // This case occurs when the request hits the client-side timeout. This is
    // quite complex as the call could still complete on the server side, but we
    // were not willing to wait any longer for the server.
    case net::ERR_TIMED_OUT: {
      result = PaymentsRpcResult::kClientSideTimeout;
      break;
    }

    // Handle anything else as a generic (permanent) failure.
    default: {
      result = PaymentsRpcResult::kPermanentFailure;
      break;
    }
  }

  if (result != PaymentsRpcResult::kSuccess) {
    DVLOG(1) << "Payments returned error: " << response_code
             << " with data: " << data;
  }

  ReportOperationResult(result);
}

void MultipleRequestPaymentsNetworkInterfaceBase::RequestOperation::
    ReportOperationResult(PaymentsRpcResult result) {
  CHECK(request_);
  request_->RespondToDelegate(result);
  payments_network_interface_->OnRequestFinished(request_operation_id_);
}

MultipleRequestPaymentsNetworkInterfaceBase::
    MultipleRequestPaymentsNetworkInterfaceBase(
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        signin::IdentityManager& identity_manager,
        bool is_off_the_record)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      is_off_the_record_(is_off_the_record) {}

MultipleRequestPaymentsNetworkInterfaceBase::
    ~MultipleRequestPaymentsNetworkInterfaceBase() = default;

RequestId MultipleRequestPaymentsNetworkInterfaceBase::IssueRequest(
    std::unique_ptr<PaymentsRequest> request) {
  auto operation =
      std::make_unique<RequestOperation>(std::move(request), *this);
  RequestId id = operation->StartOperation();
  operations_[id] = std::move(operation);
  return id;
}

void MultipleRequestPaymentsNetworkInterfaceBase::CancelRequestWithId(
    const RequestId& id) {
  // Instead of deleting the operation with `id` directly, we will mark it
  // as invalidated so it does not report any result. The lifecycle of the
  // operation should only be managed by the PaymentsNetworkInterface (i.e. by
  // OnRequestFinished) internally to avoid accidental use-after-free.
  if (operations_.contains(id)) {
    operations_[id]->InvalidateOperation();
  }
}

void MultipleRequestPaymentsNetworkInterfaceBase::OnRequestFinished(
    RequestId& id) {
  operations_.erase(id);
}

}  // namespace autofill::payments
