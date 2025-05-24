// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/user_info_fetcher.h"

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/policy/core/common/policy_logger.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace {

static const char kAuthorizationHeaderFormat[] = "Bearer %s";

static std::string MakeAuthorizationHeader(const std::string& auth_token) {
  return base::StringPrintf(kAuthorizationHeaderFormat, auth_token.c_str());
}

static const char kLegacyGoogleApisHost[] = "www.googleapis.com";

// Replaces the host of the User Info API URL with the legacy host if needed.
// The legacy host is needed when the host is set to the new OAuth2 host which
// doesn't support the User Info API anymore. This is needed on iOS, which is
// the only platform that uses the new OAuth2 host at the moment.
GURL SwitchBackToLegacyHostIfNeeded(const GURL& url) {
  if (url.host() == "oauth2.googleapis.com") {
    GURL::Replacements replace_host;
    replace_host.SetHostStr(kLegacyGoogleApisHost);
    return url.ReplaceComponents(replace_host);
  }
  return url;
}

void RecordFetchStatus(policy::EnterpriseUserInfoFetchStatus status) {
  base::UmaHistogramEnumeration("Enterprise.UserInfoFetch.Status", status);
}

void RecordHttpErrorCode(int code) {
  base::UmaHistogramSparse("Enterprise.UserInfoFetch.HttpErrorCode", code);
}

}  // namespace

namespace policy {

UserInfoFetcher::UserInfoFetcher(
    Delegate* delegate,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : delegate_(delegate), url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK(delegate_);
}

UserInfoFetcher::~UserInfoFetcher() {
}

void UserInfoFetcher::Start(const std::string& access_token) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("user_info_fetcher", R"(
        semantics {
          sender: "Cloud Policy"
          description:
            "Calls to the Google Account service to check if the signed-in "
            "user is managed."
          trigger: "User signing in to Chrome."
          data: "OAuth2 token."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be controlled by Chrome settings, but users "
            "can sign out of Chrome to disable it."
          chrome_policy {
            SigninAllowed {
              policy_options {mode: MANDATORY}
              SigninAllowed: false
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  // TODO(crbug.com/40857586): Don't switch back to the legacy host once
  // oauth_user_info_url() returns a valid URL on iOS. We are currently working
  // on finding the best approach to deal with the new User Info API.
  resource_request->url = SwitchBackToLegacyHostIfNeeded(
      GaiaUrls::GetInstance()->oauth_user_info_url());
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      MakeAuthorizationHeader(access_token));
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&UserInfoFetcher::OnFetchComplete, base::Unretained(this)),
      1024 * 1024 /* 1 MiB */);
}

void UserInfoFetcher::OnFetchComplete(
    std::unique_ptr<std::string> unparsed_data) {
  std::unique_ptr<network::SimpleURLLoader> url_loader = std::move(url_loader_);

  GoogleServiceAuthError error = GoogleServiceAuthError::AuthErrorNone();
  if (url_loader->NetError() != net::OK) {
    RecordFetchStatus(EnterpriseUserInfoFetchStatus::kFailedWithNetworkError);
    if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
      int response_code = url_loader->ResponseInfo()->headers->response_code();
      DLOG_POLICY(WARNING, POLICY_AUTH)
          << "UserInfo request failed with HTTP code: " << response_code;
      error = GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED);
      RecordHttpErrorCode(response_code);
    } else {
      DLOG_POLICY(WARNING, POLICY_AUTH) << "UserInfo request failed";
      error =
          GoogleServiceAuthError::FromConnectionError(url_loader->NetError());
    }
  }
  if (error.state() != GoogleServiceAuthError::NONE) {
    delegate_->OnGetUserInfoFailure(error);
    return;
  }

  // Successfully fetched userinfo from the server - parse it and hand it off
  // to the delegate.
  DCHECK(unparsed_data);
  DVLOG_POLICY(1, POLICY_AUTH)
      << "Received UserInfo response: " << *unparsed_data;
  std::optional<base::Value> parsed_value =
      base::JSONReader::Read(*unparsed_data);
  if (parsed_value && parsed_value->is_dict()) {
    RecordFetchStatus(EnterpriseUserInfoFetchStatus::kSuccess);
    delegate_->OnGetUserInfoSuccess(parsed_value->GetDict());
  } else {
    EnterpriseUserInfoFetchStatus status =
        parsed_value ? EnterpriseUserInfoFetchStatus::kResponseIsNotDict
                     : EnterpriseUserInfoFetchStatus::kCantParseJsonInResponse;
    RecordFetchStatus(status);
    DLOG_POLICY(WARNING, POLICY_AUTH)
        << "Could not parse userinfo response from server: " << *unparsed_data;
    delegate_->OnGetUserInfoFailure(GoogleServiceAuthError(
        GoogleServiceAuthError::CONNECTION_FAILED));
  }
}

}  // namespace policy
