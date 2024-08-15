// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_request.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_api.pb.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "components/password_manager/core/browser/leak_detection/single_lookup_response.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

void RecordLookupResponseResult(
    LeakDetectionRequest::LeakLookupResponseResult result) {
  base::UmaHistogramEnumeration(
      "PasswordManager.LeakDetection.LookupSingleLeakResponseResult", result);
}

constexpr char kAuthHeaderApiKey[] = "x-goog-api-key";
constexpr char kAuthHeaderBearer[] = "Bearer ";
constexpr char kPostMethod[] = "POST";
constexpr char kProtobufContentType[] = "application/x-protobuf";

google::internal::identity::passwords::leak::check::v1::
    LookupSingleLeakRequest::ClientUseCase
    InitiatorToClientUseCase(LeakDetectionInitiator initiator) {
  switch (initiator) {
    case LeakDetectionInitiator::kSignInCheck:
      return google::internal::identity::passwords::leak::check::v1::
          LookupSingleLeakRequest::ClientUseCase::
              LookupSingleLeakRequest_ClientUseCase_CHROME_SIGN_IN_CHECK;
    case LeakDetectionInitiator::kBulkSyncedPasswordsCheck:
      return google::internal::identity::passwords::leak::check::v1::
          LookupSingleLeakRequest::ClientUseCase::
              LookupSingleLeakRequest_ClientUseCase_CHROME_BULK_SYNCED_PASSWORDS_CHECK;
    case LeakDetectionInitiator::kEditCheck:
      return google::internal::identity::passwords::leak::check::v1::
          LookupSingleLeakRequest::ClientUseCase::
              LookupSingleLeakRequest_ClientUseCase_CHROME_EDIT_CHECK;
    case LeakDetectionInitiator::kIGABulkSyncedPasswordsCheck:
      return google::internal::identity::passwords::leak::check::v1::
          LookupSingleLeakRequest::ClientUseCase::
              LookupSingleLeakRequest_ClientUseCase_IGA_BULK_SYNCED_PASSWORDS_CHECK;
    case LeakDetectionInitiator::kClientUseCaseUnspecified:
      return google::internal::identity::passwords::leak::check::v1::
          LookupSingleLeakRequest::ClientUseCase::
              LookupSingleLeakRequest_ClientUseCase_CLIENT_USE_CASE_UNSPECIFIED;
    case LeakDetectionInitiator::kDesktopProactivePasswordCheckup:
      return google::internal::identity::passwords::leak::check::v1::
          LookupSingleLeakRequest::ClientUseCase::
              LookupSingleLeakRequest_ClientUseCase_CHROME_DESKTOP_SIGNED_IN_ON_DEVICE_PROACTIVE_PASSWORD_CHECKUP;
    case LeakDetectionInitiator::kIosProactivePasswordCheckup:
      return google::internal::identity::passwords::leak::check::v1::
          LookupSingleLeakRequest::ClientUseCase::
              LookupSingleLeakRequest_ClientUseCase_CHROME_IOS_SIGNED_IN_ON_DEVICE_PROACTIVE_PASSWORD_CHECKUP;
  }
  NOTREACHED();
}

google::internal::identity::passwords::leak::check::v1::LookupSingleLeakRequest
MakeLookupSingleLeakRequest(LookupSingleLeakPayload payload) {
  google::internal::identity::passwords::leak::check::v1::
      LookupSingleLeakRequest request;
  request.set_client_use_case(InitiatorToClientUseCase(payload.initiator));
  request.set_username_hash_prefix(std::move(payload.username_hash_prefix));
  request.set_username_hash_prefix_length(kUsernameHashPrefixLength);
  request.set_encrypted_lookup_hash(std::move(payload.encrypted_payload));
  return request;
}

}  // namespace

constexpr char LeakDetectionRequest::kLookupSingleLeakEndpoint[];

LeakDetectionRequest::LeakDetectionRequest() = default;

LeakDetectionRequest::~LeakDetectionRequest() = default;

