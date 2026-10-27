// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_web_contents_manager.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "components/payments/core/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace payments {

namespace {
std::string_view GetReloadTypeString(content::ReloadType reload_type) {
  switch (reload_type) {
    case content::ReloadType::NONE:
      return "NONE";
    case content::ReloadType::NORMAL:
      return "NORMAL";
    case content::ReloadType::BYPASSING_CACHE:
      return "BYPASSING_CACHE";
  }
  NOTREACHED();
}
}  // namespace

PaymentRequestWebContentsManager::PaymentRequestWebContentsManager(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents), WebContentsUserData(*web_contents) {}

PaymentRequestWebContentsManager::~PaymentRequestWebContentsManager() = default;

void PaymentRequestWebContentsManager::RecordActivationlessShow() {
  VLOG(2) << "PaymentRequestWebContentsManager::RecordActivationlessShow()";
  had_activationless_show_ = true;
}

void PaymentRequestWebContentsManager::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  const bool in_primary_main_frame = navigation_handle->IsInPrimaryMainFrame();
  const bool is_same_document = navigation_handle->IsSameDocument();
  VLOG(2) << "PaymentRequestWebContentsManager::DidStartNavigation(): "
          << "IsInPrimaryMainFrame()=" << in_primary_main_frame
          << ", IsSameDocument()=" << is_same_document;
  if (!in_primary_main_frame || is_same_document) {
    return;
  }

  // Reset the activationless show tracker at the next user-initiated
  // navigation, which is defined as either a renderer-initiated navigation with
  // a user gesture, or a browser-initiated navigation other than an error-page
  // reload.
  //
  // Browser-initiated reloads of error pages are excluded because
  // error_page::NetErrorAutoReloader (and other error-page reloaders) trigger
  // automatic browser-initiated reloads when a page fails to load. Because a
  // website can intentionally navigate without user activation to a URL that
  // fails with a network error (e.g., net::ERR_TOO_MANY_REDIRECTS) and then
  // succeeds on the subsequent auto-reload, treating error-page reloads as
  // user-initiated would allow websites to bypass the 'user-initiated' guard
  // and repeatedly trigger activationless show() calls (a security
  // vulnerability; see crbug.com/40072274).
  //
  // TODO(crbug.com/40622940): This check has to be done at DidStartNavigation
  // time, the HasUserGesture state is lost by the time the navigation
  // commits.
  const bool is_renderer_initiated = navigation_handle->IsRendererInitiated();
  const content::ReloadType reload_type = navigation_handle->GetReloadType();
  const bool has_user_gesture = navigation_handle->HasUserGesture();
  const bool is_error_document =
      web_contents()->GetPrimaryMainFrame()->IsErrorDocument();
  VLOG(2) << "PaymentRequestWebContentsManager::DidStartNavigation(): "
          << "IsRendererInitiated()=" << is_renderer_initiated
          << ", GetReloadType()=" << GetReloadTypeString(reload_type)
          << ", HasUserGesture()=" << has_user_gesture
          << ", IsErrorDocument()=" << is_error_document;
  const bool is_reload = reload_type != content::ReloadType::NONE;
  const bool can_reset_on_reload =
      !is_error_document &&
      base::FeatureList::IsEnabled(
          features::kPaymentRequestResetActivationlessShowOnReload);
  const bool is_allowable_browser_navigation =
      !is_renderer_initiated && (!is_reload || can_reset_on_reload);
  if (has_user_gesture || is_allowable_browser_navigation) {
    VLOG(2) << "PaymentRequestWebContentsManager::DidStartNavigation(): "
            << "Resetting had_activationless_show_";
    had_activationless_show_ = false;
  } else {
    VLOG(2) << "PaymentRequestWebContentsManager::DidStartNavigation(): "
            << "Ignoring navigation, NOT resetting had_activationless_show_";
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PaymentRequestWebContentsManager);

}  // namespace payments
