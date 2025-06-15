// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_ANDROID_DEVICE_DELEGATE_ANDROID_H_
#define COMPONENTS_FACILITATED_PAYMENTS_ANDROID_DEVICE_DELEGATE_ANDROID_H_

#include "content/public/browser/web_contents.h"

namespace payments::facilitated {

// Returns true if Google Wallet is installed, and its version supports Pix
// account linking.
bool IsWalletEligibleForPixAccountLinking();

// Opens the Pix account linking page in Google Wallet.
void OpenPixAccountLinkingPageInWallet(content::WebContents* web_contents);

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_ANDROID_DEVICE_DELEGATE_ANDROID_H_
