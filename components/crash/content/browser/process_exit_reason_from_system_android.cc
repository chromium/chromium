// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/process_exit_reason_from_system_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/crash/content/browser/jni_headers/ProcessExitReasonFromSystem_jni.h"

namespace crash_reporter {

// static
void ProcessExitReasonFromSystem::RecordExitReasonToUma(
    base::ProcessId pid,
    const std::string& uma_name) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ProcessExitReasonFromSystem_recordExitReasonToUma(
      env, pid, base::android::ConvertUTF8ToJavaString(env, uma_name));
}

}  // namespace crash_reporter
