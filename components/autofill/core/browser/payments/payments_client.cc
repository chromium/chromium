// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_client.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_data_model.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/account_info_getter.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/payments_requests/get_details_for_enrollment_request.h"
#include "components/autofill/core/browser/payments/payments_requests/get_unmask_details_request.h"
#include "components/autofill/core/browser/payments/payments_requests/get_upload_details_request.h"
#include "components/autofill/core/browser/payments/payments_requests/migrate_cards_request.h"
#include "components/autofill/core/browser/payments/payments_requests/opt_change_request.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"
#include "components/autofill/core/browser/payments/payments_requests/select_challenge_option_request.h"
#include "components/autofill/core/browser/payments/payments_requests/unmask_card_request.h"
#include "components/autofill/core/browser/payments/payments_requests/update_virtual_card_enrollment_request.h"
#include "components/autofill/core/browser/payments/payments_requests/upload_card_request.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace autofill::payments {

namespace {

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

const char PaymentsClient::kRecipientName[] = "recipient_name";
const char PaymentsClient::kPhoneNumber[] = "phone_number";

PaymentsClient::UnmaskDetails::UnmaskDetails() = default;
PaymentsClient::UnmaskDetails::~UnmaskDetails() = default;
PaymentsClient::UnmaskDetails& PaymentsClient::UnmaskDetails::operator=(
    const PaymentsClient::UnmaskDetails& other) {
  unmask_auth_method = other.unmask_auth_method;
  offer_fido_opt_in = other.offer_fido_opt_in;
  if (other.fido_request_options.has_value()) {
    fido_request_options = other.fido_request_options->Clone();
  } else {
    fido_request_options.reset();
  }
  fido_eligible_card_ids = other.fido_eligible_card_ids;
  return *this;
}

PaymentsClient::UnmaskRequestDetails::UnmaskRequestDetails() = default;
PaymentsClient::UnmaskRequestDetails::UnmaskRequestDetails(
    const UnmaskRequestDetails& other) {
  *this = other;
}
PaymentsClient::UnmaskRequestDetails&
PaymentsClient::UnmaskRequestDetails::operator=(
    const PaymentsClient::UnmaskRequestDetails& other) {
  billing_customer_number = other.billing_customer_number;
  card = other.card;
  risk_data = other.risk_data;
  user_response = other.user_response;
  if (other.fido_assertion_info.has_value()) {
    fido_assertion_info = other.fido_assertion_info->Clone();
  } else {
    fido_assertion_info.reset();
  }
  context_token = other.context_token;
  otp = other.otp;
  last_committed_primary_main_frame_origin =
      other.last_committed_primary_main_frame_origin;
  selected_challenge_option = other.selected_challenge_option;
  client_behavior_signals = other.client_behavior_signals;
  return *this;
}
PaymentsClient::UnmaskRequestDetails::~UnmaskRequestDetails() = default;

PaymentsClient::UnmaskResponseDetails::UnmaskResponseDetails() = default;
PaymentsClient::UnmaskResponseDetails::UnmaskResponseDetails(
    const UnmaskResponseDetails& other) {
  *this = other;
}
PaymentsClient::UnmaskResponseDetails::~UnmaskResponseDetails() = default;
PaymentsClient::UnmaskResponseDetails&
PaymentsClient::UnmaskResponseDetails::operator=(
    const PaymentsClient::UnmaskResponseDetails& other) {
  real_pan = other.real_pan;
  if (other.fido_request_options.has_value()) {
    fido_request_options = other.fido_request_options->Clone();
  } else {
    fido_request_options.reset();
  }
  card_unmask_challenge_options = other.card_unmask_challenge_options;
  card_authorization_token = other.card_authorization_token;
  flow_status = other.flow_status;
  context_token = other.context_token;
  return *this;
}

PaymentsClient::OptChangeRequestDetails::OptChangeRequestDetails() = default;
PaymentsClient::OptChangeRequestDetails::OptChangeRequestDetails(
    const OptChangeRequestDetails& other) {
  app_locale = other.app_locale;
  reason = other.reason;
  if (other.fido_authenticator_response.has_value()) {
    fido_authenticator_response = other.fido_authenticator_response->Clone();
  } else {
    fido_authenticator_response.reset();
  }
  card_authorization_token = other.card_authorization_token;
}
PaymentsClient::OptChangeRequestDetails::~OptChangeRequestDetails() = default;

PaymentsClient::OptChangeResponseDetails::OptChangeResponseDetails() = default;
PaymentsClient::OptChangeResponseDetails::OptChangeResponseDetails(
    const OptChangeResponseDetails& other) {
  user_is_opted_in = other.user_is_opted_in;

  if (other.fido_creation_options.has_value()) {
    fido_creation_options = other.fido_creation_options->Clone();
  } else {
    fido_creation_options.reset();
  }
  if (other.fido_request_options.has_value()) {
    fido_request_options = other.fido_request_options->Clone();
  } else {
    fido_request_options.reset();
  }
}
PaymentsClient::OptChangeResponseDetails::~OptChangeResponseDetails() = default;

PaymentsClient::UploadRequestDetails::UploadRequestDetails() = default;
PaymentsClient::UploadRequestDetails::UploadRequestDetails(
    const UploadRequestDetails& other) = default;
PaymentsClient::UploadRequestDetails::~UploadRequestDetails() = default;

PaymentsClient::MigrationRequestDetails::MigrationRequestDetails() = default;
PaymentsClient::MigrationRequestDetails::MigrationRequestDetails(
    const MigrationRequestDetails& other) = default;
PaymentsClient::MigrationRequestDetails::~MigrationRequestDetails() = default;

PaymentsClient::SelectChallengeOptionRequestDetails::
    SelectChallengeOptionRequestDetails() = default;
PaymentsClient::SelectChallengeOptionRequestDetails::
    SelectChallengeOptionRequestDetails(
        const SelectChallengeOptionRequestDetails& other) = default;
PaymentsClient::SelectChallengeOptionRequestDetails::
    ~SelectChallengeOptionRequestDetails() = default;

PaymentsClient::GetDetailsForEnrollmentRequestDetails::
    GetDetailsForEnrollmentRequestDetails() = default;
PaymentsClient::GetDetailsForEnrollmentRequestDetails::
    GetDetailsForEnrollmentRequestDetails(
        const GetDetailsForEnrollmentRequestDetails& other) = default;
PaymentsClient::GetDetailsForEnrollmentRequestDetails::
    ~GetDetailsForEnrollmentRequestDetails() = default;

PaymentsClient::GetDetailsForEnrollmentResponseDetails::
    GetDetailsForEnrollmentResponseDetails() = default;
PaymentsClient::GetDetailsForEnrollmentResponseDetails::
    GetDetailsForEnrollmentResponseDetails(
        const GetDetailsForEnrollmentResponseDetails& other) = default;
PaymentsClient::GetDetailsForEnrollmentResponseDetails::
    ~GetDetailsForEnrollmentResponseDetails() = default;

PaymentsClient::UploadCardResponseDetails::UploadCardResponseDetails() =
    default;
PaymentsClient::UploadCardResponseDetails::~UploadCardResponseDetails() =
    default;

PaymentsClient::UpdateVirtualCardEnrollmentRequestDetails::
    UpdateVirtualCardEnrollmentRequestDetails() = default;
PaymentsClient::UpdateVirtualCardEnrollmentRequestDetails::
    UpdateVirtualCardEnrollmentRequestDetails(
        const UpdateVirtualCardEnrollmentRequestDetails&) = default;
PaymentsClient::UpdateVirtualCardEnrollmentRequestDetails&
PaymentsClient::UpdateVirtualCardEnrollmentRequestDetails::operator=(
    const UpdateVirtualCardEnrollmentRequestDetails&) = default;
PaymentsClient::UpdateVirtualCardEnrollmentRequestDetails::
    ~UpdateVirtualCardEnrollmentRequestDetails() = default;

PaymentsClient::PaymentsClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    AccountInfoGetter* account_info_getter,
    bool is_off_the_record)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      account_info_getter_(account_info_getter),
      is_off_the_record_(is_off_the_record),
      has_retried_authorization_(false) {}

