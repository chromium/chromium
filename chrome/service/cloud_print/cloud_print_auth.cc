// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_auth.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "chrome/common/cloud_print/cloud_print_helpers.h"
#include "chrome/service/cloud_print/cloud_print_token_store.h"
#include "chrome/service/net/service_url_request_context_getter.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace cloud_print {

namespace {

enum CloudPrintAuthEvent {
  AUTH_EVENT_ROBO_CREATE,
  AUTH_EVENT_ROBO_SUCCEEDED,
  AUTH_EVENT_ROBO_FAILED,
  AUTH_EVENT_ROBO_JSON_ERROR,
  AUTH_EVENT_ROBO_AUTH_ERROR,
  AUTH_EVENT_AUTH_WITH_TOKEN,
  AUTH_EVENT_AUTH_WITH_CODE,
  AUTH_EVENT_TOKEN_RESPONSE,
  AUTH_EVENT_REFRESH_REQUEST,
  AUTH_EVENT_REFRESH_RESPONSE,
  AUTH_EVENT_AUTH_ERROR,
  AUTH_EVENT_NET_ERROR,
  AUTH_EVENT_MAX
};

}  // namespace

CloudPrintAuth::CloudPrintAuth(
    Client* client,
    const GURL& cloud_print_server_url,
    const gaia::OAuthClientInfo& oauth_client_info,
    const std::string& proxy_id,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
    : client_(client),
      oauth_client_info_(oauth_client_info),
      cloud_print_server_url_(cloud_print_server_url),
      proxy_id_(proxy_id),
      partial_traffic_annotation_(partial_traffic_annotation) {
  DCHECK(client);
}

void CloudPrintAuth::AuthenticateWithToken(
    const std::string& cloud_print_token) {
  VLOG(1) << "CP_AUTH: Authenticating with token";

  UMA_HISTOGRAM_ENUMERATION("CloudPrint.AuthEvent", AUTH_EVENT_ROBO_CREATE,
                            AUTH_EVENT_MAX);

  client_login_token_ = cloud_print_token;

  // We need to get the credentials of the robot here.
  GURL get_authcode_url = GetUrlForGetAuthCode(cloud_print_server_url_,
                                               oauth_client_info_.client_id,
                                               proxy_id_);
  request_ = CloudPrintURLFetcher::Create(partial_traffic_annotation_);
  request_->StartGetRequest(CloudPrintURLFetcher::REQUEST_AUTH_CODE,
                            get_authcode_url, this,
                            kCloudPrintAuthMaxRetryCount);
}

void CloudPrintAuth::AuthenticateWithRobotToken(
    const std::string& robot_oauth_refresh_token,
    const std::string& robot_email) {
  VLOG(1) << "CP_AUTH: Authenticating with robot token";

  UMA_HISTOGRAM_ENUMERATION("CloudPrint.AuthEvent", AUTH_EVENT_AUTH_WITH_TOKEN,
                            AUTH_EVENT_MAX);

  robot_email_ = robot_email;
  refresh_token_ = robot_oauth_refresh_token;
  RefreshAccessToken();
}

void CloudPrintAuth::AuthenticateWithRobotAuthCode(
    const std::string& robot_oauth_auth_code,
    const std::string& robot_email) {
  VLOG(1) << "CP_AUTH: Authenticating with robot auth code";

  UMA_HISTOGRAM_ENUMERATION("CloudPrint.AuthEvent", AUTH_EVENT_AUTH_WITH_CODE,
                            AUTH_EVENT_MAX);

  robot_email_ = robot_email;
  // Now that we have an auth code we need to get the refresh and access tokens.
  oauth_client_ =
      std::make_unique<gaia::GaiaOAuthClient>(client_->GetURLLoaderFactory());
  oauth_client_->GetTokensFromAuthCode(oauth_client_info_,
                                       robot_oauth_auth_code,
                                       kCloudPrintAuthMaxRetryCount,
                                       this);
}

void CloudPrintAuth::RefreshAccessToken() {
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.AuthEvent", AUTH_EVENT_REFRESH_REQUEST,
                            AUTH_EVENT_MAX);
  oauth_client_ =
      std::make_unique<gaia::GaiaOAuthClient>(client_->GetURLLoaderFactory());
  std::vector<std::string> empty_scope_list;  // (Use scope from refresh token.)
  oauth_client_->RefreshToken(oauth_client_info_,
                              refresh_token_,
                              empty_scope_list,
                              kCloudPrintAuthMaxRetryCount,
                              this);
}

