// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/background_task_scheduler/internal/android/native_task_scheduler.h"

#include "base/android/jni_android.h"
#include "components/background_task_scheduler/internal/android/task_info_bridge.h"
#include "components/background_task_scheduler/internal/jni_headers/NativeTaskScheduler_jni.h"

namespace background_task {

NativeTaskScheduler::NativeTaskScheduler() = default;

NativeTaskScheduler::~NativeTaskScheduler() = default;

bool NativeTaskScheduler::Schedule(const TaskInfo& task_info) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_NativeTaskScheduler_schedule(
      env, TaskInfoBridge::CreateTaskInfo(env, task_info));
}

void NativeTaskScheduler::Cancel(int task_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NativeTaskScheduler_cancel(env, task_id);
}

}  // namespace background_task
