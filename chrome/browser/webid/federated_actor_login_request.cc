// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_actor_login_request.h"

#include <string>

#include "base/functional/callback.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

FederatedActorLoginRequest::FederatedActorLoginRequest(
    content::WebContents* web_contents,
    const url::Origin& idp_origin,
    const std::string& account_id,
    OnFederatedTokenReceivedCallback callback)
    : content::WebContentsUserData<FederatedActorLoginRequest>(*web_contents),
      idp_origin_(idp_origin),
      account_id_(account_id),
      on_federated_token_received_callback_(std::move(callback)) {}

FederatedActorLoginRequest::~FederatedActorLoginRequest() = default;

// static
void FederatedActorLoginRequest::Set(
    content::WebContents* web_contents,
    const url::Origin& idp_origin,
    const std::string& account_id,
    OnFederatedTokenReceivedCallback callback) {
  web_contents->SetUserData(
      FederatedActorLoginRequest::UserDataKey(),
      std::make_unique<FederatedActorLoginRequest>(
          web_contents, idp_origin, account_id, std::move(callback)));
}

// static
void FederatedActorLoginRequest::Unset(content::WebContents* web_contents) {
  web_contents->RemoveUserData(UserDataKey());
}

// static
FederatedActorLoginRequest* FederatedActorLoginRequest::Get(
    content::WebContents* web_contents) {
  return WebContentsUserData<FederatedActorLoginRequest>::FromWebContents(
      web_contents);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FederatedActorLoginRequest);
