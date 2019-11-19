// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/user_info_fetcher.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {

static const char kAuthorizationHeaderFormat[] = "Bearer %s";

static std::string MakeAuthorizationHeader(const std::string& auth_token) {
  return base::StringPrintf(kAuthorizationHeaderFormat, auth_token.c_str());
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
  resource_request->url = GaiaUrls::GetInstance()->oauth_user_info_url();
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
    if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
      int response_code = url_loader->ResponseInfo()->headers->response_code();
      DLOG(WARNING) << "UserInfo request failed with HTTP code: "
                    << response_code;
      error = GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED);
    } else {
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
  DVLOG(1) << "Received UserInfo response: " << *unparsed_data;
  std::unique_ptr<base::Value> parsed_value =
      base::JSONReader::ReadDeprecated(*unparsed_data);
  base::DictionaryValue* dict;
  if (parsed_value.get() && parsed_value->GetAsDictionary(&dict)) {
    delegate_->OnGetUserInfoSuccess(dict);
  } else {
    NOTREACHED() << "Could not parse userinfo response from server";
    delegate_->OnGetUserInfoFailure(GoogleServiceAuthError(
        GoogleServiceAuthError::CONNECTION_FAILED));
  }
}

}  // namespace policy
