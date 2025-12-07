// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_STORE_ANDROID_H_
#define COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_STORE_ANDROID_H_

#include <vector>

#include "components/payments/content/browser_binding/browser_bound_key_store.h"
#include "third_party/jni_zero/jni_zero.h"

namespace payments {

// Implements BrowserBoundKeyStore for Android.
class BrowserBoundKeyStoreAndroid : public BrowserBoundKeyStore {
 public:
  explicit BrowserBoundKeyStoreAndroid(
      jni_zero::ScopedJavaLocalRef<jobject> java_object);

  std::unique_ptr<BrowserBoundKey> GetOrCreateBrowserBoundKeyForCredentialId(
      const std::vector<uint8_t>& credential_id,
      const std::vector<device::PublicKeyCredentialParams::CredentialInfo>&
          allowed_credentials) override;

  void DeleteBrowserBoundKey(std::vector<uint8_t> bbk_id) override;

  bool GetDeviceSupportsHardwareKeys() override;

 protected:
  ~BrowserBoundKeyStoreAndroid() override;

 private:
  // The implementation Java object.
  jni_zero::ScopedJavaGlobalRef<jobject> impl_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_BROWSER_BINDING_BROWSER_BOUND_KEY_STORE_ANDROID_H_
