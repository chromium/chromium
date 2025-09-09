// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/android/android_requirements.h"

#include "base/android/jni_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/password_manager/core/browser/android/jni/AndroidRequirements_jni.h"

namespace password_manager {

bool IsPasswordManagerAvailable() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AndroidRequirements_isPasswordManagerAvailable(
      env, Java_AndroidRequirements_get(env));
}

bool HasMinGmsVersion() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AndroidRequirements_hasMinGmsVersion(
      env, Java_AndroidRequirements_get(env));
}

bool HasInternalBackend() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AndroidRequirements_hasInternalBackend(
      env, Java_AndroidRequirements_get(env));
}

void SetAndroidRequirementsForTesting(bool has_min_gms_version,
                                      bool has_internal_backend) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AndroidRequirements_setForTesting(
      env, Java_AndroidRequirements_Constructor(env, has_min_gms_version,
                                                has_internal_backend));
}

}  // namespace password_manager
