// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/trigger_context.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "components/segmentation_platform/public/jni_headers/TriggerContext_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace segmentation_platform {

TriggerContext::TriggerContext() = default;

TriggerContext::~TriggerContext() = default;

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject> TriggerContext::CreateJavaObject()
    const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_TriggerContext_createTriggerContext(env);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace segmentation_platform
