// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/android/device_delegate_android.h"

#include "base/android/jni_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/facilitated_payments/android/java/jni_headers/DeviceDelegate_jni.h"

namespace payments::facilitated {

bool IsWalletEligibleForPixAccountLinking() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_DeviceDelegate_isWalletEligibleForPixAccountLinking(env);
}

void OpenPixAccountLinkingPageInWallet() {
  // TODO(crbug.com/419728706): Open the account linking page in Wallet.
}

}  // namespace payments::facilitated
