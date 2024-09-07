// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_network_interface_base.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/payments/account_info_getter.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace autofill::payments {
namespace {

using PaymentsRpcResult = PaymentsAutofillClient::PaymentsRpcResult;

const char kTokenFetchId[] = "wallet_client";
const char kPaymentsOAuth2Scope[] =
    "https://www.googleapis.com/auth/wallet.chrome";

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

PaymentsNetworkInterfaceBase::PaymentsNetworkInterfaceBase(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    AccountInfoGetter* account_info_getter,
    bool is_off_the_record)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      account_info_getter_(account_info_getter),
      is_off_the_record_(is_off_the_record),
      has_retried_authorization_(false) {}

PaymentsNetworkInterfaceBase::~PaymentsNetworkInterfaceBase() = default;

void PaymentsNetworkInterfaceBase::CancelRequest() {
  request_.reset();
  resource_request_.reset();
  simple_url_loader_.reset();
  token_fetcher_.reset();
  access_token_.clear();
  has_retried_authorization_ = false;
}

void PaymentsNetworkInterfaceBase::set_url_loader_factory_for_testing(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = std::move(url_loader_factory);
}

void PaymentsNetworkInterfaceBase::set_access_token_for_testing(
    std::string access_token) {
  access_token_ = access_token;
}

void PaymentsNetworkInterfaceBase::IssueRequest(
    std::unique_ptr<PaymentsRequest> request) {
  request_ = std::move(request);
  has_retried_authorization_ = false;

  InitializeResourceRequest();

  if (access_token_.empty()) {
    StartTokenFetch(false);
  } else {
    SetOAuth2TokenAndStartRequest();
  }
}

void PaymentsNetworkInterfaceBase::InitializeResourceRequest() {
  resource_request_ = std::make_unique<network::ResourceRequest>();
  resource_request_->url = GetRequestUrl(request_->GetRequestUrlPath());
  resource_request_->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request_->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request_->method = "POST";

  // Add Chrome experiment state to the request headers.
  net::HttpRequestHeaders headers;
  // User is always signed-in to be able to upload card to Google Payments.
  variations::AppendVariationsHeader(
      resource_request_->url,
      is_off_the_record_ ? variations::InIncognito::kYes
                         : variations::InIncognito::kNo,
      variations::SignedIn::kYes, resource_request_.get());
}

void PaymentsNetworkInterfaceBase::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
  } else if (simple_url_loader_->NetError() == net::ERR_TIMED_OUT) {
    response_code = net::ERR_TIMED_OUT;
  }

  std::string data;
  if (response_body) {
    data = std::move(*response_body);
  }

  OnSimpleLoaderCompleteInternal(response_code, data);
}

void PaymentsNetworkInterfaceBase::OnSimpleLoaderCompleteInternal(
    int response_code,
    const std::string& data) {
  VLOG(2) << "Got data: " << data;

  PaymentsRpcResult result = PaymentsRpcResult::kSuccess;

  if (!request_) {
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
      std::optional<base::Value> message_value = base::JSONReader::Read(data);
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

      if (base::EqualsCaseInsensitiveASCII(error_api_error_reason,
                                           "virtual_card_temporary_error")) {
        result = PaymentsRpcResult::kVcnRetrievalTryAgainFailure;
      } else if (base::EqualsCaseInsensitiveASCII(
                     error_api_error_reason, "virtual_card_permanent_error")) {
        result = PaymentsRpcResult::kVcnRetrievalPermanentFailure;
      } else if (request_->IsRetryableFailure(error_code)) {
        result = PaymentsRpcResult::kTryAgainFailure;
      } else if (!error_code.empty() || !request_->IsResponseComplete()) {
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

      InitializeResourceRequest();
      StartTokenFetch(true);
      return;
    }

    // TODO(estade): is this actually how network connectivity issues are
    // reported?
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
    VLOG(1) << "Payments returned error: " << response_code
            << " with data: " << data;
  }

  request_->RespondToDelegate(result);
}

void PaymentsNetworkInterfaceBase::AccessTokenFetchFinished(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK(token_fetcher_);
  token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    AccessTokenError(error);
    return;
  }

  access_token_ = access_token_info.token;
  if (resource_request_) {
    SetOAuth2TokenAndStartRequest();
  }
}

void PaymentsNetworkInterfaceBase::AccessTokenError(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "Unhandled OAuth2 error: " << error.ToString();
  if (simple_url_loader_) {
    simple_url_loader_.reset();
  }
  if (request_) {
    request_->RespondToDelegate(PaymentsRpcResult::kPermanentFailure);
  }
}

void PaymentsNetworkInterfaceBase::StartTokenFetch(bool invalidate_old) {
  // We're still waiting for the last request to come back.
  if (!invalidate_old && token_fetcher_) {
    return;
  }

  DCHECK(account_info_getter_);

  signin::ScopeSet payments_scopes;
  payments_scopes.insert(kPaymentsOAuth2Scope);
  CoreAccountId account_id =
      account_info_getter_->GetAccountInfoForPaymentsServer().account_id;
  if (invalidate_old) {
    DCHECK(!access_token_.empty());
    identity_manager_->RemoveAccessTokenFromCache(account_id, payments_scopes,
                                                  access_token_);
  }
  access_token_.clear();
  token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      account_id, kTokenFetchId, payments_scopes,
      base::BindOnce(&PaymentsNetworkInterfaceBase::AccessTokenFetchFinished,
                     base::Unretained(this)),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void PaymentsNetworkInterfaceBase::SetOAuth2TokenAndStartRequest() {
  // Set OAuth2 token:
  DCHECK(resource_request_);
  resource_request_->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                       std::string("Bearer ") + access_token_);

  // Start request:
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("payments_sync_cards", R"(
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
          last_reviewed: "2024-01-24"
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
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request_), traffic_annotation);
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
      url_loader_factory_.get(),
      base::BindOnce(&PaymentsNetworkInterfaceBase::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

}  // namespace autofill::payments
