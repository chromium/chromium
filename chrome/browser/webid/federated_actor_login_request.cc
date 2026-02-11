// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_actor_login_request.h"

#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace {
constexpr base::TimeDelta kRequestTimeout = base::Seconds(20);
}  // namespace

FederatedActorLoginRequest::FederatedActorLoginRequest(
    content::WebContents* web_contents,
    const url::Origin& idp_origin,
    const std::string& account_id,
    OnFederatedResultReceivedCallback callback)
    : content::WebContentsUserData<FederatedActorLoginRequest>(*web_contents),
      idp_origin_(idp_origin),
      account_id_(account_id),
      on_federated_result_received_callback_(std::move(callback)) {
  timeout_timer_.Start(FROM_HERE, kRequestTimeout,
                       base::BindOnce(&FederatedActorLoginRequest::OnTimeout,
                                      base::Unretained(this)));
}

FederatedActorLoginRequest::~FederatedActorLoginRequest() = default;

void FederatedActorLoginRequest::OnFederatedResultReceived(
    content::webid::FederatedLoginResult result) {
  has_run_callback_ = true;
  on_federated_result_received_callback_.Run(result);
}

void FederatedActorLoginRequest::OnTimeout() {
  if (has_run_callback_) {
    return;
  }
  on_federated_result_received_callback_.Run(
      content::webid::FederatedLoginResult::kTimeout);
  Unset(&GetWebContents());
}

// static
void FederatedActorLoginRequest::Set(
    content::WebContents* web_contents,
    const url::Origin& idp_origin,
    const std::string& account_id,
    OnFederatedResultReceivedCallback callback) {
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
