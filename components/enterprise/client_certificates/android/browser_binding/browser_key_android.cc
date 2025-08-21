// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/android/browser_binding/browser_key_android.h"

#include <strings.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "net/android/keystore.h"
#include "net/base/net_errors.h"
#include "net/ssl/ssl_platform_key_android.h"
#include "net/ssl/ssl_platform_key_util.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/threaded_ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that sepcialize ToJniType()/FromJniType()
#include "components/enterprise/client_certificates/android/browser_binding_jni/BrowserKey_jni.h"

namespace client_certificates {

BrowserKeyAndroid::BrowserKeyAndroid(const jni_zero::JavaRef<jobject>& impl)
    : impl_(impl) {
  CHECK(impl_);
}

BrowserKeyAndroid::~BrowserKeyAndroid() = default;

std::vector<uint8_t> BrowserKeyAndroid::GetIdentifier() const {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_BrowserKey_getIdentifier(env, impl_);
}

jni_zero::ScopedJavaLocalRef<jobject> BrowserKeyAndroid::GetPrivateKey() const {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_BrowserKey_getPrivateKey(env, impl_);
}

std::vector<uint8_t> BrowserKeyAndroid::GetPublicKeyAsSPKI() const {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_BrowserKey_getPublicKeyAsSPKI(env, impl_);
}

scoped_refptr<net::SSLPrivateKey> BrowserKeyAndroid::GetSSLPrivateKey() const {
  JNIEnv* env = jni_zero::AttachCurrentThread();

  std::vector<uint8_t> spki_bytes = GetPublicKeyAsSPKI();
  if (spki_bytes.empty()) {
    return nullptr;
  }

  // Use BoringSSL's public API to parse the SPKI DER bytes.
  CBS cbs;
  CBS_init(&cbs, spki_bytes.data(), spki_bytes.size());
  bssl::UniquePtr<EVP_PKEY> public_key(EVP_parse_public_key(&cbs));

  if (!public_key) {
    return nullptr;
  }

  return net::WrapJavaPrivateKey(std::move(public_key),
                                 Java_BrowserKey_getPrivateKey(env, impl_));
}

std::optional<std::vector<uint8_t>> BrowserKeyAndroid::SignSlowly(
    base::span<const uint8_t> data) const {
  std::vector<uint8_t> signature;
  // TODO(crbug.com/432304139): Add support for other algorithms.
  if (!net::android::SignWithPrivateKey(GetPrivateKey(), "SHA256withECDSA",
                                        data, &signature)) {
    return std::nullopt;
  }
  return signature;
}

}  // namespace client_certificates
