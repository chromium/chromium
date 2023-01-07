// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_web_contents_manager.h"

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
    : WebContentsUserData(*web_contents) {}

PaymentRequestWebContentsManager::~PaymentRequestWebContentsManager() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(PaymentRequestWebContentsManager);

}  // namespace payments
