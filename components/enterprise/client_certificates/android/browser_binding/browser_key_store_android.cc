// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/android/browser_binding/browser_key_store_android.h"

#include "base/android/scoped_java_ref.h"
#include "base/numerics/safe_conversions.h"
#include "components/enterprise/client_certificates/android/browser_binding/browser_key_android.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize ToJniType()/FromJniType()
#include "components/enterprise/client_certificates/android/browser_binding_jni/BrowserKeyStore_jni.h"

namespace client_certificates {

namespace {

jni_zero::ScopedJavaLocalRef<jobject>
ConvertToListOfPublicKeyCredentialParameters(
    JNIEnv* env,
    const std::vector<device::PublicKeyCredentialParams::CredentialInfo>&
        credential_infos) {
  std::vector<int32_t> types;
  types.reserve(credential_infos.size());
  std::vector<int32_t> algorithms;
  algorithms.reserve(credential_infos.size());
  for (auto& credential_info : credential_infos) {
    types.push_back(base::strict_cast<int32_t>(credential_info.type));
    algorithms.push_back(credential_info.algorithm);
  }
  return Java_BrowserKeyStore_createListOfCredentialParameters(env, types,
                                                               algorithms);
}
}  // namespace

scoped_refptr<BrowserKeyStore> CreateBrowserKeyStoreInstance() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return base::MakeRefCounted<BrowserKeyStoreAndroid>(
      Java_BrowserKeyStore_getInstance(env));
}

BrowserKeyStoreAndroid::BrowserKeyStoreAndroid(
    jni_zero::ScopedJavaLocalRef<jobject> impl)
    : impl_(impl) {}

std::unique_ptr<BrowserKey>
BrowserKeyStoreAndroid::GetOrCreateBrowserKeyForCredentialId(
    const std::vector<uint8_t>& credential_id,
    const std::vector<device::PublicKeyCredentialParams::CredentialInfo>&
        allowed_credentials) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_BrowserKeyStore_getOrCreateBrowserKeyForCredentialId(
      env, impl_, credential_id,
      ConvertToListOfPublicKeyCredentialParameters(env, allowed_credentials));
}

bool BrowserKeyStoreAndroid::GetDeviceSupportsHardwareKeys() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_BrowserKeyStore_getDeviceSupportsHardwareKeys(env);
}

void BrowserKeyStoreAndroid::DeleteBrowserKey(
    const std::vector<uint8_t>& credential_id) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_BrowserKeyStore_deleteBrowserKey(env, impl_, credential_id);
}

BrowserKeyStoreAndroid::~BrowserKeyStoreAndroid() = default;

}  // namespace client_certificates

DEFINE_JNI(BrowserKeyStore)
