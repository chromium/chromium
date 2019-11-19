// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/interactive_test_utils.h"

#include <Psapi.h>

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/stl_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/test/base/interactive_test_utils_aura.h"
#include "chrome/test/base/process_lineage_win.h"
#include "chrome/test/base/save_desktop_snapshot_win.h"
#include "chrome/test/base/window_contents_as_string_win.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/win/foreground_helper.h"
#include "ui/views/focus/focus_manager.h"

namespace ui_test_utils {

void HideNativeWindow(gfx::NativeWindow window) {
#if defined(OS_CHROMEOS)
  HideNativeWindowAura(window);
#else
  HWND hwnd = window->GetHost()->GetAcceleratedWidget();
  ::ShowWindow(hwnd, SW_HIDE);
#endif  // OS_CHROMEOS
}

bool ShowAndFocusNativeWindow(gfx::NativeWindow window) {
#if defined(OS_CHROMEOS)
  ShowAndFocusNativeWindowAura(window);
#endif  // OS_CHROMEOS
  window->Show();
  // Always make sure the window hosting ash is visible and focused.
  HWND hwnd = window->GetHost()->GetAcceleratedWidget();

  ::ShowWindow(hwnd, SW_SHOW);

  int attempts_left = 5;
  bool have_snapshot = false;
  while (true) {
    if (::GetForegroundWindow() == hwnd)
      return true;

    VLOG(1) << "Forcefully refocusing front window";
    ui::ForegroundHelper::SetForeground(hwnd);

    // ShowWindow does not necessarily activate the window. In particular if a
    // window from another app is the foreground window then the request to
    // activate the window fails. See SetForegroundWindow for details.
    HWND foreground_window = ::GetForegroundWindow();
    if (foreground_window == hwnd)
      return true;

    // Emit some diagnostic information about the foreground window and its
    // owning process.
    wchar_t window_title[256];
    GetWindowText(foreground_window, window_title, base::size(window_title));

    base::string16 lineage_str;
    base::string16 window_contents;
    DWORD foreground_process_id = 0;
    if (foreground_window) {
      GetWindowThreadProcessId(foreground_window, &foreground_process_id);
      ProcessLineage lineage = ProcessLineage::Create(foreground_process_id);
      if (!lineage.IsEmpty()) {
        lineage_str = STRING16_LITERAL(", process lineage: ");
        lineage_str.append(lineage.ToString());
      }

      window_contents = WindowContentsAsString(foreground_window);
    }
    LOG(ERROR) << "ShowAndFocusNativeWindow found a foreground window: "
               << foreground_window << ", title: " << window_title
               << lineage_str << ", contents:" << std::endl
               << window_contents;

    // Take a snapshot of the screen.
    const base::FilePath output_dir =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            kSnapshotOutputDir);
    if (!have_snapshot && !output_dir.empty()) {
      base::FilePath snapshot_file = SaveDesktopSnapshot(output_dir);
      if (!snapshot_file.empty()) {
        have_snapshot = true;
        LOG(ERROR) << "Screenshot saved to file: \"" << snapshot_file.value()
                   << "\"";
      }
    }

    if (!attempts_left--) {
      LOG(ERROR) << "ShowAndFocusNativeWindow failed after too many attempts.";
      break;
    }

    // Give up if there is no foreground window or if it's mysteriously from
    // this process.
    if (!foreground_window) {
      LOG(ERROR) << "ShowAndFocusNativeWindow failed to focus any window.";
      break;
    }

    if (foreground_process_id == base::GetCurrentProcId()) {
      LOG(ERROR) << "ShowAndFocusNativeWindow failed because another window in"
                    " the test process will not give up focus.";
      break;
    }

    // Attempt to close the offending window.
    ::PostMessageW(foreground_window, WM_CLOSE, 0, 0);
    // Poll to wait for the window to be destroyed. While it is possible to
    // avoid polling via use of UI Automation to observe the closing of the
    // window, the code to do so is non-trivial and requires use of another
    // thread in the MTA.
    base::RunLoop run_loop;
    base::RepeatingTimer timer(
        FROM_HERE, TestTimeouts::tiny_timeout(),
        base::BindRepeating(
            [](HWND foreground_window,
               const base::RepeatingClosure& quit_closure, int* polls) {
              if (!*polls-- || ::GetForegroundWindow() != foreground_window)
                quit_closure.Run();
            },
            foreground_window, run_loop.QuitClosure(),
            base::Owned(std::make_unique<int>(
                TestTimeouts::action_timeout().InMicroseconds() /
                TestTimeouts::tiny_timeout().InMicroseconds()))));
    timer.Reset();
    run_loop.Run();
    if (::GetForegroundWindow() == foreground_window) {
      LOG(ERROR) << "ShowAndFocusNativeWindow timed out closing the "
                    "foreground window.";
      break;
    }
    // Otherwise, loop around and try focusing the desired window again.
  }

  return false;
}

}  // namespace ui_test_utils
