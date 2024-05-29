// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/android/input_context_android.h"

#include <jni.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/types/processed_value.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/segmentation_platform/public/jni_headers/InputContext_jni.h"

using base::android::JavaParamRef;

namespace segmentation_platform {

namespace {

// Takes a pair of java arrays with keys and values, converts them to native
// types and inserts then at the given InputContext. Keys are always strings.
// Values are converted from a java array of type JV (e.g. jintArray) to an
// std::vector<NV> where NV is a native value type (e.g. int). A function to
// convert from java array to vector of native values is required.
template <class JV, class NV>
void ConvertAndAddToInputContext(
    JNIEnv* env,
    InputContext* input_context,
    const base::android::JavaRef<jobjectArray>& java_keys,
    const base::android::JavaRef<JV>& java_values,
    void (*converter)(JNIEnv*,
                      const base::android::JavaRef<JV>&,
                      std::vector<NV>*)) {
  std::vector<std::string> native_keys;
  std::vector<NV> native_values;

  base::android::AppendJavaStringArrayToStringVector(env, java_keys,
                                                     &native_keys);
  converter(env, java_values, &native_values);

  CHECK(native_keys.size() == native_values.size())
      << "Key and value count must be equal: " << native_keys.size()
      << " != " << native_values.size();

  for (size_t i = 0; i < native_keys.size(); i++) {
    input_context->metadata_args.emplace(native_keys[i], native_values[i]);
  }
}

void JavaGURLArrayToGURLVector(
    JNIEnv* env,
    const base::android::JavaRef<jobjectArray>& j_gurls,
    std::vector<GURL>* ret) {
  *ret = jni_zero::FromJniArray<std::vector<GURL>>(env, j_gurls);
}

static void JavaLongArrayToBaseTimeVector(
    JNIEnv* env,
    const base::android::JavaRef<jlongArray>& java_values,
    std::vector<base::Time>* out_times) {
  std::vector<int64_t> time_values;
  base::android::JavaLongArrayToInt64Vector(env, java_values, &time_values);
  for (int64_t time_value : time_values) {
    out_times->emplace_back(
        base::Time::FromMillisecondsSinceUnixEpoch(time_value));
  }
}

}  // namespace

// static
scoped_refptr<InputContext> InputContextAndroid::ToNativeInputContext(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_input_context) {
  if (!j_input_context) {
    return nullptr;
  }
  scoped_refptr<InputContext> input_context =
      base::MakeRefCounted<InputContext>();
  Java_InputContext_fillNativeInputContext(
      env, j_input_context, reinterpret_cast<intptr_t>(input_context.get()));

  return input_context;
}

void InputContextAndroid::FromJavaParams(
    JNIEnv* env,
    const jlong input_context_ptr,
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
    const base::android::JavaRef<jobjectArray>& jurl_values) {
  InputContext* input_context =
      reinterpret_cast<InputContext*>(input_context_ptr);

  ConvertAndAddToInputContext(env, input_context, jboolean_keys,
                              jboolean_values,
                              base::android::JavaBooleanArrayToBoolVector);
  ConvertAndAddToInputContext(env, input_context, jint_keys, jint_values,
                              base::android::JavaIntArrayToIntVector);
  ConvertAndAddToInputContext(env, input_context, jfloat_keys, jfloat_values,
                              base::android::JavaFloatArrayToFloatVector);
  ConvertAndAddToInputContext(env, input_context, jdouble_keys, jdouble_values,
                              base::android::JavaDoubleArrayToDoubleVector);
  ConvertAndAddToInputContext<jobjectArray, std::string>(
      env, input_context, jstring_keys, jstring_values,
      base::android::AppendJavaStringArrayToStringVector);
  ConvertAndAddToInputContext(env, input_context, jtime_keys, jtime_values,
                              JavaLongArrayToBaseTimeVector);
  ConvertAndAddToInputContext(env, input_context, jint64_keys, jint64_values,
                              base::android::JavaLongArrayToInt64Vector);
  ConvertAndAddToInputContext(env, input_context, jurl_keys, jurl_values,
                              JavaGURLArrayToGURLVector);
}

static void JNI_InputContext_FillNative(
    JNIEnv* env,
    const jlong input_context_ptr,
    const JavaParamRef<jobjectArray>& jboolean_keys,
    const JavaParamRef<jbooleanArray>& jboolean_values,
    const JavaParamRef<jobjectArray>& jint_keys,
    const JavaParamRef<jintArray>& jint_values,
    const JavaParamRef<jobjectArray>& jfloat_keys,
    const JavaParamRef<jfloatArray>& jfloat_values,
    const JavaParamRef<jobjectArray>& jdouble_keys,
    const JavaParamRef<jdoubleArray>& jdouble_values,
    const JavaParamRef<jobjectArray>& jstring_keys,
    const JavaParamRef<jobjectArray>& jstring_values,
    const JavaParamRef<jobjectArray>& jtime_keys,
    const JavaParamRef<jlongArray>& jtime_values,
    const JavaParamRef<jobjectArray>& jint64_keys,
    const JavaParamRef<jlongArray>& jint64_values,
    const JavaParamRef<jobjectArray>& jurl_keys,
    const JavaParamRef<jobjectArray>& jurl_values) {
  segmentation_platform::InputContextAndroid::FromJavaParams(
      env, input_context_ptr, jboolean_keys, jboolean_values, jint_keys,
      jint_values, jfloat_keys, jfloat_values, jdouble_keys, jdouble_values,
      jstring_keys, jstring_values, jtime_keys, jtime_values, jint64_keys,
      jint64_values, jurl_keys, jurl_values);
}

}  // namespace segmentation_platform
