// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_scheduler.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

// static
ChromeAuthenticatorRequestDelegate*
AuthenticatorRequestScheduler::CreateRequestDelegate(
    content::RenderFrameHost* render_frame_host) {
  // RenderFrameHosts which are not exposed to the user can't create
  // authenticator request delegate.
  if (!render_frame_host->IsActive())
    return nullptr;

  auto* const web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents) {
    return nullptr;
  }
  if (ChromeAuthenticatorRequestDelegate::FromWebContents(web_contents)) {
    return nullptr;
  }

  ChromeAuthenticatorRequestDelegate::CreateForWebContents(web_contents,
                                                           render_frame_host);
  return ChromeAuthenticatorRequestDelegate::FromWebContents(web_contents);
}

// static
ChromeAuthenticatorRequestDelegate*
AuthenticatorRequestScheduler::GetRequestDelegate(
    content::WebContents* web_contents) {
  return ChromeAuthenticatorRequestDelegate::FromWebContents(web_contents);
}
