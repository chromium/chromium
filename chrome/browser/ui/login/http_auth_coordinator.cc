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
  // For subresources, create a LoginHandler directly, which may show a login
  // prompt to the user. Main frame resources go through LoginTabHelper, which
  // manages a more complicated flow to avoid confusion about which website is
  // showing the prompt.
  if (is_request_for_primary_main_frame) {
    return CreateLoginDelegateFromTabHelper(web_contents, auth_info, request_id,
                                            url, response_headers,
                                            std::move(auth_required_callback));
  }

  return CreateLoginDelegateFromLoginHandler(web_contents, auth_info,
                                             request_id, url, response_headers,
                                             std::move(auth_required_callback));
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