PaymentsClient::~PaymentsClient() = default;

void PaymentsClient::Prepare() {
  if (access_token_.empty())
    StartTokenFetch(false);
}

void PaymentsClient::GetUnmaskDetails(
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            PaymentsClient::UnmaskDetails&)> callback,
    const std::string& app_locale) {
  IssueRequest(std::make_unique<GetUnmaskDetailsRequest>(
                   std::move(callback), app_locale,
                   account_info_getter_->IsSyncFeatureEnabled()),
               /*authenticate=*/true);
}

void PaymentsClient::UnmaskCard(
    const PaymentsClient::UnmaskRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            PaymentsClient::UnmaskResponseDetails&)> callback) {
  IssueRequest(
      std::make_unique<UnmaskCardRequest>(
          request_details, account_info_getter_->IsSyncFeatureEnabled(),
          std::move(callback)),
      /*authenticate=*/true);
}

void PaymentsClient::OptChange(
    const OptChangeRequestDetails request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            PaymentsClient::OptChangeResponseDetails&)>
        callback) {
  IssueRequest(std::make_unique<OptChangeRequest>(
                   request_details, std::move(callback),
                   account_info_getter_->IsSyncFeatureEnabled()),
               /*authenticate=*/true);
}

void PaymentsClient::GetUploadDetails(
    const std::vector<AutofillProfile>& addresses,
    const int detected_values,
    const std::vector<ClientBehaviorConstants>& client_behavior_signals,
    const std::string& app_locale,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const std::u16string&,
                            std::unique_ptr<base::Value::Dict>,
                            std::vector<std::pair<int, int>>)> callback,
    const int billable_service_number,
    const int64_t billing_customer_number,
    UploadCardSource upload_card_source) {
  IssueRequest(std::make_unique<GetUploadDetailsRequest>(
                   addresses, detected_values, client_behavior_signals,
                   account_info_getter_->IsSyncFeatureEnabled(), app_locale,
                   std::move(callback), billable_service_number,
                   billing_customer_number, upload_card_source),
               /*authenticate=*/base::FeatureList::IsEnabled(
                   features::kAutofillUpstreamAuthenticatePreflightCall));
}

