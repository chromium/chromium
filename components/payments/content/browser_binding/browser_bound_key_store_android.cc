// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/browser_binding/browser_bound_key_store_android.h"

#include "base/android/scoped_java_ref.h"
#include "base/numerics/safe_conversions.h"
#include "components/payments/content/browser_binding/browser_bound_key_android.h"
#include "device/fido/public_key_credential_params.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize ToJniType()/FromJniType()
#include "components/payments/content/android/spc/browser_binding_jni/BrowserBoundKeyStore_jni.h"

namespace payments {
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
  return Java_BrowserBoundKeyStore_createListOfCredentialParameters(env, types,
                                                                    algorithms);
}

}  // namespace

BrowserBoundKeyStoreAndroid::BrowserBoundKeyStoreAndroid(
    jni_zero::ScopedJavaLocalRef<jobject> impl)
    : impl_(impl) {}

std::unique_ptr<BrowserBoundKey>
BrowserBoundKeyStoreAndroid::GetOrCreateBrowserBoundKeyForCredentialId(
    const std::vector<uint8_t>& credential_id,
    const std::vector<device::PublicKeyCredentialParams::CredentialInfo>&
        allowed_credentials) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_BrowserBoundKeyStore_getOrCreateBrowserBoundKeyForCredentialId(
      env, impl_, credential_id,
      ConvertToListOfPublicKeyCredentialParameters(env, allowed_credentials));
}

void BrowserBoundKeyStoreAndroid::DeleteBrowserBoundKey(
    std::vector<uint8_t> bbk_id) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_BrowserBoundKeyStore_deleteBrowserBoundKey(env, impl_, bbk_id);
}

bool BrowserBoundKeyStoreAndroid::GetDeviceSupportsHardwareKeys() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return Java_BrowserBoundKeyStore_getDeviceSupportsHardwareKeys(env);
}

BrowserBoundKeyStoreAndroid::~BrowserBoundKeyStoreAndroid() = default;

scoped_refptr<BrowserBoundKeyStore> GetBrowserBoundKeyStoreInstance(
    BrowserBoundKeyStore::Config config) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return base::MakeRefCounted<BrowserBoundKeyStoreAndroid>(
      Java_BrowserBoundKeyStore_getInstance(env));
}

}  // namespace payments

DEFINE_JNI(BrowserBoundKeyStore)
