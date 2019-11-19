// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/test/test_initializer.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "chrome/updater/updater_constants.h"

namespace updater {

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
    notifier_event->Signal();
  }

  return notifier_event;
}

}  // namespace

void NotifyInitializationDoneForTesting() {
  auto notifier_event = SignalInitializationDone();

  // The event has ResetPolicy AUTOMATIC, so after the test is woken up it is
  // immediately reset. Wait at most 5 seconds for the test to signal that
  // it's ready using the same event before continuing. If the test takes
  // longer than that stop waiting to prevent hangs.
  if (notifier_event)
    notifier_event->TimedWait(base::TimeDelta::FromSeconds(5));
}

}  // namespace updater
