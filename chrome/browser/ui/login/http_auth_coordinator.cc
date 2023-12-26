// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/http_auth_coordinator.h"

#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/login/login_tab_helper.h"

HttpAuthCoordinator::HttpAuthCoordinator() = default;
HttpAuthCoordinator::~HttpAuthCoordinator() = default;

std::unique_ptr<content::LoginDelegate>
HttpAuthCoordinator::CreateLoginDelegate(
    content::WebContents* web_contents,
    const net::AuthChallengeInfo& auth_info,
    const content::GlobalRequestID& request_id,
    bool is_request_for_primary_main_frame,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    content::LoginDelegate::LoginAuthRequiredCallback auth_required_callback) {
  auto flow_owned = std::make_unique<Flow>(this);
  Flow* flow = flow_owned.get();
  flows_[flow] = std::move(flow_owned);

  content::LoginDelegate::LoginAuthRequiredCallback wrapped_callback =
      flow->GetLoginHandlerCallback(std::move(auth_required_callback));

  // For subresources, create a LoginHandler directly, which may show a login
  // prompt to the user. Main frame resources go through LoginTabHelper, which
  // manages a more complicated flow to avoid confusion about which website is
  // showing the prompt.
  if (is_request_for_primary_main_frame) {
    flow->SetLoginHandler(CreateLoginDelegateFromTabHelper(
        web_contents, auth_info, request_id, url, response_headers,
        std::move(wrapped_callback)));
  } else {
    flow->SetLoginHandler(CreateLoginDelegateFromLoginHandler(
        web_contents, auth_info, request_id, url, response_headers,
        std::move(wrapped_callback)));
  }

  return std::make_unique<LoginDelegateWrapper>(flow);
}

std::unique_ptr<content::LoginDelegate>
HttpAuthCoordinator::CreateLoginDelegateFromTabHelper(
    content::WebContents* web_contents,
    const net::AuthChallengeInfo& auth_info,
    const content::GlobalRequestID& request_id,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    content::LoginDelegate::LoginAuthRequiredCallback auth_required_callback) {
  LoginTabHelper::CreateForWebContents(web_contents);
  return LoginTabHelper::FromWebContents(web_contents)
      ->CreateAndStartMainFrameLoginDelegate(auth_info, web_contents,
                                             request_id, url, response_headers,
                                             std::move(auth_required_callback));
}

std::unique_ptr<content::LoginDelegate>
HttpAuthCoordinator::CreateLoginDelegateFromLoginHandler(
    content::WebContents* web_contents,
    const net::AuthChallengeInfo& auth_info,
    const content::GlobalRequestID& request_id,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    content::LoginDelegate::LoginAuthRequiredCallback auth_required_callback) {
  std::unique_ptr<LoginHandler> login_handler = LoginHandler::Create(
      auth_info, web_contents, std::move(auth_required_callback));
  login_handler->StartSubresource(request_id, url, response_headers);
  return login_handler;
}

HttpAuthCoordinator::Flow::Flow(HttpAuthCoordinator* coordinator)
    : coordinator_(coordinator) {}
HttpAuthCoordinator::Flow::~Flow() = default;

content::LoginDelegate::LoginAuthRequiredCallback
HttpAuthCoordinator::Flow::GetLoginHandlerCallback(
    content::LoginDelegate::LoginAuthRequiredCallback callback) {
  callback_ = std::move(callback);
  return base::BindOnce(&Flow::OnCredentials, weak_factory_.GetWeakPtr());
}

void HttpAuthCoordinator::Flow::WrapperDestroyed() {
  callback_.Reset();
  login_handler_.reset();

  // For now, we tie the lifetime of Flow to that of the wrapper.
  coordinator_->FlowFinished(this);
}

void HttpAuthCoordinator::Flow::SetLoginHandler(
    std::unique_ptr<content::LoginDelegate> handler) {
  login_handler_ = std::move(handler);
}

void HttpAuthCoordinator::Flow::OnCredentials(
    const absl::optional<net::AuthCredentials>& credentials) {
  std::move(callback_).Run(credentials);
}

HttpAuthCoordinator::LoginDelegateWrapper::LoginDelegateWrapper(Flow* flow)
    : flow_(flow) {}

HttpAuthCoordinator::LoginDelegateWrapper::~LoginDelegateWrapper() {
  Flow* flow = flow_;
  flow_ = nullptr;
  flow->WrapperDestroyed();
}

void HttpAuthCoordinator::FlowFinished(Flow* flow) {
  flows_.erase(flow);
}
