// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/background_task_scheduler/internal/android/task_info_bridge.h"

#include "base/android/jni_string.h"

// Must come after headers that provide symbols used by @JniType.
#include "components/background_task_scheduler/internal/jni_headers/TaskInfoBridge_jni.h"

namespace background_task {

namespace {

jni_zero::ScopedJavaLocalRef<jobject> CreatePeriodicInfo(
    JNIEnv* env,
    const PeriodicInfo& timing_info) {
  return Java_TaskInfoBridge_createPeriodicInfo(
      env, timing_info.interval_ms, timing_info.flex_ms,
      timing_info.expires_after_window_end_time);
}

jni_zero::ScopedJavaLocalRef<jobject> CreateOneOffInfo(
    JNIEnv* env,
    const OneOffInfo& timing_info) {
  return Java_TaskInfoBridge_createOneOffInfo(
      env, timing_info.window_start_time_ms, timing_info.window_end_time_ms,
      timing_info.expires_after_window_end_time);
}

}  // namespace

// static
jni_zero::ScopedJavaLocalRef<jobject> TaskInfoBridge::CreateTaskInfo(
    JNIEnv* env,
    const TaskInfo& task_info) {
  // Only one type of timing info should be active.
  DCHECK((task_info.periodic_info.has_value() +
          task_info.one_off_info.has_value()) == 1);
  jni_zero::ScopedJavaLocalRef<jobject> j_timing_info;
  if (task_info.periodic_info.has_value()) {
    j_timing_info = CreatePeriodicInfo(env, task_info.periodic_info.value());
  } else if (task_info.one_off_info.has_value()) {
    j_timing_info = CreateOneOffInfo(env, task_info.one_off_info.value());
  }

  return Java_TaskInfoBridge_createTaskInfo(
      env, task_info.task_id, j_timing_info, task_info.extras,
      task_info.network_type, task_info.requires_charging,
      task_info.is_persisted, task_info.update_current);
}

}  // namespace background_task

DEFINE_JNI(TaskInfoBridge)
