// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/scoped_use_fake_instance_id_android.h"

#include <string>
#include <string_view>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/gcm_driver/instance_id/android/test_support_jni_headers/FakeInstanceIDWithSubtype_jni.h"

using base::android::AttachCurrentThread;

namespace instance_id {

ScopedUseFakeInstanceIDAndroid::ScopedUseFakeInstanceIDAndroid() {
  JNIEnv* env = AttachCurrentThread();
  previous_value_ =
      Java_FakeInstanceIDWithSubtype_clearDataAndSetEnabled(env, true);
}

ScopedUseFakeInstanceIDAndroid::~ScopedUseFakeInstanceIDAndroid() {
  JNIEnv* env = AttachCurrentThread();
  Java_FakeInstanceIDWithSubtype_clearDataAndSetEnabled(env, previous_value_);
}

bool ScopedUseFakeInstanceIDAndroid::DispatchMessageForToken(
    const std::string& token,
    std::string_view raw_data) {
  JNIEnv* env = AttachCurrentThread();
  return Java_FakeInstanceIDWithSubtype_dispatchMessageForToken(
      env, token, base::android::ToJavaByteArray(env, raw_data));
}

}  // namespace instance_id

DEFINE_JNI(FakeInstanceIDWithSubtype)
