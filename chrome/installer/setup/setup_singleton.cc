// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/setup_singleton.h"

#include <functional>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/util/installation_state.h"

namespace installer {

std::unique_ptr<SetupSingleton> SetupSingleton::Acquire(
    const base::CommandLine& command_line,
    const InitialPreferences& initial_preferences,
    InstallationState* original_state,
    InstallerState* installer_state) {
  DCHECK(original_state);
  DCHECK(installer_state);

  const std::wstring sync_primitive_name_suffix(
      base::NumberToWString(std::hash<base::FilePath::StringType>()(
          installer_state->target_path().value())));

  base::win::ScopedHandle setup_mutex(::CreateMutex(
      nullptr, FALSE,
      (L"Global\\ChromeSetupMutex_" + sync_primitive_name_suffix).c_str()));
  if (!setup_mutex.IsValid()) {
    // UMA data indicates that this happens 0.03 % of the time.
    return nullptr;
  }

  base::win::ScopedHandle exit_event(::CreateEvent(
      nullptr, TRUE, FALSE,
      (L"Global\\ChromeSetupExitEvent_" + sync_primitive_name_suffix).c_str()));
  if (!exit_event.IsValid()) {
    // UMA data indicates that this happens < 0.01 % of the time.
    return nullptr;
  }

  auto setup_singleton = base::WrapUnique(
      new SetupSingleton(std::move(setup_mutex), std::move(exit_event)));

  {
    // Acquire a mutex to ensure that a single call to SetupSingleton::Acquire()
    // signals |exit_event_| and waits for |setup_mutex_| to be released at a
    // time.
    base::win::ScopedHandle exit_event_mutex(::CreateMutex(
        nullptr, FALSE,
        (L"Global\\ChromeSetupExitEventMutex_" + sync_primitive_name_suffix)
            .c_str()));
    if (!exit_event_mutex.IsValid()) {
      // UMA data indicates that this happens < 0.01 % of the time.
      return nullptr;
    }

    ScopedHoldMutex scoped_hold_exit_event_mutex;
    if (!scoped_hold_exit_event_mutex.Acquire(exit_event_mutex.Get())) {
      // UMA data indicates that this happens < 0.01 % of the time.
      return nullptr;
    }

    // Signal |exit_event_|. This causes any call to WaitForInterrupt() on a
    // SetupSingleton bound to the same Chrome installation to return
    // immediately.
    setup_singleton->exit_event_.Signal();

    // Acquire |setup_mutex_|.
    if (!setup_singleton->scoped_hold_setup_mutex_.Acquire(
            setup_singleton->setup_mutex_.Get())) {
      // UMA data indicates that this happens 0.84 % of the time.
      return nullptr;
    }
    setup_singleton->exit_event_.Reset();
  }

  // Update |original_state| and |installer_state|.
  original_state->Initialize();
  installer_state->Initialize(command_line, initial_preferences,
                              *original_state);

  // UMA data indicates that this method succeeds > 99% of the time.
  return setup_singleton;
}

SetupSingleton::~SetupSingleton() = default;

bool SetupSingleton::WaitForInterrupt(const base::TimeDelta& max_time) const {
  const bool exit_event_signaled = exit_event_.TimedWait(max_time);
  return exit_event_signaled;
}

SetupSingleton::ScopedHoldMutex::ScopedHoldMutex() = default;

SetupSingleton::ScopedHoldMutex::~ScopedHoldMutex() {
  if (mutex_ != INVALID_HANDLE_VALUE)
    ::ReleaseMutex(mutex_);
}

bool SetupSingleton::ScopedHoldMutex::Acquire(HANDLE mutex) {
  DCHECK_NE(INVALID_HANDLE_VALUE, mutex);
  DCHECK_EQ(INVALID_HANDLE_VALUE, mutex_);

  const DWORD wait_return_value = ::WaitForSingleObject(
      mutex, static_cast<DWORD>(base::Seconds(5).InMilliseconds()));
  if (wait_return_value == WAIT_ABANDONED ||
      wait_return_value == WAIT_OBJECT_0) {
    mutex_ = mutex;
    return true;
  }

  DPCHECK(wait_return_value != WAIT_FAILED);
  return false;
}

SetupSingleton::SetupSingleton(base::win::ScopedHandle setup_mutex,
                               base::win::ScopedHandle exit_event)
    : setup_mutex_(std::move(setup_mutex)), exit_event_(std::move(exit_event)) {
  DCHECK(setup_mutex_.IsValid());
}

}  // namespace installer
