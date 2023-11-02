// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/initializer.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"

namespace chrome_cleaner {

namespace {

std::unique_ptr<base::WaitableEvent> SignalInitializationDone() {
  base::win::ScopedHandle init_done_notifier;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  uint32_t handle = 0;
  if (command_line->HasSwitch(kInitDoneNotifierSwitch) &&
      base::StringToUint(
          command_line->GetSwitchValueNative(kInitDoneNotifierSwitch),
          &handle)) {
    init_done_notifier.Set(base::win::Uint32ToHandle(handle));
  }

  std::unique_ptr<base::WaitableEvent> notifier_event;
  if (init_done_notifier.IsValid()) {
    notifier_event =
        std::make_unique<base::WaitableEvent>(std::move(init_done_notifier));

    // Wake up the test that is waiting on this event.
    notifier_event->Signal();
  }
  return notifier_event;
}

}  // namespace

bool InitializeOSUtils() {
  chrome_cleaner::InitializeDiskUtil();

  if (!chrome_cleaner::PreFetchedPaths::GetInstance()->Initialize()) {
    LOG(ERROR) << "PreFetchedPaths failed to initialize";
    return false;
  }

  // Call into the random number generator to initialize it. This must be done
  // once before lowering the token in the sandbox target process.
  std::ignore = base::RandUint64();

  return true;
}

void NotifyInitializationDone() {
  SignalInitializationDone();
}

void NotifyInitializationDoneForTesting() {
  std::unique_ptr<base::WaitableEvent> notifier_event =
      SignalInitializationDone();

  if (notifier_event) {
    // The event has ResetPolicy AUTOMATIC, so after the test is woken up it is
    // immediately reset. Wait at most 5 seconds for the test to signal that
    // it's ready using the same event before continuing. If the test takes
    // longer than that stop waiting to prevent hangs.
    notifier_event->TimedWait(base::Seconds(5));
  }
}

}  // namespace chrome_cleaner
