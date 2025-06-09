// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_web_contents_manager.h"

#include "base/logging.h"
#include "content/public/browser/navigation_handle.h"

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
  // a user gesture, or a non-reload browser-initiated navigation.
  // TODO(crbug.com/40282980): Reset the bit for user-initiated browser reloads.
  //
  // TODO(crbug.com/40622940): This check has to be done at DidStartNavigation
  // time, the HasUserGesture state is lost by the time the navigation
  // commits.
  const bool is_renderer_initiated = navigation_handle->IsRendererInitiated();
  const content::ReloadType reload_type = navigation_handle->GetReloadType();
  const bool has_user_gesture = navigation_handle->HasUserGesture();
  VLOG(2) << "PaymentRequestWebContentsManager::DidStartNavigation(): "
          << "IsRendererInitiated()=" << is_renderer_initiated
          << ", GetReloadType()=" << GetReloadTypeString(reload_type)
          << ", HasUserGesture()= " << has_user_gesture;
  if ((!is_renderer_initiated && reload_type == content::ReloadType::NONE) ||
      has_user_gesture) {
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
