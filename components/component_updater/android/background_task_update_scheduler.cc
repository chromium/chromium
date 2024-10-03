// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/android/background_task_update_scheduler.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/component_updater/android/background_task_update_scheduler_jni_headers/UpdateScheduler_jni.h"

namespace component_updater {

namespace {

// Delay of running component updates after the background task fires to give
// enough time for async component registration.
const base::TimeDelta kOnStartTaskDelay = base::Seconds(2);

}  // namespace

BackgroundTaskUpdateScheduler::BackgroundTaskUpdateScheduler() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  j_update_scheduler_.Reset(Java_UpdateScheduler_getInstance(env));
  Java_UpdateScheduler_setNativeScheduler(env, j_update_scheduler_,
                                          reinterpret_cast<intptr_t>(this));
}

BackgroundTaskUpdateScheduler::~BackgroundTaskUpdateScheduler() = default;

void BackgroundTaskUpdateScheduler::Schedule(
    base::TimeDelta initial_delay,
    base::TimeDelta delay,
    const UserTask& user_task,
    const OnStopTaskCallback& on_stop) {
  user_task_ = user_task;
  on_stop_ = on_stop;
  Java_UpdateScheduler_schedule(
      jni_zero::AttachCurrentThread(), j_update_scheduler_,
      initial_delay.InMilliseconds(), delay.InMilliseconds());
}

void BackgroundTaskUpdateScheduler::Stop() {
  Java_UpdateScheduler_cancelTask(jni_zero::AttachCurrentThread(),
                                  j_update_scheduler_);
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void BackgroundTaskUpdateScheduler::OnStartTask(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  // Component registration is async. Add some delay to give some time for the
  // registration.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BackgroundTaskUpdateScheduler::OnStartTaskDelayed,
                     weak_ptr_factory_.GetWeakPtr()),
      kOnStartTaskDelay);
}

void BackgroundTaskUpdateScheduler::OnStopTask(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  DCHECK(on_stop_);
  on_stop_.Run();
}

void BackgroundTaskUpdateScheduler::OnStartTaskDelayed() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  if (!user_task_) {
    LOG(WARNING) << "No components registered to update";
    Java_UpdateScheduler_finishTask(env, j_update_scheduler_,
                                    /*reschedule=*/false);
    return;
  }
  user_task_.Run(base::BindOnce(&Java_UpdateScheduler_finishTask,
                                base::Unretained(env), j_update_scheduler_,
                                /*reschedule=*/true));
}

}  // namespace component_updater
