// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/child_process_crash_observer_android.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/crash/android/jni_headers/ChildProcessCrashObserver_jni.h"
#include "components/crash/content/browser/crash_metrics_reporter_android.h"

namespace crash_reporter {

ChildProcessCrashObserver::ChildProcessCrashObserver() {
  task_runner_ = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT});
}

ChildProcessCrashObserver::~ChildProcessCrashObserver() = default;

void ChildProcessCrashObserver::OnChildExit(
    const ChildExitObserver::TerminationInfo& info) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ChildProcessCrashObserver::OnChildExitImpl,
                                base::Unretained(this), info));
}

void ChildProcessCrashObserver::OnChildExitImpl(
    const ChildExitObserver::TerminationInfo& info) {
  crash_reporter::CrashMetricsReporter::GetInstance()->ChildProcessExited(info);

  if (!info.is_crashed()) {
    return;
  }

  base::ScopedBlockingCall sbc(FROM_HERE, base::BlockingType::WILL_BLOCK);

  // Hop over to Java to attempt to attach the logcat to the crash. This may
  // fail, which is ok -- if it does, the crash will still be uploaded on the
  // next browser start.
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ChildProcessCrashObserver_childCrashed(env, info.pid);
}

}  // namespace crash_reporter
