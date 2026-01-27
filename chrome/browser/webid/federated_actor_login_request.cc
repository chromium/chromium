// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_actor_login_request.h"

#include <string>

#include "base/functional/callback.h"
#include "content/public/browser/page.h"
#include "url/origin.h"

FederatedActorLoginRequest::FederatedActorLoginRequest(
    content::Page& page,
    const url::Origin& idp_origin,
    const std::string& account_id,
    OnFederatedTokenReceivedCallback callback)
    : content::PageUserData<FederatedActorLoginRequest>(page),
      idp_origin_(idp_origin),
      account_id_(account_id),
      on_federated_token_received_callback_(std::move(callback)) {}

FederatedActorLoginRequest::~FederatedActorLoginRequest() = default;

// static
void FederatedActorLoginRequest::Set(
    content::Page& page,
    const url::Origin& idp_origin,
    const std::string& account_id,
    OnFederatedTokenReceivedCallback callback) {
  page.SetUserData(FederatedActorLoginRequest::UserDataKey(),
                   std::make_unique<FederatedActorLoginRequest>(
                       page, idp_origin, account_id, std::move(callback)));
}

// static
void FederatedActorLoginRequest::Unset(content::Page& page) {
  FederatedActorLoginRequest::DeleteForPage(page);
}

// static
FederatedActorLoginRequest* FederatedActorLoginRequest::Get(
    content::Page& page) {
  return PageUserData<FederatedActorLoginRequest>::GetForPage(page);
}

PAGE_USER_DATA_KEY_IMPL(FederatedActorLoginRequest);
