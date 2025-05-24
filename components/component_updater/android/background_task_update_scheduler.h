// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_ANDROID_BACKGROUND_TASK_UPDATE_SCHEDULER_H_
#define COMPONENTS_COMPONENT_UPDATER_ANDROID_BACKGROUND_TASK_UPDATE_SCHEDULER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/component_updater/update_scheduler.h"

namespace component_updater {

// Native-side implementation of the component update scheduler using the
// BackgroundTaskScheduler.
class BackgroundTaskUpdateScheduler : public UpdateScheduler {
 public:
  BackgroundTaskUpdateScheduler();

  BackgroundTaskUpdateScheduler(const BackgroundTaskUpdateScheduler&) = delete;
  BackgroundTaskUpdateScheduler& operator=(
      const BackgroundTaskUpdateScheduler&) = delete;

  ~BackgroundTaskUpdateScheduler() override;

  // UpdateScheduler:
  void Schedule(base::TimeDelta initial_delay,
                base::TimeDelta delay,
                const UserTask& user_task,
                const OnStopTaskCallback& on_stop) override;
  void Stop() override;

  // JNI:
  void OnStartTask(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj);
  void OnStopTask(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

 private:
  void OnStartTaskDelayed();

  base::android::ScopedJavaGlobalRef<jobject> j_update_scheduler_;
  UserTask user_task_;
  OnStopTaskCallback on_stop_;

  base::WeakPtrFactory<BackgroundTaskUpdateScheduler> weak_ptr_factory_{this};
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_ANDROID_BACKGROUND_TASK_UPDATE_SCHEDULER_H_
