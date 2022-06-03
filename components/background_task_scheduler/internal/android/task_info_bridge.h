// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BACKGROUND_TASK_SCHEDULER_INTERNAL_ANDROID_TASK_INFO_BRIDGE_H_
#define COMPONENTS_BACKGROUND_TASK_SCHEDULER_INTERNAL_ANDROID_TASK_INFO_BRIDGE_H_

#include "components/background_task_scheduler/task_info.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

namespace background_task {

// Helper class to convert TaskInfo params to Java.
class TaskInfoBridge {
 public:
  static base::android::ScopedJavaLocalRef<jobject> CreateTaskInfo(
      JNIEnv* env,
      const TaskInfo& task_info);
};

}  // namespace background_task

#endif  // COMPONENTS_BACKGROUND_TASK_SCHEDULER_INTERNAL_ANDROID_TASK_INFO_BRIDGE_H_
