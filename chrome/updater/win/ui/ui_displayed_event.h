// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_UI_DISPLAYED_EVENT_H_
#define CHROME_UPDATER_WIN_UI_UI_DISPLAYED_EVENT_H_

#include <windows.h>

#include "chrome/updater/win/scoped_handle.h"

namespace updater {
namespace ui {

// Manages an event used to synchronize the state of the UI between processes.
// In this case, one process presents the splash screen UI, forks another
// process, and waits for the new process to display UI before the splash
// screen goes away.
class UIDisplayedEventManager {
 public:
  // Signals the event. Creates the event if the event does not exist.
  static void SignalEvent(bool is_machine);

 private:
  // Creates the event and sets its name in an environment variable.
  static HRESULT CreateEvent(bool is_machine);

  // Gets the event from the name in the environment variable. The caller does
  // not own the event handle and must not close it. The handle is own by
  // the process and the handle is closed when the process exits.
  static HRESULT GetEvent(bool is_machine, HANDLE* ui_displayed_event);

  // Returns true if the event handle has been initialized in this process.
  static bool IsEventHandleInitialized();

  // A single instance of the handle in this process.
  //
  // TODO(sorin): fix the static user-defined type instance. It may go away
  // with the rest of the class, https://crbug.com/1016986.
  static ScopedKernelHANDLE ui_displayed_event_;
};

}  // namespace ui
}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UI_UI_DISPLAYED_EVENT_H_
