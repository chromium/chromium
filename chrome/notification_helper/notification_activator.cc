// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/notification_helper/notification_activator.h"

#include <windows.h>

#include <shellapi.h>

#include <string>

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process.h"
#include "base/win/windows_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/notification_helper/notification_helper_util.h"
#include "chrome/notification_helper/trace_util.h"

namespace {

// The response entered by the user while interacting with the toast.
const wchar_t kUserResponse[] = L"userResponse";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class NotificationActivatorPrimaryStatus {
  kSuccess = 0,
  kChromeExeMissing = 1,
  kShellExecuteFailed = 2,
  kMaxValue = kShellExecuteFailed,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class NotificationActivatorSecondaryStatus {
  kSuccess = 0,
  kLaunchIdEmpty = 1 << 0,
  kAllowSetForegroundWindowFailed = 1 << 1,
  kProcessHandleMissing = 1 << 2,
  kScenarioCount = 1 << 3,
  kMaxValue = kScenarioCount,
};

void LogNotificationActivatorPrimaryStatus(
    NotificationActivatorPrimaryStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "Notifications.NotificationHelper.NotificationActivatorPrimaryStatus",
      status);
}

void LogNotificationActivatorSecondaryStatus(
    NotificationActivatorSecondaryStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "Notifications.NotificationHelper.NotificationActivatorSecondaryStatus",
      status);
}

}  // namespace

namespace notification_helper {

NotificationActivator::~NotificationActivator() = default;

// Handles toast activation outside of the browser process lifecycle by
// launching chrome.exe with --notification-launch-id. This new process may
// rendezvous to an existing browser process or become a new one, as
// appropriate.
//
// When this method is called, there are three possibilities depending on the
// running state of Chrome.
// 1) NOT_RUNNING: Chrome is not running.
// 2) NEW_INSTANCE: Chrome is running, but it's NOT the same instance that sent
//    the toast.
// 3) SAME_INSTANCE : Chrome is running, and it _is_ the same instance that sent
//    the toast.
//
// Chrome could attach an activation event handler to the toast so that Windows
// can call it directly to handle the activation. However, Windows makes this
// function call only in case SAME_INSTANCE. For the other two cases, Chrome
// needs to handle the activation on its own. Since there is no way to
// differentiate cases SAME_INSTANCE and NEW_INSTANCE in this
// notification_helper process, Chrome doesn't attach an activation event
// handler to the toast and handles all three cases through the command line.
HRESULT NotificationActivator::Activate(
    LPCWSTR app_user_model_id,
    LPCWSTR invoked_args,
    const NOTIFICATION_USER_INPUT_DATA* data,
    ULONG count) {
  base::FilePath chrome_exe_path = GetChromeExePath();
  if (chrome_exe_path.empty()) {
    Trace(L"Failed to get chrome exe path\n");
    LogNotificationActivatorPrimaryStatus(
        NotificationActivatorPrimaryStatus::kChromeExeMissing);
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  int secondary_status =
      static_cast<int>(NotificationActivatorSecondaryStatus::kSuccess);

  // |invoked_args| contains the launch ID string encoded by Chrome. Chrome adds
  // it to the launch argument of the toast and gets it back via |invoked_args|.
  // Chrome needs the data to be able to look up the notification on its end.
  //
  // When the user clicks the Chrome app title rather than the notifications in
  // the Action Center, an empty launch id string is generated. It is preferable
  // to launch Chrome with this empty launch id in this scenario, which results
  // in displaying a NTP.
  if (invoked_args == nullptr || invoked_args[0] == 0) {
    secondary_status |=
        static_cast<int>(NotificationActivatorSecondaryStatus::kLaunchIdEmpty);
  }
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchNative(switches::kNotificationLaunchId,
                                  invoked_args);

  // Check to see if a user response (inline reply) is also supplied.
  for (ULONG i = 0; i < count; ++i) {
    if (lstrcmpW(kUserResponse, data[i].Key) == 0) {
      command_line.AppendSwitchNative(switches::kNotificationInlineReply,
                                      data[i].Value);
      break;
    }
  }

  std::wstring params(command_line.GetCommandLineString());

  SHELLEXECUTEINFO info;
  memset(&info, 0, sizeof(info));
  info.cbSize = sizeof(info);
  info.fMask =
      SEE_MASK_NOASYNC | SEE_MASK_FLAG_LOG_USAGE | SEE_MASK_NOCLOSEPROCESS;
  info.lpFile = chrome_exe_path.value().c_str();
  info.lpParameters = params.c_str();
  info.nShow = SW_SHOWNORMAL;

  if (!::ShellExecuteEx(&info)) {
    DWORD error_code = ::GetLastError();
    Trace(L"Unable to launch Chrome.exe; error: 0x%08X\n", error_code);
    LogNotificationActivatorPrimaryStatus(
        NotificationActivatorPrimaryStatus::kShellExecuteFailed);
    return HRESULT_FROM_WIN32(error_code);
  }

  if (info.hProcess != nullptr) {
    base::Process process(info.hProcess);
    DWORD pid = ::GetProcessId(process.Handle());

    // Despite the fact that the Windows notification center grants the helper
    // permission to set the foreground window, the helper fails to pass the
    // baton to Chrome at an alarming rate; see https://crbug.com/837796.
    // Sending generic down/up key events seems to fix it.
    INPUT keyboard_inputs[2] = {};

    keyboard_inputs[0].type = INPUT_KEYBOARD;
    keyboard_inputs[0].ki.dwFlags = 0;  // Key press.

    keyboard_inputs[1] = keyboard_inputs[0];
    keyboard_inputs[1].ki.dwFlags |= KEYEVENTF_KEYUP;  // key release.

    ::SendInput(2, keyboard_inputs, sizeof(keyboard_inputs[0]));

    if (!::AllowSetForegroundWindow(pid)) {
#if !defined(NDEBUG)
      DWORD error_code = ::GetLastError();
      Trace(L"Unable to forward activation privilege; error: 0x%08X\n",
            error_code);
#endif
      // The lack of ability to set the window to foreground is not reason
      // enough to fail the activation call. The user will see the Chrome icon
      // flash in the task bar if this happens, which is a graceful failure.
      secondary_status |=
          static_cast<int>(NotificationActivatorSecondaryStatus::
                               kAllowSetForegroundWindowFailed);
    }
  } else {
    secondary_status |= static_cast<int>(
        NotificationActivatorSecondaryStatus::kProcessHandleMissing);
  }

  LogNotificationActivatorPrimaryStatus(
      NotificationActivatorPrimaryStatus::kSuccess);

  LogNotificationActivatorSecondaryStatus(
      static_cast<NotificationActivatorSecondaryStatus>(secondary_status));

  return S_OK;
}

}  // namespace notification_helper
