// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/browser_bound_key_store_android.h"

#include "components/payments/content/android/browser_binding_jni/BrowserBoundKeyStore_jni.h"
#include "components/payments/content/browser_binding/browser_bound_key_android.h"
#include "third_party/jni_zero/jni_zero.h"

namespace payments {

std::unique_ptr<BrowserBoundKeyStore> GetBrowserBoundKeyStoreInstance() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return std::make_unique<BrowserBoundKeyStoreAndroid>(
      Java_BrowserBoundKeyStore_getInstance(env));
}

BrowserBoundKeyStoreAndroid::BrowserBoundKeyStoreAndroid(
    jni_zero::ScopedJavaLocalRef<jobject> impl)
    : impl_(impl) {}

BrowserBoundKeyStoreAndroid::~BrowserBoundKeyStoreAndroid() = default;

std::unique_ptr<BrowserBoundKey>
BrowserBoundKeyStoreAndroid::GetOrCreateBrowserBoundKeyForCredentialId(
    const std::vector<uint8_t>& credential_id) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return std::make_unique<BrowserBoundKeyAndroid>(
      Java_BrowserBoundKeyStore_getOrCreateBrowserBoundKeyForCredentialId(
          env, impl_, credential_id));
}

}  // namespace payments
