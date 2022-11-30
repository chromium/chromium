// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_SETUP_SINGLETON_H_
#define CHROME_INSTALLER_SETUP_SETUP_SINGLETON_H_

#include <windows.h>

#include <memory>

#include "base/synchronization/waitable_event.h"
#include "base/win/scoped_handle.h"

namespace base {
class CommandLine;
class TimeDelta;
}  // namespace base

namespace installer {

class InstallationState;
class InstallerState;
class InitialPreferences;

// Any modification to a Chrome installation should be done within the scope of
// a SetupSingleton. There can be only one active SetupSingleton per Chrome
// installation at a time.
class SetupSingleton {
 public:
  // Returns a SetupSingleton which, throughout its lifetime, gives the current
  // process the exclusive right to modify the Chrome installation described by
  // |installer_state| (installation directory and associated registry keys).
  // May block. |original_state| and |installer_state| are updated using
  // |command_line| and |initial_preferences| to reflect the new state of the
  // installation after acquisition. Returns nullptr on failure.
  static std::unique_ptr<SetupSingleton> Acquire(
      const base::CommandLine& command_line,
      const InitialPreferences& initial_preferences,
      InstallationState* original_state,
      InstallerState* installer_state);

  SetupSingleton(const SetupSingleton&) = delete;
  SetupSingleton& operator=(const SetupSingleton&) = delete;

  // Releases the exclusive right to modify the Chrome installation.
  ~SetupSingleton();

  // Waits until |max_time| has passed or another process tries to acquire a
  // SetupSingleton for the same Chrome installation. In the latter case, the
  // method returns true and this SetupSingleton should be released as soon as
  // possible to unblock the other process.
  bool WaitForInterrupt(const base::TimeDelta& max_time) const;

 private:
  class ScopedHoldMutex {
   public:
    ScopedHoldMutex();

    ScopedHoldMutex(const ScopedHoldMutex&) = delete;
    ScopedHoldMutex& operator=(const ScopedHoldMutex&) = delete;

    ~ScopedHoldMutex();

    // Waits up to a certain amount of time to acquire |mutex|. Returns true on
    // success. |mutex| will be released in the destructor.
    bool Acquire(HANDLE mutex);

   private:
    HANDLE mutex_ = INVALID_HANDLE_VALUE;
  };

  SetupSingleton(base::win::ScopedHandle setup_mutex,
                 base::win::ScopedHandle exit_event);

  // A mutex that must be held to modify the Chrome installation directory.
  base::win::ScopedHandle setup_mutex_;

  // Holds |setup_mutex_| throughout the lifetime of this SetupSingleton.
  ScopedHoldMutex scoped_hold_setup_mutex_;

  // An event signaled to ask the owner of |setup_mutex_| to release it as soon
  // as possible.
  mutable base::WaitableEvent exit_event_;
};

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_SETUP_SINGLETON_H_
