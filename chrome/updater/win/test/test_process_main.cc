// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "chrome/updater/win/test/test_initializer.h"
#include "chrome/updater/win/test/test_strings.h"

int main(int, char**) {
  bool success = base::CommandLine::Init(0, nullptr);
  DCHECK(success);

  updater::NotifyInitializationDoneForTesting();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(updater::kTestSleepMinutesSwitch)) {
    std::string value =
        command_line->GetSwitchValueASCII(updater::kTestSleepMinutesSwitch);
    int sleep_minutes = 0;
    if (base::StringToInt(value, &sleep_minutes) && sleep_minutes > 0) {
      VLOG(1) << "Process is sleeping for " << sleep_minutes << " minutes";
      ::Sleep(base::TimeDelta::FromMinutes(sleep_minutes).InMilliseconds());
    } else {
      LOG(ERROR) << "Invalid sleep delay value " << value;
    }
    NOTREACHED();
    return 1;
  }

  if (command_line->HasSwitch(updater::kTestEventToSignal)) {
    VLOG(1) << "Process is signaling event '" << updater::kTestEventToSignal
            << "'";
    base::string16 event_name =
        command_line->GetSwitchValueNative(updater::kTestEventToSignal);
    base::win::ScopedHandle handle(
        ::OpenEvent(EVENT_ALL_ACCESS, TRUE, event_name.c_str()));
    PLOG_IF(ERROR, !handle.IsValid())
        << "Cannot create event '" << updater::kTestEventToSignal << "'";
    base::WaitableEvent event(std::move(handle));
    event.Signal();
  }

  VLOG(1) << "Process ended.";
  return 0;
}