void PaymentsClient::UploadCard(
    const PaymentsClient::UploadRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const UploadCardResponseDetails&)> callback) {
  IssueRequest(
      std::make_unique<UploadCardRequest>(
          request_details, account_info_getter_->IsSyncFeatureEnabled(),
          std::move(callback)),
      /*authenticate=*/true);
}

void PaymentsClient::MigrateCards(
    const MigrationRequestDetails& request_details,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    MigrateCardsCallback callback) {
  IssueRequest(
      std::make_unique<MigrateCardsRequest>(
          request_details, migratable_credit_cards,
          account_info_getter_->IsSyncFeatureEnabled(), std::move(callback)),
      /*authenticate=*/true);
}

void PaymentsClient::SelectChallengeOption(
    const SelectChallengeOptionRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const std::string&)> callback) {
  IssueRequest(std::make_unique<SelectChallengeOptionRequest>(
                   request_details, std::move(callback)),
               /*authenticate=*/true);
}

void PaymentsClient::GetVirtualCardEnrollmentDetails(
    const GetDetailsForEnrollmentRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const payments::PaymentsClient::
                                GetDetailsForEnrollmentResponseDetails&)>
        callback) {
  IssueRequest(std::make_unique<GetDetailsForEnrollmentRequest>(
                   request_details, std::move(callback)),
               /*authenticate=*/true);
}

void PaymentsClient::UpdateVirtualCardEnrollment(
    const UpdateVirtualCardEnrollmentRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult)> callback) {
  IssueRequest(std::make_unique<UpdateVirtualCardEnrollmentRequest>(
                   request_details, std::move(callback)),
               /*authenticate=*/true);
}

void PaymentsClient::CancelRequest() {
  request_.reset();
  resource_request_.reset();
  simple_url_loader_.reset();
  token_fetcher_.reset();
  access_token_.clear();
  has_retried_authorization_ = false;
}

void PaymentsClient::set_url_loader_factory_for_testing(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = std::move(url_loader_factory);
}

void PaymentsClient::IssueRequest(std::unique_ptr<PaymentsRequest> request,
                                  bool authenticate) {
  request_ = std::move(request);
  has_retried_authorization_ = false;

  InitializeResourceRequest();

  if (!authenticate) {
    StartRequest();
  } else if (access_token_.empty()) {
    StartTokenFetch(false);
  } else {
    SetOAuth2TokenAndStartRequest();
  }
}

void PaymentsClient::InitializeResourceRequest() {
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

void PaymentsClient::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
  }
  std::string data;
  if (response_body)
    data = std::move(*response_body);
  OnSimpleLoaderCompleteInternal(response_code, data);
}

