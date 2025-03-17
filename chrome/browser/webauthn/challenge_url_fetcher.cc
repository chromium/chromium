// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/challenge_url_fetcher.h"

#include "base/functional/bind.h"
#include "components/device_event_log/device_event_log.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

// The specification says a challenge must be at least 16 bytes but does not
// provide an upper bound. 1KiB should contain any reasonable challenge.
const size_t kMaxChallengeSize = 1024;
const size_t kMinChallengeSize = 16;

constexpr char kChallengeContentType[] = "application/x-webauthn-challenge";

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("webauthn_challenge_fetch", R"(
        semantics {
          sender: "Web Authentication Client"
          description:
            "When a user attempts to sign in to a site using a WebAuthn "
            "credential, the site can include a challenge in the request, or "
            "else a URL from which the challenge can be asynchronously "
            "fetched. A challenge is a random byte string that protects "
            "against replay attacks. This request is the challenge request "
            "to the provided URL."
          trigger:
            "A user attempts a Web Authentication sign-in on a web site."
          data:
            "In response to this HTTP GET, the site returns a random string "
            "of bytes that will be signed over and embedded in the response "
            "to the WebAuthn request, if that request is successful."
          internal {
            contacts {
              email: "chrome-webauthn@google.com"
            }
          }
          user_data {
            type: NONE
          }
          destination: WEBSITE
          last_reviewed: "2024-12-03"
        }
        policy {
          cookies_allowed: NO
          setting: "This cannot be disabled."
          policy_exception_justification:
            "This request is part of a web platform API. No user data is "
            "passed to the server or received in the response."
        })");
}  // namespace

ChallengeUrlFetcher::ChallengeUrlFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}
ChallengeUrlFetcher::~ChallengeUrlFetcher() = default;

void ChallengeUrlFetcher::FetchUrl(GURL challenge_url,
                                   base::OnceClosure callback) {
  CHECK(!url_loader_);
  CHECK(challenge_url.is_valid());
  CHECK_EQ(state_, State::kFetchNotStarted);

  FIDO_LOG(EVENT) << "Fetching request challenge from " << challenge_url;

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = std::move(challenge_url);

  resource_request->redirect_mode = network::mojom::RedirectMode::kError;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kTrafficAnnotation);

  state_ = State::kFetchInProgress;
  callback_ = std::move(callback);

  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&ChallengeUrlFetcher::OnChallengeReceived,
                     weak_ptr_factory_.GetWeakPtr()),
      kMaxChallengeSize);
}

void ChallengeUrlFetcher::OnChallengeReceived(
    std::optional<std::string> response_body) {
  CHECK_EQ(state_, State::kFetchInProgress);

  state_ = State::kError;

  auto* response_info = url_loader_->ResponseInfo();
  int response_code = response_info && response_info->headers
                          ? response_info->headers->response_code()
                          : url_loader_->NetError();
  std::string content_type;
  if (response_info && response_info->headers) {
    response_info->headers->GetMimeType(&content_type);
  }

  url_loader_.reset();

  if (response_code != net::HTTP_OK) {
    FIDO_LOG(ERROR) << "Challenge fetch returned HTTP error " << response_code;
  } else if (!response_body.has_value()) {
    FIDO_LOG(ERROR) << "Challenge fetch returned no data";
  } else if (content_type != kChallengeContentType) {
    FIDO_LOG(ERROR)
        << "Challenge fetch encountered wrong content-type in response";
  } else if (response_body->size() < kMinChallengeSize) {
    FIDO_LOG(ERROR) << "Challenge obtained from fetch is too small";
  } else {
    challenge_ =
        std::vector<uint8_t>(response_body->begin(), response_body->end());
    state_ = State::kChallengeReceived;
  }
  std::move(callback_).Run();
}

base::expected<std::vector<uint8_t>,
               ChallengeUrlFetcher::ChallengeNotAvailableReason>
ChallengeUrlFetcher::GetChallenge() {
  switch (state_) {
    case State::kFetchNotStarted:
      return base::unexpected(ChallengeNotAvailableReason::kNotRequested);
    case State::kFetchInProgress:
      return base::unexpected(
          ChallengeNotAvailableReason::kWaitingForChallenge);
    case State::kChallengeReceived:
      return base::ok(challenge_);
    case State::kError:
      return base::unexpected(
          ChallengeNotAvailableReason::kErrorFetchingChallenge);
  }
}
