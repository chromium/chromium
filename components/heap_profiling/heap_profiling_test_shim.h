// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAP_PROFILING_HEAP_PROFILING_TEST_SHIM_H_
#define COMPONENTS_HEAP_PROFILING_HEAP_PROFILING_TEST_SHIM_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"

// This class implements the native methods of HeapProfilingTestShim.java, and
// acts as a bridge to TestDriver. Note that this class is only used for
// testing.
class HeapProfilingTestShim {
 public:
  HeapProfilingTestShim(JNIEnv* env, jobject obj);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  jboolean RunTestForMode(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& mode,
      jboolean dynamically_start_profiling,
      const base::android::JavaParamRef<jstring>& stack_mode,
      jboolean should_sample,
      jboolean sample_everything);

 private:
  ~HeapProfilingTestShim();

  DISALLOW_COPY_AND_ASSIGN(HeapProfilingTestShim);
};

#endif  // COMPONENTS_HEAP_PROFILING_HEAP_PROFILING_TEST_SHIM_H_
