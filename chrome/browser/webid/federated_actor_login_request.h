// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_ACTOR_LOGIN_REQUEST_H_
#define CHROME_BROWSER_WEBID_FEDERATED_ACTOR_LOGIN_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "base/timer/timer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "url/origin.h"

namespace content {

namespace webid {
enum class FederatedLoginResult;
}  // namespace webid

class WebContents;
}  // namespace content

using OnFederatedResultReceivedCallback =
    base::RepeatingCallback<void(content::webid::FederatedLoginResult)>;

// Represents an actor login request. The actor may choose to request
// a federated token from a specific account, and request to be notified when
// the request is completed.
class FederatedActorLoginRequest
    : public content::WebContentsUserData<FederatedActorLoginRequest> {
 public:
  FederatedActorLoginRequest(content::WebContents* web_contents,
                             const url::Origin& idp_origin,
                             const std::string& account_id,
                             OnFederatedResultReceivedCallback callback);
  FederatedActorLoginRequest(const FederatedActorLoginRequest&) = delete;
  FederatedActorLoginRequest& operator=(const FederatedActorLoginRequest&) =
      delete;
  ~FederatedActorLoginRequest() override;

  const url::Origin& idp_origin() const { return idp_origin_; }
  const std::string& account_id() const { return account_id_; }
  void OnFederatedResultReceived(content::webid::FederatedLoginResult result);

  // Sets the actor login request information. This is used to know whether a
  // current pending web identity request is an actor login request, which
  // account to automatically select, and how to notify the actor.
  static void Set(content::WebContents* web_contents,
                  const url::Origin& idp_origin,
                  const std::string& account_id,
                  OnFederatedResultReceivedCallback callback);
  static void Unset(content::WebContents* web_contents);
  static FederatedActorLoginRequest* Get(content::WebContents* web_contents);

  WEB_CONTENTS_USER_DATA_KEY_DECL();

 private:
  void OnTimeout();

  url::Origin idp_origin_;
  std::string account_id_;
  OnFederatedResultReceivedCallback on_federated_result_received_callback_;
  base::OneShotTimer timeout_timer_;
  bool has_run_callback_ = false;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_ACTOR_LOGIN_REQUEST_H_
