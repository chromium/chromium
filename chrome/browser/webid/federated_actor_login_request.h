// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_ACTOR_LOGIN_REQUEST_H_
#define CHROME_BROWSER_WEBID_FEDERATED_ACTOR_LOGIN_REQUEST_H_

#include <string>

#include "base/functional/callback.h"
#include "content/public/browser/page_user_data.h"
#include "url/origin.h"

namespace content {
class Page;
}

using OnFederatedTokenReceivedCallback = base::OnceCallback<void(bool)>;

// Represents an actor login request. The actor may choose to request
// a federated token from a specific account, and request to be notified when
// the request is completed.
class FederatedActorLoginRequest
    : public content::PageUserData<FederatedActorLoginRequest> {
 public:
  FederatedActorLoginRequest(content::Page& page,
                             const url::Origin& idp_origin,
                             const std::string& account_id,
                             OnFederatedTokenReceivedCallback callback);
  FederatedActorLoginRequest(const FederatedActorLoginRequest&) = delete;
  FederatedActorLoginRequest& operator=(const FederatedActorLoginRequest&) =
      delete;
  ~FederatedActorLoginRequest() override;

  const url::Origin& idp_origin() const { return idp_origin_; }
  const std::string& account_id() const { return account_id_; }
  OnFederatedTokenReceivedCallback on_federated_token_received_callback() {
    return std::move(on_federated_token_received_callback_);
  }

  // Sets the actor login request information. This is used to know whether a
  // current pending web identity request is an actor login request, which
  // account to automatically select, and how to notify the actor.
  static void Set(content::Page& page,
                  const url::Origin& idp_origin,
                  const std::string& account_id,
                  OnFederatedTokenReceivedCallback callback);
  static void Unset(content::Page& page);
  static FederatedActorLoginRequest* Get(content::Page& page);

  PAGE_USER_DATA_KEY_DECL();

 private:
  url::Origin idp_origin_;
  std::string account_id_;
  OnFederatedTokenReceivedCallback on_federated_token_received_callback_;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_ACTOR_LOGIN_REQUEST_H_
