// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/http_auth_coordinator.h"

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/login/login_tab_helper.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "content/public/browser/browser_context.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/api/web_request/web_request_api.h"  // nogncheck
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#endif

HttpAuthCoordinator::HttpAuthCoordinator() = default;
HttpAuthCoordinator::~HttpAuthCoordinator() = default;

std::unique_ptr<content::LoginDelegate>
HttpAuthCoordinator::CreateLoginDelegate(
    content::WebContents* web_contents,
    content::BrowserContext* browser_context,
    const net::AuthChallengeInfo& auth_info,
    const content::GlobalRequestID& request_id,
    bool is_request_for_primary_main_frame,
    bool is_request_for_navigation,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    content::LoginDelegate::LoginAuthRequiredCallback auth_required_callback) {
  auto flow_owned = std::make_unique<Flow>(
      this, web_contents, auth_info, request_id,
      is_request_for_primary_main_frame, is_request_for_navigation, url,
      response_headers, std::move(auth_required_callback));
  Flow* flow = flow_owned.get();
  flows_[flow] = std::move(flow_owned);

  if (!flow->ForwardToExtension(browser_context)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&Flow::ShowDialog, flow->GetWeakPtr()));
  }

  return std::make_unique<LoginDelegateWrapper>(flow);
}

void HttpAuthCoordinator::CreateLoginTabHelper(
    content::WebContents* web_contents) {
  LoginTabHelper::CreateForWebContents(web_contents);
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
  login_handler->ShowLoginPrompt(url);
  return login_handler;
}

HttpAuthCoordinator::Flow::Flow(
    HttpAuthCoordinator* coordinator,
    content::WebContents* web_contents,
    const net::AuthChallengeInfo& auth_info,
    const content::GlobalRequestID& request_id,
    bool is_request_for_primary_main_frame,
    bool is_request_for_navigation,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    content::LoginDelegate::LoginAuthRequiredCallback auth_required_callback)
    : coordinator_(coordinator),
      auth_info_(auth_info),
      request_id_(request_id),
      is_request_for_primary_main_frame_(is_request_for_primary_main_frame),
      is_request_for_navigation_(is_request_for_navigation),
      url_(url),
      response_headers_(response_headers),
      callback_(std::move(auth_required_callback)) {
  if (web_contents) {
    web_contents_ = web_contents->GetWeakPtr();
  }
}

HttpAuthCoordinator::Flow::~Flow() = default;

void HttpAuthCoordinator::Flow::WrapperDestroyed() {
  callback_.Reset();
  login_handler_.reset();

  // For now, we tie the lifetime of Flow to that of the wrapper.
  coordinator_->FlowFinished(this);
}

bool HttpAuthCoordinator::Flow::ForwardToExtension(
    content::BrowserContext* browser_context) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If the WebRequest API wants to take a shot at intercepting this, we can
  // return immediately. |continuation| will eventually be invoked if the
  // request isn't cancelled.
  auto* api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          browser_context);
  auto continuation = base::BindOnce(&Flow::OnExtensionResponse, GetWeakPtr());
  if (api->MaybeProxyAuthRequest(
          browser_context, auth_info_, response_headers_, request_id_,
          is_request_for_navigation_, std::move(continuation),
          extensions::WebViewGuest::FromWebContents(
              web_contents_ ? web_contents_.get() : nullptr))) {
    return true;
  }
#endif
  return false;
}

void HttpAuthCoordinator::Flow::ShowDialog() {
  // If we're being asked to show a dialog, then the callback must still be
  // valid.
  CHECK(callback_);

  // If the WebContents is no longer valid, then we cannot show a dialog.
  if (!web_contents_) {
    std::move(callback_).Run(std::nullopt);
    return;
  }

  // If the WebContents was for a prerender use case, then cancel authentication
  // and destroy the prerender.
  prerender::NoStatePrefetchContents* no_state_prefetch_contents =
      prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          web_contents_.get());
  if (no_state_prefetch_contents) {
    no_state_prefetch_contents->Destroy(prerender::FINAL_STATUS_AUTH_NEEDED);
    std::move(callback_).Run(std::nullopt);
    return;
  }

  if (is_request_for_primary_main_frame_) {
    // For main frame resources, create a login tab helper. The login tab helper
    // will take care of flows (3b) and (5b), see class comment.
    coordinator_->CreateLoginTabHelper(web_contents_.get());
    if (did_cancel_from_extension_) {
      LoginTabHelper::FromWebContents(web_contents_.get())
          ->RegisterExtensionCancelledNavigation(request_id_);
    }

    // Cancel the current auth request. This will result in synchronous
    // destruction of `this`.
    std::move(callback_).Run(std::nullopt);
    return;
  }

  // If there is no WebContentsModalDialogManager, then a dialog cannot be
  // shown.
#if !BUILDFLAG(IS_ANDROID)
  web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(
          web_contents_.get());
  if (!manager) {
    std::move(callback_).Run(std::nullopt);
    return;
  }
#endif

  // For subresources, create a LoginHandler which will show a login prompt.
  auto wrapped_callback = base::BindOnce(&Flow::OnCredentials, GetWeakPtr());
  login_handler_ = coordinator_->CreateLoginDelegateFromLoginHandler(
      web_contents_.get(), auth_info_, request_id_, url_, response_headers_,
      std::move(wrapped_callback));
}

base::WeakPtr<HttpAuthCoordinator::Flow>
HttpAuthCoordinator::Flow::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void HttpAuthCoordinator::Flow::OnExtensionResponse(
    const std::optional<net::AuthCredentials>& credentials,
    bool should_cancel) {
  if (credentials) {
    std::move(callback_).Run(credentials);
    return;
  }
  if (should_cancel) {
    if (is_request_for_primary_main_frame_) {
      did_cancel_from_extension_ = true;
    }
    std::move(callback_).Run(std::nullopt);
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&Flow::ShowDialog, GetWeakPtr()));
}

void HttpAuthCoordinator::Flow::OnCredentials(
    const std::optional<net::AuthCredentials>& credentials) {
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
