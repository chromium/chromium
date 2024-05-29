// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/child_process_crash_observer_android.h"

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/crash/content/browser/crash_metrics_reporter_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/crash/content/browser/jni_headers/ChildProcessCrashObserver_jni.h"

namespace crash_reporter {

ChildProcessCrashObserver::ChildProcessCrashObserver() {
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
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
