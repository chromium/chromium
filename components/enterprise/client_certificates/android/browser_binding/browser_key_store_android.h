// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_ANDROID_BROWSER_BINDING_BROWSER_KEY_STORE_ANDROID_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_ANDROID_BROWSER_BINDING_BROWSER_KEY_STORE_ANDROID_H_

#include <vector>

#include "components/enterprise/client_certificates/android/browser_binding/browser_key_store.h"
#include "third_party/jni_zero/jni_zero.h"

namespace client_certificates {

// Implements BrowserKeyStore for Android.
class BrowserKeyStoreAndroid : public BrowserKeyStore {
 public:
  explicit BrowserKeyStoreAndroid(
      jni_zero::ScopedJavaLocalRef<jobject> java_object);

  std::unique_ptr<BrowserKey> GetOrCreateBrowserKeyForCredentialId(
      const std::vector<uint8_t>& credential_id,
      const CredentialInfoList& allowed_credentials) override;

  bool GetDeviceSupportsHardwareKeys() override;

  void DeleteBrowserKey(const std::vector<uint8_t>& credential_id) override;

 protected:
  ~BrowserKeyStoreAndroid() override;

 private:
  // The implementation Java object.
  jni_zero::ScopedJavaGlobalRef<jobject> impl_;
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_ANDROID_BROWSER_BINDING_BROWSER_KEY_STORE_ANDROID_H_
