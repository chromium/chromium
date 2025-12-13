// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_ANDROID_BROWSER_BINDING_BROWSER_KEY_ANDROID_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_ANDROID_BROWSER_BINDING_BROWSER_KEY_ANDROID_H_

#include <memory>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "components/enterprise/client_certificates/android/browser_binding/browser_key.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"
#include "third_party/jni_zero/default_conversions.h"
#include "third_party/jni_zero/jni_zero.h"

namespace client_certificates {

// Implements BrowserKey for Android.
class BrowserKeyAndroid : public BrowserKey {
 public:
  // `impl` must not be a null Java reference.
  explicit BrowserKeyAndroid(const jni_zero::JavaRef<jobject>& impl);
  ~BrowserKeyAndroid() override;
  std::vector<uint8_t> GetIdentifier() const override;
  jni_zero::ScopedJavaLocalRef<jobject> GetPrivateKey() const override;
  std::vector<uint8_t> GetPublicKeyAsSPKI() const override;
  scoped_refptr<net::SSLPrivateKey> GetSSLPrivateKey() const override;
  std::optional<std::vector<uint8_t>> Sign(
      const std::vector<uint8_t>& data) const override;
  SecurityLevel GetSecurityLevel() const override;

 private:
  jni_zero::ScopedJavaGlobalRef<jobject> impl_;
};

}  // namespace client_certificates

namespace jni_zero {

template <>
inline std::unique_ptr<client_certificates::BrowserKeyAndroid> FromJniType(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj) {
  if (!obj) {
    return nullptr;
  }
  return std::make_unique<client_certificates::BrowserKeyAndroid>(obj);
}

}  // namespace jni_zero

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_ANDROID_BROWSER_BINDING_BROWSER_KEY_ANDROID_H_
