// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_SHUTDOWN_WATCHDOG_H_
#define CHROME_CHROME_CLEANER_OS_SHUTDOWN_WATCHDOG_H_

#include "base/functional/callback.h"
#include "base/synchronization/lock.h"
#include "base/threading/watchdog.h"
#include "base/time/time.h"

namespace chrome_cleaner {

// This implements a watchdog timer that ensures the process eventually
// terminates, even if some threads are blocked or being kept alive for
// some reason. This is not expected to trigger if process shutdown is working
// correctly. The triggering of the alarm indicates a sign of trouble, and so
// the Alarm() method will log some diagnostic information before shutting down
// the process.
class ShutdownWatchdog : public base::Watchdog {
 public:
  // An |AlarmCallback| returns the exit code that should be used when
  // terminating the process.
  typedef base::OnceCallback<int()> AlarmCallback;

  // Create a watchdog that waits for |duration| (after the watchdog is armed)
  // before shutting down the process. |callback| is called before the process
  // is terminated and should return the exit code to be used when exiting.
  ShutdownWatchdog(const base::TimeDelta& duration, AlarmCallback callback);

  ShutdownWatchdog(const ShutdownWatchdog&) = delete;
  ShutdownWatchdog& operator=(const ShutdownWatchdog&) = delete;

  ~ShutdownWatchdog() override;

 private:
  // This method is called by the watchdog thread when a timeout occurs.
  void Alarm() override;

  // Callback called when the process execution is aborted.
  AlarmCallback callback_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_SHUTDOWN_WATCHDOG_H_