void LeakDetectionRequest::LookupSingleLeak(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const std::optional<std::string>& access_token,
    const std::optional<std::string>& api_key,
    LookupSingleLeakPayload payload,
    LookupSingleLeakCallback callback) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("lookup_single_password_leak", R"(
        semantics {
          sender: "Leaked Credential Detector"
          description:
            "In order to inform signed-in users about leaked credentials this "
            "service uploads a prefix of the hashed username, as well as the "
            "encrypted username and password following a successful password "
            "form submission. The former is a 3 bytes of the hash and doesn't "
            "reveal the username to the server in any way. The latter is "
            "completely opaque to the server. The server responds with a list "
            "of encrypted leaked credentials matching the prefix of the hashed "
            "username, as well as with a re-encypted version of the uploaded "
            "username and password. Chrome then reverses its encryption on the "
            "re-encrypted credential and tries to find it in the list of "
            "leaked credentials. If a match is found, Chrome notifies the user "
            "and prompts them to change their credentials. Re-encryption part "
            "is for the privacy reason. The server can't read the user's "
            "password. At the same time the client can't read the "
            "usernames/passwords of other leaked accounts but only can check "
            "the current one.";
          trigger:
            "Following a successful password form submission by a signed-in "
            "user"
          data:
            "A hash prefix of the username and the encrypted username and "
            "password. An OAuth2 access token for the user account."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              owners: "//components/password_manager/OWNERS"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: CREDENTIALS
          }
          last_reviewed: "2023-08-14"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable this feature in Chrome's password "
            "settings. The feature is enabled by default."
          chrome_policy {
            PasswordLeakDetectionEnabled {
              PasswordLeakDetectionEnabled: false
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kLookupSingleLeakEndpoint);
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = kPostMethod;
  if (access_token.has_value()) {
    resource_request->headers.SetHeader(
        net::HttpRequestHeaders::kAuthorization,
        base::StrCat({kAuthHeaderBearer, access_token.value()}));
  }
  if (api_key.has_value()) {
    resource_request->headers.SetHeader(kAuthHeaderApiKey, api_key.value());
  }

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->AttachStringForUpload(
      MakeLookupSingleLeakRequest(std::move(payload)).SerializeAsString(),
      kProtobufContentType);
  simple_url_loader_->DownloadToString(
      url_loader_factory,
      base::BindOnce(&LeakDetectionRequest::OnLookupSingleLeakResponse,
                     base::Unretained(this), std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void LeakDetectionRequest::OnLookupSingleLeakResponse(
    LookupSingleLeakCallback callback,
    std::unique_ptr<std::string> response) {
  if (!response) {
    RecordLookupResponseResult(LeakLookupResponseResult::kFetchError);
    DLOG(ERROR) << "Empty Lookup Single Leak Response";
    int response_code = -1;
    LeakDetectionError error = LeakDetectionError::kNetworkError;
    if (simple_url_loader_->ResponseInfo() &&
        simple_url_loader_->ResponseInfo()->headers) {
      response_code =
          simple_url_loader_->ResponseInfo()->headers->response_code();
      DLOG(ERROR) << "HTTP Response Code: " << response_code;
      error = response_code == net::HTTP_TOO_MANY_REQUESTS
                  ? LeakDetectionError::kQuotaLimit
                  : LeakDetectionError::kInvalidServerResponse;
    }

    base::UmaHistogramSparse("PasswordManager.LeakDetection.HttpResponseCode",
                             response_code);

    int net_error = simple_url_loader_->NetError();
    DLOG(ERROR) << "Net Error: " << net::ErrorToString(net_error);

    std::move(callback).Run(nullptr, error);
    return;
  }

  base::UmaHistogramCounts1M(
      "PasswordManager.LeakDetection.SingleLeakResponseSize", response->size());
  google::internal::identity::passwords::leak::check::v1::
      LookupSingleLeakResponse leak_response;
  if (!leak_response.ParseFromString(*response)) {
    RecordLookupResponseResult(LeakLookupResponseResult::kParseError);
    DLOG(ERROR) << "Could not parse response: " << base::HexEncode(*response);
    std::move(callback).Run(nullptr,
                            LeakDetectionError::kInvalidServerResponse);
    return;
  }

  RecordLookupResponseResult(LeakLookupResponseResult::kSuccess);
  auto single_lookup_response = std::make_unique<SingleLookupResponse>();
  single_lookup_response->encrypted_leak_match_prefixes.assign(
      std::make_move_iterator(
          leak_response.mutable_encrypted_leak_match_prefix()->begin()),
      std::make_move_iterator(
          leak_response.mutable_encrypted_leak_match_prefix()->end()));
  single_lookup_response->reencrypted_lookup_hash =
      std::move(*leak_response.mutable_reencrypted_lookup_hash());
  base::UmaHistogramCounts100000(
      "PasswordManager.LeakDetection.SingleLeakResponsePrefixes",
      single_lookup_response->encrypted_leak_match_prefixes.size());
  std::move(callback).Run(std::move(single_lookup_response), std::nullopt);
}

}  // namespace password_manager
