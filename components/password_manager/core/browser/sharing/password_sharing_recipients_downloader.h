// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_SHARING_RECIPIENTS_DOWNLOADER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_SHARING_RECIPIENTS_DOWNLOADER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/sync/protocol/password_sharing_recipients.pb.h"
#include "components/version_info/channel.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace signin {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace password_manager {

// Manages making a request to the sync server to download sharing candidates.
// Must be used only once per request and on the UI thread.
class PasswordSharingRecipientsDownloader {
 public:
  PasswordSharingRecipientsDownloader(
      version_info::Channel channel,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);

  PasswordSharingRecipientsDownloader(
      const PasswordSharingRecipientsDownloader&) = delete;
  PasswordSharingRecipientsDownloader& operator=(
      const PasswordSharingRecipientsDownloader&) = delete;
  ~PasswordSharingRecipientsDownloader();

  // Starts a request to the server. Once the request succeeds or fails,
  // `on_complete` will be called. Must be called at most once.
  void Start(base::OnceClosure on_complete);

  // Returns response if request successfully complete, nullopt otherwise. Must
  // be called at most once.
  std::optional<sync_pb::PasswordSharingRecipientsResponse> TakeResponse();

  // Returns request failure. Must be called once the request is complete.
  int GetNetError() const;
  int GetHttpError() const;
  GoogleServiceAuthError GetAuthError() const;

  // Visible for tests.
  static GURL GetPasswordSharingRecipientsURL(version_info::Channel channel);

 private:
  void AccessTokenFetched(GoogleServiceAuthError error,
                          signin::AccessTokenInfo access_token_info);
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);
  void SendRequest(const signin::AccessTokenInfo& access_token_info);
  void StartFetchingAccessToken();

  // Used to determine Sync server URL and user agent.
  const version_info::Channel channel_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // The current URL loader. Null unless a request is in progress.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Callback used to notify when request succeeds or fails.
  base::OnceClosure on_request_complete_callback_;

  // Pending request for an access token. Non-null iff there is a request
  // ongoing.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      ongoing_access_token_fetch_;

  // Contains a parsed response if the request succeeded.
  std::optional<sync_pb::PasswordSharingRecipientsResponse> response_;

  // Contains request failure if the request is complete and there was an HTTP
  // error code or request timed out.
  int net_error_ = 0;
  int http_error_ = 0;

  // Whether there was a retry request for an access token. Used to retry
  // fetching the access token only once.
  bool access_token_retried_ = false;

  // Contains the last error code while requesting an access token.
  GoogleServiceAuthError last_auth_error_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_SHARING_RECIPIENTS_DOWNLOADER_H_
