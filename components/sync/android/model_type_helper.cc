// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/sync/android/jni_headers/ModelTypeHelper_jni.h"
#include "components/sync/base/model_type.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace syncer {

static ScopedJavaLocalRef<jstring>
JNI_ModelTypeHelper_ModelTypeToNotificationType(
    JNIEnv* env,
    jint model_type_int) {
  std::string model_type_string;
  ModelType model_type = static_cast<ModelType>(model_type_int);
  if (!RealModelTypeToNotificationType(model_type, &model_type_string)) {
    NOTREACHED() << "No string representation of model type " << model_type;
  }
  return base::android::ConvertUTF8ToJavaString(env, model_type_string);
}

}  // namespace syncer
