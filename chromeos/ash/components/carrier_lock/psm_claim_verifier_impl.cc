// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/carrier_lock/psm_claim_verifier_impl.h"

#include "base/logging.h"
#include "base/values.h"
#include "google_apis/google_api_keys.h"
#include "url/gurl.h"

namespace psm_rlwe = private_membership::rlwe;

namespace ash::carrier_lock {

namespace {

// const values
constexpr size_t kMaxOprfResponseSizeBytes = 1 << 20;   // 1MB;
constexpr size_t kMaxQueryResponseSizeBytes = 5 << 20;  // 5MB;
constexpr base::TimeDelta kRequestTimeoutSeconds = base::Seconds(60);

// PSM server URL
#define kPrivateSetBaseUrl "https://privatemembership-pa.googleapis.com"
const char kPrivateSetOprfUrl[] = kPrivateSetBaseUrl "/v1/membership:oprf";
const char kPrivateSetQueryUrl[] = kPrivateSetBaseUrl "/v1/membership:query";

// Traffic annotation for membership check
const net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("carrier_lock_manager_check_membership",
                                        R"(
        semantics {
          sender: "Carrier Lock manager"
          description:
            "Check if the device is a member of Carrier Lock group on the "
            "Private Set Membership service."
          trigger: "Carrier Lock manager makes this network request at first "
                   "boot to check if the cellular modem should be locked."
          data: "Manufacturer, model, serial of the device and API key."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
                email: "cros-cellular-core@google.com"
            }
          }
          user_data {
            type: DEVICE_ID
            type: HW_OS_INFO
          }
          last_reviewed: "2023-10-24"
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification: "Carrier Lock is always enforced."
        })");

std::vector<psm_rlwe::RlwePlaintextId> GetPsmDeviceId(std::string serial,
                                                      std::string manufacturer,
                                                      std::string model) {
  std::string psm_id = base::JoinString({manufacturer, model, serial}, ",");

  psm_rlwe::RlwePlaintextId psm_rlwe_id;
  psm_rlwe_id.set_sensitive_id(psm_id);
  psm_rlwe_id.set_non_sensitive_id(psm_id);

  return {psm_rlwe_id};
}

}  // namespace

PsmClaimVerifierImpl::PsmClaimVerifierImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

PsmClaimVerifierImpl::~PsmClaimVerifierImpl() = default;

void PsmClaimVerifierImpl::CheckPsmClaim(std::string serial,
                                         std::string manufacturer,
                                         std::string model,
                                         Callback callback) {
  ::rlwe::StatusOr<std::unique_ptr<psm_rlwe::PrivateMembershipRlweClient>>
      client_or_status;

  if (claim_callback_) {
    LOG(ERROR) << "PsmClaimVerifierImpl cannot handle multiple requests.";
    std::move(callback).Run(Result::kHandlerBusy);
    return;
  }

  std::vector<psm_rlwe::RlwePlaintextId> psm_rlwe_ids =
      GetPsmDeviceId(serial, manufacturer, model);
  api_key_ = google_apis::GetAPIKey();
  claim_callback_ = std::move(callback);

  if (!is_testing()) {
    client_or_status = psm_rlwe::PrivateMembershipRlweClient::Create(
        psm_rlwe::RlweUseCase::CROS_SIM_LOCK, psm_rlwe_ids);
  } else {
    client_or_status = psm_rlwe::PrivateMembershipRlweClient::CreateForTesting(
        psm_rlwe::RlweUseCase::CROS_SIM_LOCK, psm_rlwe_ids, "ec_cipher_key",
        "seed4567890123456789012345678912");
  }

  if (!client_or_status.ok()) {
    LOG(ERROR) << "Failed to create PSM client";
    ReturnError(Result::kCreatePsmClientFailed);
    return;
  }
  psm_client_ = std::move(client_or_status.value());

  SendOprfRequest();
}

bool PsmClaimVerifierImpl::GetMembership() {
  return membership_response_.is_member();
}

void PsmClaimVerifierImpl::SetupUrlLoader(std::string& request,
                                          const char* url) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(url);
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->headers.SetHeader("x-goog-api-key", api_key_);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/x-protobuf");

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  if (!url_loader_factory_ || !simple_url_loader_) {
    LOG(ERROR) << "Failed to create URL loader";
    ReturnError(Result::kInitializationFailed);
    return;
  }

  simple_url_loader_->AttachStringForUpload(request, "application/x-protobuf");
  simple_url_loader_->SetTimeoutDuration(kRequestTimeoutSeconds);
  simple_url_loader_->SetAllowHttpErrorResults(true);
}

