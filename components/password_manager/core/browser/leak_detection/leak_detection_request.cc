// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_request.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_api.pb.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "components/password_manager/core/browser/leak_detection/single_lookup_response.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

void RecordLookupResponseResult(
    LeakDetectionRequest::LeakLookupResponseResult result) {
  base::UmaHistogramEnumeration(
      "PasswordManager.LeakDetection.LookupSingleLeakResponseResult", result);
}

constexpr char kAuthHeaderBearer[] = "Bearer ";
constexpr char kPostMethod[] = "POST";
constexpr char kProtobufContentType[] = "application/x-protobuf";

google::internal::identity::passwords::leak::check::v1::LookupSingleLeakRequest
MakeLookupSingleLeakRequest(std::string username_hash_prefix,
                            std::string encrypted_payload) {
  google::internal::identity::passwords::leak::check::v1::
      LookupSingleLeakRequest request;
  request.set_username_hash_prefix(std::move(username_hash_prefix));
  request.set_username_hash_prefix_length(kUsernameHashPrefixLength);
  request.set_encrypted_lookup_hash(std::move(encrypted_payload));
  return request;
}

}  // namespace

constexpr char LeakDetectionRequest::kLookupSingleLeakEndpoint[];

LeakDetectionRequest::LeakDetectionRequest() = default;

LeakDetectionRequest::~LeakDetectionRequest() = default;

void LeakDetectionRequest::LookupSingleLeak(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const std::string& access_token,
    std::string username_hash_prefix,
    std::string encrypted_payload,
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
            "password."
          destination: GOOGLE_OWNED_SERVICE
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
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StrCat({kAuthHeaderBearer, access_token}));

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->AttachStringForUpload(
      MakeLookupSingleLeakRequest(std::move(username_hash_prefix),
                                  std::move(encrypted_payload))
          .SerializeAsString(),
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
    if (simple_url_loader_->ResponseInfo() &&
        simple_url_loader_->ResponseInfo()->headers) {
      response_code =
          simple_url_loader_->ResponseInfo()->headers->response_code();
      DLOG(ERROR) << "HTTP Response Code: " << response_code;
    }

    base::UmaHistogramSparse("PasswordManager.LeakDetection.HttpResponseCode",
                             response_code);

    int net_error = simple_url_loader_->NetError();
    DLOG(ERROR) << "Net Error: " << net::ErrorToString(net_error);
    // Network error codes are negative. See: src/net/base/net_error_list.h.
    base::UmaHistogramSparse("PasswordManager.LeakDetection.NetErrorCode",
                             -net_error);

    std::move(callback).Run(nullptr);
    return;
  }

  base::UmaHistogramCounts1M(
      "PasswordManager.LeakDetection.SingleLeakResponseSize", response->size());
  google::internal::identity::passwords::leak::check::v1::
      LookupSingleLeakResponse leak_response;
  if (!leak_response.ParseFromString(*response)) {
    RecordLookupResponseResult(LeakLookupResponseResult::kParseError);
    DLOG(ERROR) << "Could not parse response: "
                << base::HexEncode(response->data(), response->size());
    std::move(callback).Run(nullptr);
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
  std::move(callback).Run(std::move(single_lookup_response));
}

}  // namespace password_manager
