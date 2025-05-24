// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jni_delegate_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/cached_flags/jni_headers/CachedFlagUtils_jni.h"

namespace cached_flags {

JniDelegateImpl::~JniDelegateImpl() = default;

void JniDelegateImpl::CacheNativeFlagsImmediately(
    const std::map<std::string, std::string>& features) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CachedFlagUtils_cacheNativeFlagsImmediately(env, features);
}

void JniDelegateImpl::CacheFeatureParamsImmediately(
    const std::map<std::string, std::map<std::string, std::string>>&
        feature_params) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CachedFlagUtils_cacheFeatureParamsImmediately(env, feature_params);
}

void JniDelegateImpl::EraseNativeFlagCachedValues(
    const std::vector<std::string>& features_to_erase) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CachedFlagUtils_eraseNativeFlagCachedValues(env, features_to_erase);
}

void JniDelegateImpl::EraseFeatureParamCachedValues(
    const std::vector<std::string>& feature_params_to_erase) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CachedFlagUtils_eraseFeatureParamCachedValues(env,
                                                     feature_params_to_erase);
}

}  // namespace cached_flags