void PsmClaimVerifierImpl::SendOprfRequest() {
  ::rlwe::StatusOr<psm_rlwe::PrivateMembershipRlweOprfRequest>
      oprf_request_or_status = psm_client_->CreateOprfRequest();
  if (!oprf_request_or_status.ok()) {
    LOG(ERROR) << "Failed to create PSM OPRF request";
    ReturnError(Result::kCreateOprfRequestFailed);
    return;
  }

  psm_rlwe::PrivateMembershipRlweOprfRequest oprf_request =
      oprf_request_or_status.value();

  std::string request_body;
  oprf_request.SerializeToString(&request_body);

  SetupUrlLoader(request_body, kPrivateSetOprfUrl);
  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PsmClaimVerifierImpl::OnCheckMembershipOprfDone,
                     weak_ptr_factory_.GetWeakPtr()),
      kMaxOprfResponseSizeBytes);
}

void PsmClaimVerifierImpl::OnCheckMembershipOprfDone(
    std::unique_ptr<std::string> response_body) {
  simple_url_loader_.reset();

  if (!response_body) {
    LOG(ERROR) << "Error in response to PSM OPRF request";
    ReturnError(Result::kConnectionError);
    return;
  }

  psm_rlwe::PrivateMembershipRlweOprfResponse psm_oprf_response;
  if (!psm_oprf_response.ParseFromString(*response_body)) {
    LOG(ERROR) << "Failed to parse PSM OPRF response";
    ReturnError(Result::kInvalidOprfReply);
    return;
  }

  // Generate PSM Query request body.
  const ::rlwe::StatusOr<psm_rlwe::PrivateMembershipRlweQueryRequest>
      query_request_or_status =
          psm_client_->CreateQueryRequest(psm_oprf_response);
  if (!query_request_or_status.ok()) {
    LOG(ERROR) << "Failed to create PSM Query request"
               << query_request_or_status.status();
    ReturnError(Result::kCreateQueryRequestFailed);
    return;
  }

  psm_rlwe::PrivateMembershipRlweQueryRequest query_request =
      query_request_or_status.value();

  std::string request_body;
  query_request.SerializeToString(&request_body);

  SetupUrlLoader(request_body, kPrivateSetQueryUrl);
  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PsmClaimVerifierImpl::OnCheckMembershipQueryDone,
                     weak_ptr_factory_.GetWeakPtr()),
      kMaxQueryResponseSizeBytes);
}

void PsmClaimVerifierImpl::OnCheckMembershipQueryDone(
    std::unique_ptr<std::string> response_body) {
  simple_url_loader_.reset();

  if (!response_body) {
    LOG(ERROR) << "Error in response to PSM Query request";
    ReturnError(Result::kConnectionError);
    return;
  }

  psm_rlwe::PrivateMembershipRlweQueryResponse psm_query_response;
  if (!psm_query_response.ParseFromString(*response_body)) {
    LOG(ERROR) << "Failed to parse PSM Query response";
    ReturnError(Result::kInvalidQueryReply);
    return;
  }

  ::rlwe::StatusOr<psm_rlwe::RlweMembershipResponses> response_or_status =
      psm_client_->ProcessQueryResponse(psm_query_response);
  if (!response_or_status.ok()) {
    LOG(ERROR) << "Failed to process PSM Query response";
    ReturnError(Result::kInvalidQueryReply);
    return;
  }

  // Ensure the existence of one membership response. Then, verify that it is
  // regarding the current PSM ID.
  psm_rlwe::RlweMembershipResponses rlwe_membership_responses =
      response_or_status.value();
  if (rlwe_membership_responses.membership_responses_size() != 1) {
    LOG(ERROR) << "Invalid membership response from PSM server";
    ReturnError(Result::kInvalidResponse);
    return;
  }

  membership_response_ =
      rlwe_membership_responses.membership_responses(0).membership_response();

  ReturnSuccess();
}

void PsmClaimVerifierImpl::ReturnError(Result err) {
  std::move(claim_callback_).Run(err);
}

void PsmClaimVerifierImpl::ReturnSuccess() {
  std::move(claim_callback_).Run(Result::kSuccess);
}

}  // namespace ash::carrier_lock
