// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_scheduler.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace {

// Holds a weak pointer to the active request in a WebContents, if any.
class ActiveRequestWeakHolder
    : public content::WebContentsUserData<ActiveRequestWeakHolder> {
 public:
  ActiveRequestWeakHolder(const ActiveRequestWeakHolder&) = delete;
  ActiveRequestWeakHolder& operator=(const ActiveRequestWeakHolder&) = delete;

  ~ActiveRequestWeakHolder() override = default;

  base::WeakPtr<ChromeAuthenticatorRequestDelegate>& request() {
    return request_;
  }

 private:
  explicit ActiveRequestWeakHolder(content::WebContents* web_contents)
      : WebContentsUserData(*web_contents) {}

  friend class content::WebContentsUserData<ActiveRequestWeakHolder>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtr<ChromeAuthenticatorRequestDelegate> request_;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(ActiveRequestWeakHolder);

}  // namespace

// static
std::unique_ptr<ChromeAuthenticatorRequestDelegate>
AuthenticatorRequestScheduler::CreateRequestDelegate(
    content::RenderFrameHost* render_frame_host) {
  // RenderFrameHosts which are not exposed to the user can't create
  // authenticator request delegate.
  if (!render_frame_host->IsActive())
    return nullptr;

  auto* const web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  auto* const active_request_holder =
      ActiveRequestWeakHolder::GetOrCreateForWebContents(web_contents);

  if (active_request_holder->request())
    return nullptr;

  auto request =
      std::make_unique<ChromeAuthenticatorRequestDelegate>(render_frame_host);
  active_request_holder->request() = request->AsWeakPtr();
  return request;
}

// static
ChromeAuthenticatorRequestDelegate*
AuthenticatorRequestScheduler::GetRequestDelegate(
    content::WebContents* web_contents) {
  return ActiveRequestWeakHolder::GetOrCreateForWebContents(web_contents)
      ->request()
      .get();
}