void CloudPrintAuth::OnGetTokensResponse(const std::string& refresh_token,
                                         const std::string& access_token,
                                         int expires_in_seconds) {
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.AuthEvent", AUTH_EVENT_TOKEN_RESPONSE,
                            AUTH_EVENT_MAX);
  refresh_token_ = refresh_token;
  // After saving the refresh token, this is just like having just refreshed
  // the access token. Just call OnRefreshTokenResponse.
  OnRefreshTokenResponse(access_token, expires_in_seconds);
}

void CloudPrintAuth::OnRefreshTokenResponse(const std::string& access_token,
                                            int expires_in_seconds) {
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.AuthEvent", AUTH_EVENT_REFRESH_RESPONSE,
                            AUTH_EVENT_MAX);
  client_->OnAuthenticationComplete(access_token, refresh_token_,
                                    robot_email_, user_email_);

  // Schedule a task to refresh the access token again when it is about to
  // expire.
  DCHECK(expires_in_seconds > kTokenRefreshGracePeriodSecs);
  base::TimeDelta refresh_delay = base::TimeDelta::FromSeconds(
      expires_in_seconds - kTokenRefreshGracePeriodSecs);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&CloudPrintAuth::RefreshAccessToken, this),
      refresh_delay);
}

void CloudPrintAuth::OnOAuthError() {
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.AuthEvent", AUTH_EVENT_AUTH_ERROR,
                            AUTH_EVENT_MAX);
  // Notify client about authentication error.
  client_->OnInvalidCredentials();
}

void CloudPrintAuth::OnNetworkError(int response_code) {
  UMA_HISTOGRAM_ENUMERATION("CloudPrint.AuthEvent", AUTH_EVENT_NET_ERROR,
                            AUTH_EVENT_MAX);
  // Since we specify infinite retries on network errors, this should never
  // be called.
  NOTREACHED() <<
      "OnNetworkError invoked when not expected, response code is " <<
      response_code;
}

CloudPrintURLFetcher::ResponseAction CloudPrintAuth::HandleJSONData(
    const net::URLFetcher* source,
    const GURL& url,
    const base::Value& json_data,
    bool succeeded) {
  DCHECK(json_data.is_dict());

  if (!succeeded) {
    VLOG(1) << "CP_AUTH: Creating robot account failed";
    UMA_HISTOGRAM_ENUMERATION("CloudPrint.AuthEvent",
                              AUTH_EVENT_ROBO_FAILED,
                              AUTH_EVENT_MAX);
    client_->OnInvalidCredentials();
    return CloudPrintURLFetcher::STOP_PROCESSING;
  }

  const std::string* auth_code = json_data.FindStringKey(kOAuthCodeValue);
  if (!auth_code) {
    VLOG(1) << "CP_AUTH: Creating robot account returned invalid json response";
    UMA_HISTOGRAM_ENUMERATION("CloudPrint.AuthEvent",
                              AUTH_EVENT_ROBO_JSON_ERROR,
                              AUTH_EVENT_MAX);
    client_->OnInvalidCredentials();
    return CloudPrintURLFetcher::STOP_PROCESSING;
  }

  UMA_HISTOGRAM_ENUMERATION("CloudPrint.AuthEvent",
                              AUTH_EVENT_ROBO_SUCCEEDED,
                              AUTH_EVENT_MAX);

  const std::string* robot_email = json_data.FindStringKey(kXMPPJidValue);
  if (robot_email)
    robot_email_ = *robot_email;
  // Now that we have an auth code we need to get the refresh and access tokens.
  oauth_client_ =
      std::make_unique<gaia::GaiaOAuthClient>(client_->GetURLLoaderFactory());
  oauth_client_->GetTokensFromAuthCode(oauth_client_info_, *auth_code,
                                       kCloudPrintAPIMaxRetryCount, this);

  return CloudPrintURLFetcher::STOP_PROCESSING;
}

CloudPrintURLFetcher::ResponseAction CloudPrintAuth::OnRequestAuthError() {
  VLOG(1) << "CP_AUTH: Creating robot account authentication error";

  UMA_HISTOGRAM_ENUMERATION("CloudPrint.AuthEvent",
                            AUTH_EVENT_ROBO_AUTH_ERROR,
                            AUTH_EVENT_MAX);

  // Notify client about authentication error.
  client_->OnInvalidCredentials();
  return CloudPrintURLFetcher::STOP_PROCESSING;
}

std::string CloudPrintAuth::GetAuthHeaderValue() {
  DCHECK(!client_login_token_.empty());
  std::string header;
  header = "GoogleLogin auth=";
  header += client_login_token_;
  return header;
}

CloudPrintAuth::~CloudPrintAuth() {}

}  // namespace cloud_print
