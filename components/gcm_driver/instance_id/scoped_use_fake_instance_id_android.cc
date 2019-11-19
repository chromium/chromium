// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/scoped_use_fake_instance_id_android.h"

#include "base/android/jni_android.h"
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

}  // namespace instance_id
