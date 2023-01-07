// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_CHILD_PROCESS_CRASH_OBSERVER_ANDROID_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_CHILD_PROCESS_CRASH_OBSERVER_ANDROID_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/crash/content/browser/child_exit_observer_android.h"

namespace crash_reporter {

// Records metrics and initiates minidump upload in response to child process
// crashes.
class ChildProcessCrashObserver
    : public crash_reporter::ChildExitObserver::Client {
 public:
  ChildProcessCrashObserver();

  ChildProcessCrashObserver(const ChildProcessCrashObserver&) = delete;
  ChildProcessCrashObserver& operator=(const ChildProcessCrashObserver&) =
      delete;

  ~ChildProcessCrashObserver() override;

  // crash_reporter::ChildExitObserver::Client implementation:
  void OnChildExit(const ChildExitObserver::TerminationInfo& info) override;

 private:
  void OnChildExitImpl(const ChildExitObserver::TerminationInfo& info);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_CHILD_PROCESS_CRASH_OBSERVER_ANDROID_H_
