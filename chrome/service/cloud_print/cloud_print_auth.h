// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_AUTH_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_AUTH_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/service/cloud_print/cloud_print_url_fetcher.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace cloud_print {

// CloudPrintAuth is a class to handle login, token refresh, and other
// authentication tasks for Cloud Print.
// CloudPrintAuth will create new robot account for this proxy if needed.
// CloudPrintAuth will obtain new OAuth token.
// CloudPrintAuth will schedule periodic OAuth token refresh
// It is running in the same thread as CloudPrintProxyBackend::Core.
class CloudPrintAuth : public base::RefCountedThreadSafe<CloudPrintAuth>,
                       public CloudPrintURLFetcher::Delegate,
                       public gaia::GaiaOAuthClient::Delegate {
 public:
  class Client {
   public:
    virtual void OnAuthenticationComplete(
        const std::string& access_token,
        const std::string& robot_oauth_refresh_token,
        const std::string& robot_email,
        const std::string& user_email) = 0;
    virtual void OnInvalidCredentials() = 0;
    virtual scoped_refptr<network::SharedURLLoaderFactory>
    GetURLLoaderFactory() = 0;

   protected:
     virtual ~Client() {}
  };

  CloudPrintAuth(Client* client,
                 const GURL& cloud_print_server_url,
                 const gaia::OAuthClientInfo& oauth_client_info,
                 const std::string& proxy_id,
                 const net::PartialNetworkTrafficAnnotationTag&
                     partial_traffic_annotation);

  // Note:
  //
  // The Authenticate* methods are the various entry points from
  // CloudPrintProxyBackend::Core. It calls us on a dedicated thread to
  // actually perform synchronous (and potentially blocking) operations.
  void AuthenticateWithToken(const std::string& cloud_print_token);
  void AuthenticateWithRobotToken(const std::string& robot_oauth_refresh_token,
                                  const std::string& robot_email);
  void AuthenticateWithRobotAuthCode(const std::string& robot_oauth_auth_code,
                                     const std::string& robot_email);

  void RefreshAccessToken();

  // gaia::GaiaOAuthClient::Delegate implementation.
  void OnGetTokensResponse(const std::string& refresh_token,
                           const std::string& access_token,
                           int expires_in_seconds) override;
  void OnRefreshTokenResponse(const std::string& access_token,
                              int expires_in_seconds) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

  // CloudPrintURLFetcher::Delegate implementation.
  CloudPrintURLFetcher::ResponseAction HandleJSONData(
      const net::URLFetcher* source,
      const GURL& url,
      const base::Value& json_data,
      bool succeeded) override;
  CloudPrintURLFetcher::ResponseAction OnRequestAuthError() override;
  std::string GetAuthHeader() override;

 private:
  friend class base::RefCountedThreadSafe<CloudPrintAuth>;
  ~CloudPrintAuth() override;

  Client* client_;
  gaia::OAuthClientInfo oauth_client_info_;
  std::unique_ptr<gaia::GaiaOAuthClient> oauth_client_;

  // The CloudPrintURLFetcher instance for the current request.
  scoped_refptr<CloudPrintURLFetcher> request_;

  GURL cloud_print_server_url_;
  // Proxy id, need to send to the cloud print server to find and update
  // necessary printers during the migration process.
  const std::string& proxy_id_;
  // The OAuth2 refresh token for the robot.
  std::string refresh_token_;
  // The email address of the user. This is only used during initial
  // authentication with an LSID. This is only used for storing in prefs for
  // display purposes.
  std::string user_email_;
  // The email address of the robot account.
  std::string robot_email_;
  // client login token used to authenticate request to cloud print server to
  // get the robot account.
  std::string client_login_token_;
  // Partial network traffic annotation for network requests.
  const net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintAuth);
};

}  // namespace cloud_print

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_AUTH_H_

