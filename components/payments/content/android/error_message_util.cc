// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>
#include <set>
#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/payments/core/error_message_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/jni_headers/ErrorMessageUtil_jni.h"

namespace payments {
namespace android {

// static
base::android::ScopedJavaLocalRef<jstring>
JNI_ErrorMessageUtil_GetNotSupportedErrorMessage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& jmethods) {
  std::vector<std::string> method_vector;
  base::android::AppendJavaStringArrayToStringVector(env, jmethods,
                                                     &method_vector);
  return base::android::ConvertUTF8ToJavaString(
      env, GetNotSupportedErrorMessage(std::set<std::string>(
               method_vector.begin(), method_vector.end())));
}

}  // namespace android
}  // namespace payments
