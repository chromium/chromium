// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_ANDROID_INPUT_CONTEXT_ANDROID_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_ANDROID_INPUT_CONTEXT_ANDROID_H_

#include <jni.h>
#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/gtest_prod_util.h"
#include "components/segmentation_platform/public/input_context.h"

using base::android::JavaParamRef;

namespace segmentation_platform {

// Android implementation of InputContext, contains methods to convert a Java
// InputContext into a native InputContext along with all its keys and values.
class InputContextAndroid {
 public:
  static scoped_refptr<InputContext> ToNativeInputContext(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& j_input_context);

  static void FromJavaParams(
      JNIEnv* env,
      const jlong target,
      const base::android::JavaRef<jobjectArray>& jboolean_keys,
      const base::android::JavaRef<jbooleanArray>& jboolean_values,
      const base::android::JavaRef<jobjectArray>& jint_keys,
      const base::android::JavaRef<jintArray>& jint_values,
      const base::android::JavaRef<jobjectArray>& jfloat_keys,
      const base::android::JavaRef<jfloatArray>& jfloat_values,
      const base::android::JavaRef<jobjectArray>& jdouble_keys,
      const base::android::JavaRef<jdoubleArray>& jdouble_values,
      const base::android::JavaRef<jobjectArray>& jstring_keys,
      const base::android::JavaRef<jobjectArray>& jstring_values,
      const base::android::JavaRef<jobjectArray>& jtime_keys,
      const base::android::JavaRef<jlongArray>& jtime_values,
      const base::android::JavaRef<jobjectArray>& jint64_keys,
      const base::android::JavaRef<jlongArray>& jint64_values,
      const base::android::JavaRef<jobjectArray>& jurl_keys,
      const base::android::JavaRef<jobjectArray>& jurl_values);

 private:
  FRIEND_TEST_ALL_PREFIXES(InputContextAndroidTest, FromJavaParams);
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_ANDROID_INPUT_CONTEXT_ANDROID_H_
