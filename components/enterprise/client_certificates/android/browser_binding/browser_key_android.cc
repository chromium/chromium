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
#include "crypto/evp.h"
#include "net/android/keystore.h"
#include "net/base/net_errors.h"
#include "net/ssl/ssl_platform_key_android.h"
#include "net/ssl/ssl_platform_key_util.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/threaded_ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
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

  std::vector<uint8_t> spki = GetPublicKeyAsSPKI();
  if (spki.empty()) {
    return nullptr;
  }

  // Use BoringSSL's public API to parse the SPKI DER bytes.
  bssl::UniquePtr<EVP_PKEY> public_key = crypto::evp::PublicKeyFromBytes(spki);
  if (!public_key) {
    return nullptr;
  }

  return net::WrapJavaPrivateKey(std::move(public_key),
                                 Java_BrowserKey_getPrivateKey(env, impl_));
}

std::optional<std::vector<uint8_t>> BrowserKeyAndroid::Sign(
    const std::vector<uint8_t>& data) const {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  jni_zero::ScopedJavaLocalRef<jbyteArray> data_json_jarray =
      base::android::ToJavaByteArray(env, data);
  jni_zero::ScopedJavaLocalRef<jbyteArray> signature_jarray =
      Java_BrowserKey_sign(env, impl_, data_json_jarray);

  if (signature_jarray.is_null()) {
    return std::nullopt;
  }

  std::vector<uint8_t> signature_output;
  base::android::AppendJavaByteArrayToByteVector(env, signature_jarray,
                                                 &signature_output);
  return signature_output;
}

BrowserKey::SecurityLevel BrowserKeyAndroid::GetSecurityLevel() const {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return static_cast<BrowserKey::SecurityLevel>(
      Java_BrowserKey_getSecurityLevel(env, impl_));
}

}  // namespace client_certificates

DEFINE_JNI(BrowserKey)
