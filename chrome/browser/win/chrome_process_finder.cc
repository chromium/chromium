// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/chrome_process_finder.h"

#include <windows.h>

#include <shellapi.h>

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/base_tracing.h"
#include "base/win/message_window.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"

namespace {

uint32_t g_timeout_in_milliseconds = 20 * 1000;

}  // namespace

namespace chrome {

HWND FindRunningChromeWindow(const base::FilePath& user_data_dir) {
  TRACE_EVENT0("startup", "FindRunningChromeWindow");
  return base::win::MessageWindow::FindWindow(user_data_dir.value());
}

NotifyChromeResult AttemptToNotifyRunningChrome(HWND remote_window) {
  TRACE_EVENT0("startup", "AttemptToNotifyRunningChrome");

  DCHECK(remote_window);
  DWORD process_id = 0;
  DWORD thread_id = GetWindowThreadProcessId(remote_window, &process_id);
  if (!thread_id || !process_id) {
    TRACE_EVENT0(
        "startup",
        "AttemptToNotifyRunningChrome:GetWindowThreadProcessId failed");
    return NOTIFY_FAILED;
  }

  base::FilePath cur_dir;
  if (!base::GetCurrentDirectory(&cur_dir)) {
    TRACE_EVENT_INSTANT(
        "startup", "AttemptToNotifyRunningChrome:GetCurrentDirectory failed");
    return NOTIFY_FAILED;
  }
  base::CommandLine new_command_line(*base::CommandLine::ForCurrentProcess());
  // If this process was launched from a shortcut, add the shortcut path to
  // the command line, so the process we rendezvous with can record the
  // launch mode correctly.
  STARTUPINFOW si = {sizeof(si)};
  ::GetStartupInfoW(&si);
  if (si.dwFlags & STARTF_TITLEISLINKNAME)
    new_command_line.AppendSwitchNative(switches::kSourceShortcut, si.lpTitle);

  // Send the command line to the remote chrome window.
  // Format is "START\0<<<current directory>>>\0<<<commandline>>>".
  std::wstring to_send = base::StrCat(
      {std::wstring_view{L"START\0", 6}, cur_dir.value(),
       std::wstring_view{L"\0", 1}, new_command_line.GetCommandLineString(),
       std::wstring_view{L"\0", 1}});

  // Allow the current running browser window to make itself the foreground
  // window (otherwise it will just flash in the taskbar).
  ::AllowSetForegroundWindow(process_id);

  COPYDATASTRUCT cds;
  cds.dwData = 0;
  cds.cbData = static_cast<DWORD>((to_send.length() + 1) * sizeof(wchar_t));
  cds.lpData = const_cast<wchar_t*>(to_send.c_str());
  DWORD_PTR result = 0;
  {
    TRACE_EVENT0("startup", "AttemptToNotifyRunningChrome:SendMessage");
    if (::SendMessageTimeout(remote_window, WM_COPYDATA, NULL,
                             reinterpret_cast<LPARAM>(&cds), SMTO_ABORTIFHUNG,
                             g_timeout_in_milliseconds, &result)) {
      return result ? NOTIFY_SUCCESS : NOTIFY_FAILED;
    }
  }

  // If SendMessageTimeout failed to send message consider this as
  // NOTIFY_FAILED. Timeout can be represented as either ERROR_TIMEOUT or 0...
  // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendmessagetimeoutw
  // Anecdotally which error code comes out seems to depend on whether this
  // process had non-empty data to deliver via |to_send| or not.
  const auto error = ::GetLastError();
  const bool timed_out = (error == ERROR_TIMEOUT || error == 0);
  if (!timed_out) {
    TRACE_EVENT_INSTANT("startup",
                        "AttemptToNotifyRunningChrome:Error SendFailed");
    return NOTIFY_FAILED;
  }

  // It is possible that the process owning this window may have died by now.
  if (!::IsWindow(remote_window)) {
    TRACE_EVENT_INSTANT("startup",
                        "AttemptToNotifyRunningChrome:Error RemoteDied");
    return NOTIFY_FAILED;
  }

  // If the window couldn't be notified but still exists, assume it is hung.
  TRACE_EVENT_INSTANT("startup",
                      "AttemptToNotifyRunningChrome:Error RemoteHung");
  return NOTIFY_WINDOW_HUNG;
}

base::TimeDelta SetNotificationTimeoutForTesting(base::TimeDelta new_timeout) {
  base::TimeDelta old_timeout = base::Milliseconds(g_timeout_in_milliseconds);
  g_timeout_in_milliseconds =
      base::checked_cast<uint32_t>(new_timeout.InMilliseconds());
  return old_timeout;
}

}  // namespace chrome
