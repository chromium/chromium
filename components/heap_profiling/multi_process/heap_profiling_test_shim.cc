// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/multi_process/heap_profiling_test_shim.h"

#include "base/android/jni_string.h"
#include "components/heap_profiling/multi_process/jni_headers/HeapProfilingTestShim_jni.h"
#include "components/heap_profiling/multi_process/test_driver.h"
#include "components/services/heap_profiling/public/cpp/settings.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static jlong JNI_HeapProfilingTestShim_Init(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  HeapProfilingTestShim* profiler = new HeapProfilingTestShim(env, obj);
  return reinterpret_cast<intptr_t>(profiler);
}

HeapProfilingTestShim::HeapProfilingTestShim(JNIEnv* env, jobject obj) {}
HeapProfilingTestShim::~HeapProfilingTestShim() = default;

void HeapProfilingTestShim::Destroy(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  delete this;
}

jboolean HeapProfilingTestShim::RunTestForMode(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& mode,
    jboolean dynamically_start_profiling,
    const base::android::JavaParamRef<jstring>& stack_mode,
    jboolean should_sample,
    jboolean sample_everything) {
  heap_profiling::TestDriver driver;
  heap_profiling::TestDriver::Options options;
  options.mode = heap_profiling::ConvertStringToMode(
      base::android::ConvertJavaStringToUTF8(mode));
  options.stack_mode = heap_profiling::ConvertStringToStackMode(
      base::android::ConvertJavaStringToUTF8(stack_mode));
  options.profiling_already_started = !dynamically_start_profiling;
  return driver.RunTest(options);
}
