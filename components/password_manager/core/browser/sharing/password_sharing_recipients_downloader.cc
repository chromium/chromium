// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/password_sharing_recipients_downloader.h"

#include <utility>

#include "base/command_line.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/sync/base/sync_util.h"
#include "google_apis/credentials_mode.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace password_manager {

namespace {

constexpr char kPasswordSharingRecipientsOAuthConsumerName[] =
    "PasswordSharingRecipients";
constexpr char kPasswordSharingRecipientsEndpoint[] =
    "password_sharing_recipients";
constexpr base::TimeDelta kRequestTimeout = base::Seconds(10);

void LogRequestResult(const network::SimpleURLLoader& url_loader) {
  int http_status_code = -1;
  if (url_loader.ResponseInfo() && url_loader.ResponseInfo()->headers) {
    http_status_code = url_loader.ResponseInfo()->headers->response_code();
  }
  const int net_error_code = url_loader.NetError();
  const bool request_succeeded =
      net_error_code == net::OK && http_status_code != -1;
  base::UmaHistogramSparse(
      "PasswordManager.PasswordSharingRecipients.ResponseOrErrorCode",
      request_succeeded ? http_status_code : net_error_code);
}

}  // namespace

PasswordSharingRecipientsDownloader::PasswordSharingRecipientsDownloader(
    version_info::Channel channel,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : channel_(channel),
      url_loader_factory_(std::move(url_loader_factory)),
      identity_manager_(identity_manager) {
  CHECK(url_loader_factory_);
  CHECK(identity_manager_);
}

PasswordSharingRecipientsDownloader::~PasswordSharingRecipientsDownloader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PasswordSharingRecipientsDownloader::Start(base::OnceClosure on_complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(on_complete);
  CHECK(!on_request_complete_callback_);

  on_request_complete_callback_ = std::move(on_complete);
  StartFetchingAccessToken();
}

std::optional<sync_pb::PasswordSharingRecipientsResponse>
PasswordSharingRecipientsDownloader::TakeResponse() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::move(response_);
}

int PasswordSharingRecipientsDownloader::GetNetError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return net_error_;
}

int PasswordSharingRecipientsDownloader::GetHttpError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return http_error_;
}

GoogleServiceAuthError PasswordSharingRecipientsDownloader::GetAuthError()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return last_auth_error_;
}

void PasswordSharingRecipientsDownloader::AccessTokenFetched(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Access token fetch complete, error state: "
           << static_cast<int>(error.state());

  base::UmaHistogramEnumeration(
      "PasswordManager.PasswordSharingRecipients.FetchAccessTokenResult",
      error.state(), GoogleServiceAuthError::NUM_STATES);

  CHECK(ongoing_access_token_fetch_);
  ongoing_access_token_fetch_.reset();

  last_auth_error_ = error;
  if (error.state() == GoogleServiceAuthError::NONE) {
    SendRequest(access_token_info);
    return;
  }

  // Do not retry on permanent error or if there was a retry before.
  if (error.IsPersistentError() || access_token_retried_) {
    std::move(on_request_complete_callback_).Run();
    return;
  }

  // Otherwise, retry fetching the access token.
  DVLOG(1) << "Retry fetching the access token";
  access_token_retried_ = true;
  StartFetchingAccessToken();
}

void PasswordSharingRecipientsDownloader::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(on_request_complete_callback_);
  CHECK(!ongoing_access_token_fetch_);
  CHECK(simple_url_loader_);

  LogRequestResult(*simple_url_loader_);
  net_error_ = simple_url_loader_->NetError();
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    http_error_ = simple_url_loader_->ResponseInfo()->headers->response_code();
  }

  if (!response_body || http_error_ != net::HTTP_OK) {
    DVLOG(1) << "Password sharing recipients request failed, http code: "
             << http_error_ << ", net error: " << net_error_;
    std::move(on_request_complete_callback_).Run();
    return;
  }

  sync_pb::PasswordSharingRecipientsResponse response;
  if (!response.ParseFromString(*response_body)) {
    DVLOG(1) << "Error parsing response from the sync server";
    std::move(on_request_complete_callback_).Run();
    return;
  }

  response_ = std::move(response);
  std::move(on_request_complete_callback_).Run();
}

void PasswordSharingRecipientsDownloader::SendRequest(
    const signin::AccessTokenInfo& access_token_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Leave the request empty for now because the OAuth token is sufficient to
  // return recipients.
  sync_pb::PasswordSharingRecipientsRequest request;
  std::string msg;
  request.SerializeToString(&msg);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("password_sharing_recipients", R"(
        semantics {
          sender: "Password Manager"
          description:
            "A network request to download possible recipients for password"
            " sharing."
          trigger: "When a user picks a password to be sent."
          data: "An OAuth2 access token."
          destination: GOOGLE_OWNED_SERVICE
          user_data {
            type: ACCESS_TOKEN
          }
          internal {
            contacts {
              email: "rushans@google.com"
            }
            contacts {
              email: "rgod@google.com"
            }
          }
          last_reviewed: "2023-06-19"
        }
        policy {
          cookies_allowed: NO
          setting: "Users can disable Chrome Sync by going into the profile "
            "settings and choosing to sign out."
          chrome_policy {
            SyncDisabled {
              policy_options {mode: MANDATORY}
              SyncDisabled: true
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetPasswordSharingRecipientsURL(channel_);
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode =
      google_apis::GetOmitCredentialsModeForGaiaRequests();
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf("Bearer %s", access_token_info.token.c_str()));
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                                      syncer::MakeUserAgentForSync(channel_));
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->AttachStringForUpload(msg, "application/x-protobuf");
  simple_url_loader_->SetTimeoutDuration(kRequestTimeout);
  simple_url_loader_->SetAllowHttpErrorResults(true);
  simple_url_loader_->SetRetryOptions(
      /*max_retries=*/2, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
                             network::SimpleURLLoader::RETRY_ON_5XX);
  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          &PasswordSharingRecipientsDownloader::OnSimpleLoaderComplete,
          base::Unretained(this)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

// static
GURL PasswordSharingRecipientsDownloader::GetPasswordSharingRecipientsURL(
    version_info::Channel channel) {
  GURL sync_service_url = syncer::GetSyncServiceURL(
      *base::CommandLine::ForCurrentProcess(), channel);
  std::string path = sync_service_url.path();
  if (path.empty() || *path.rbegin() != '/') {
    path += '/';
  }
  path += kPasswordSharingRecipientsEndpoint;
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return sync_service_url.ReplaceComponents(replacements);
}

void PasswordSharingRecipientsDownloader::StartFetchingAccessToken() {
  CHECK(!ongoing_access_token_fetch_);
  ongoing_access_token_fetch_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          kPasswordSharingRecipientsOAuthConsumerName, identity_manager_,
          signin::ScopeSet{GaiaConstants::kChromeSyncOAuth2Scope},
          base::BindOnce(
              &PasswordSharingRecipientsDownloader::AccessTokenFetched,
              base::Unretained(this)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          signin::ConsentLevel::kSignin);
}

}  // namespace password_manager
