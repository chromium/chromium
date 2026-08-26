// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAP_PROFILING_MULTI_PROCESS_HEAP_PROFILING_TEST_SHIM_H_
#define COMPONENTS_HEAP_PROFILING_MULTI_PROCESS_HEAP_PROFILING_TEST_SHIM_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

// This class implements the native methods of HeapProfilingTestShim.java, and
// acts as a bridge to TestDriver. Note that this class is only used for
// testing.
class HeapProfilingTestShim {
 public:
  HeapProfilingTestShim(JNIEnv* env,
                        const base::android::JavaRef<jobject>& obj);
  void Destroy(JNIEnv* env);

  HeapProfilingTestShim(const HeapProfilingTestShim&) = delete;
  HeapProfilingTestShim& operator=(const HeapProfilingTestShim&) = delete;

  bool RunTestForMode(JNIEnv* env,
                      const base::android::JavaRef<jstring>& mode,
                      bool dynamically_start_profiling,
                      const base::android::JavaRef<jstring>& stack_mode,
                      bool should_sample,
                      bool sample_everything);

 private:
  ~HeapProfilingTestShim();
};

#endif  // COMPONENTS_HEAP_PROFILING_MULTI_PROCESS_HEAP_PROFILING_TEST_SHIM_H_