void PaymentsClient::OnSimpleLoaderCompleteInternal(int response_code,
                                                    const std::string& data) {
  VLOG(2) << "Got data: " << data;

  AutofillClient::PaymentsRpcResult result =
      AutofillClient::PaymentsRpcResult::kSuccess;

  if (!request_)
    return;

  switch (response_code) {
    // Valid response.
    case net::HTTP_OK: {
      std::string error_code;
      std::string error_api_error_reason;
      absl::optional<base::Value> message_value = base::JSONReader::Read(data);
      if (message_value && message_value->is_dict()) {
        const auto* found_error_code =
            message_value->GetDict().FindStringByDottedPath("error.code");
        if (found_error_code)
          error_code = *found_error_code;

        const auto* found_error_reason =
            message_value->GetDict().FindStringByDottedPath(
                "error.api_error_reason");
        if (found_error_reason)
          error_api_error_reason = *found_error_reason;

        request_->ParseResponse(message_value->GetDict());
      }

      if (base::EqualsCaseInsensitiveASCII(error_api_error_reason,
                                           "virtual_card_temporary_error")) {
        result =
            AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure;
      } else if (base::EqualsCaseInsensitiveASCII(
                     error_api_error_reason, "virtual_card_permanent_error")) {
        result =
            AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure;
      } else if (request_->IsRetryableFailure(error_code)) {
        result = AutofillClient::PaymentsRpcResult::kTryAgainFailure;
      } else if (!error_code.empty() || !request_->IsResponseComplete()) {
        result = AutofillClient::PaymentsRpcResult::kPermanentFailure;
      }

      break;
    }

    case net::HTTP_UNAUTHORIZED: {
      if (has_retried_authorization_) {
        result = AutofillClient::PaymentsRpcResult::kPermanentFailure;
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
      result = AutofillClient::PaymentsRpcResult::kNetworkError;
      break;
    }

    // Handle anything else as a generic (permanent) failure.
    default: {
      result = AutofillClient::PaymentsRpcResult::kPermanentFailure;
      break;
    }
  }

  if (result != AutofillClient::PaymentsRpcResult::kSuccess) {
    VLOG(1) << "Payments returned error: " << response_code
            << " with data: " << data;
  }

  request_->RespondToDelegate(result);
}

void PaymentsClient::AccessTokenFetchFinished(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK(token_fetcher_);
  token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    AccessTokenError(error);
    return;
  }

  access_token_ = access_token_info.token;
  if (resource_request_)
    SetOAuth2TokenAndStartRequest();
}

void PaymentsClient::AccessTokenError(const GoogleServiceAuthError& error) {
  VLOG(1) << "Unhandled OAuth2 error: " << error.ToString();
  if (simple_url_loader_)
    simple_url_loader_.reset();
  if (request_)
    request_->RespondToDelegate(
        AutofillClient::PaymentsRpcResult::kPermanentFailure);
}

void PaymentsClient::StartTokenFetch(bool invalidate_old) {
  // We're still waiting for the last request to come back.
  if (!invalidate_old && token_fetcher_)
    return;

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
      base::BindOnce(&PaymentsClient::AccessTokenFetchFinished,
                     base::Unretained(this)),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void PaymentsClient::SetOAuth2TokenAndStartRequest() {
  DCHECK(resource_request_);
  resource_request_->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                       std::string("Bearer ") + access_token_);
  StartRequest();
}

void PaymentsClient::StartRequest() {
  DCHECK(resource_request_);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("payments_sync_cards", R"(
        semantics {
          sender: "Payments"
          description:
            "This service communicates with Google Payments servers to upload "
            "(save) or receive the user's credit card info."
          trigger:
            "Requests are triggered by a user action, such as selecting a "
            "masked server card from Chromium's credit card autofill dropdown, "
            "submitting a form which has credit card information, or accepting "
            "the prompt to save a credit card to Payments servers."
          data:
            "In case of save, a protocol buffer containing relevant address "
            "and credit card information which should be saved in Google "
            "Payments servers, along with user credentials. In case of load, a "
            "protocol buffer containing the id of the credit card to unmask, "
            "an encrypted cvc value, an optional updated card expiration date, "
            "and user credentials."
          destination: GOOGLE_OWNED_SERVICE
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

  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&PaymentsClient::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

}  // namespace autofill::payments
