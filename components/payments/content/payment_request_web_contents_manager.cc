// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_web_contents_manager.h"

#include "content/public/browser/navigation_handle.h"

namespace payments {

// static
PaymentRequestWebContentsManager*
PaymentRequestWebContentsManager::GetOrCreateForWebContents(
    content::WebContents& web_contents) {
  // CreateForWebContents does nothing if the manager instance already exists.
  PaymentRequestWebContentsManager::CreateForWebContents(&web_contents);
  return PaymentRequestWebContentsManager::FromWebContents(&web_contents);
}

PaymentRequestWebContentsManager::PaymentRequestWebContentsManager(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents), WebContentsUserData(*web_contents) {}

PaymentRequestWebContentsManager::~PaymentRequestWebContentsManager() = default;

void PaymentRequestWebContentsManager::RecordActivationlessShow() {
  had_activationless_show_ = true;
}

void PaymentRequestWebContentsManager::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
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
  if ((!navigation_handle->IsRendererInitiated() &&
       navigation_handle->GetReloadType() == content::ReloadType::NONE) ||
      navigation_handle->HasUserGesture()) {
    had_activationless_show_ = false;
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PaymentRequestWebContentsManager);

}  // namespace payments
