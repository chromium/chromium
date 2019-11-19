// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/always_on_top_window_killer_win.h"

#include <Windows.h>

#include <ios>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/test/base/process_lineage_win.h"
#include "chrome/test/base/save_desktop_snapshot_win.h"
#include "ui/display/win/screen_win.h"

namespace {

constexpr char kDialogFoundBeforeTest[] =
    "There is an always on top dialog on the desktop. This was most likely "
    "caused by a previous test and may cause this test to fail. Trying to "
    "close it;";

constexpr char kDialogFoundPostTest[] =
    "There is an always on top dialog on the desktop after this test timed "
    "out. This was most likely caused by this test and may cause future tests "
    "to fail, trying to close it;";

constexpr char kWindowFoundBeforeTest[] =
    "There is an always on top window on the desktop. This may have been "
    "caused by a previous test and may cause this test to fail;";

constexpr char kWindowFoundPostTest[] =
    "There is an always on top window on the desktop after this test timed "
    "out. This may have been caused by this test or a previous test and may "
    "cause flakes;";

// A window enumerator that searches for always-on-top windows. A snapshot of
// the screen is saved if any unexpected on-top windows are found.
class WindowEnumerator {
 public:
  // |run_type| influences which log message is used. |child_command_line|, only
  // specified when |run_type| is AFTER_TEST_TIMEOUT, is the command line of the
  // child process that timed out.
  WindowEnumerator(RunType run_type,
                   const base::CommandLine* child_command_line);
  void Run();

 private:
  // An EnumWindowsProc invoked by EnumWindows once for each window.
  static BOOL CALLBACK OnWindowProc(HWND hwnd, LPARAM l_param);

  // Returns true if |hwnd| is an always-on-top window.
  static bool IsTopmostWindow(HWND hwnd);

  // Returns the class name of |hwnd| or an empty string in case of error.
  static base::string16 GetWindowClass(HWND hwnd);

  // Returns true if |class_name| is the name of a system dialog.
  static bool IsSystemDialogClass(const base::string16& class_name);

  // Returns true if |class_name| is the name of a window owned by the Windows
  // shell.
  static bool IsShellWindowClass(const base::string16& class_name);

  // Main processing function run for each window.
  BOOL OnWindow(HWND hwnd);

  const base::FilePath output_dir_;
  const RunType run_type_;
  const base::CommandLine* const child_command_line_;
  bool saved_snapshot_;
  DISALLOW_COPY_AND_ASSIGN(WindowEnumerator);
};

WindowEnumerator::WindowEnumerator(RunType run_type,
                                   const base::CommandLine* child_command_line)
    : output_dir_(base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kSnapshotOutputDir)),
      run_type_(run_type),
      child_command_line_(child_command_line),
      saved_snapshot_(false) {}

void WindowEnumerator::Run() {
  if (run_type_ == RunType::AFTER_TEST_TIMEOUT && !output_dir_.empty()) {
    base::FilePath snapshot_file = SaveDesktopSnapshot(output_dir_);
    if (!snapshot_file.empty()) {
      saved_snapshot_ = true;

      std::wostringstream sstream;
      sstream << "Screen snapshot saved to file: \"" << snapshot_file.value()
              << "\" after timeout of test";
      if (child_command_line_) {
        sstream << " process with command line: \""
                << child_command_line_->GetCommandLineString() << "\".";
      } else {
        sstream << ".";
      }
      LOG(ERROR) << sstream.str();
    }
  }

  ::EnumWindows(&OnWindowProc, reinterpret_cast<LPARAM>(this));
}

// static
BOOL CALLBACK WindowEnumerator::OnWindowProc(HWND hwnd, LPARAM l_param) {
  return reinterpret_cast<WindowEnumerator*>(l_param)->OnWindow(hwnd);
}

// static
bool WindowEnumerator::IsTopmostWindow(HWND hwnd) {
  const LONG ex_styles = ::GetWindowLong(hwnd, GWL_EXSTYLE);
  return (ex_styles & WS_EX_TOPMOST) != 0;
}

// static
base::string16 WindowEnumerator::GetWindowClass(HWND hwnd) {
  wchar_t buffer[257];  // Max is 256.
  buffer[base::size(buffer) - 1] = L'\0';
  int name_len = ::GetClassName(hwnd, &buffer[0], base::size(buffer));
  if (name_len <= 0 || static_cast<size_t>(name_len) >= base::size(buffer))
    return base::string16();
  return base::string16(&buffer[0], name_len);
}

// static
bool WindowEnumerator::IsSystemDialogClass(const base::string16& class_name) {
  return class_name == L"#32770";
}

// static
bool WindowEnumerator::IsShellWindowClass(const base::string16& class_name) {
  // 'Button' is the start button, 'Shell_TrayWnd' the taskbar, and
  // 'Shell_SecondaryTrayWnd' is the taskbar on non-primary displays.
  return class_name == L"Button" || class_name == L"Shell_TrayWnd" ||
         class_name == L"Shell_SecondaryTrayWnd";
}

BOOL WindowEnumerator::OnWindow(HWND hwnd) {
  const BOOL kContinueIterating = TRUE;

  if (!::IsWindowVisible(hwnd) || ::IsIconic(hwnd) || !IsTopmostWindow(hwnd))
    return kContinueIterating;

  base::string16 class_name = GetWindowClass(hwnd);
  if (class_name.empty())
    return kContinueIterating;

  // Ignore specific windows owned by the shell.
  if (IsShellWindowClass(class_name))
    return kContinueIterating;

  // All other always-on-top windows may be problematic, but in theory tests
  // should not be creating an always on top window that outlives the test.
  // Prepare details of the command line of the test that timed out (if
  // provided), the process owning the window, and the location of a snapshot
  // taken of the screen.
  base::string16 details;
  if (LOG_IS_ON(ERROR)) {
    std::wostringstream sstream;

    if (!IsSystemDialogClass(class_name))
      sstream << " window class name: " << class_name << ";";

    if (child_command_line_) {
      sstream << " subprocess command line: \""
              << child_command_line_->GetCommandLineString() << "\";";
    }

    // Save a snapshot of the screen if one hasn't already been saved and an
    // output directory was specified.
    base::FilePath snapshot_file;
    if (!saved_snapshot_ && !output_dir_.empty()) {
      snapshot_file = SaveDesktopSnapshot(output_dir_);
      if (!snapshot_file.empty())
        saved_snapshot_ = true;
    }

    DWORD process_id = 0;
    GetWindowThreadProcessId(hwnd, &process_id);
    ProcessLineage lineage = ProcessLineage::Create(process_id);
    if (!lineage.IsEmpty())
      sstream << " owning process lineage: " << lineage.ToString() << ";";

    if (!snapshot_file.empty()) {
      sstream << " screen snapshot saved to file: \"" << snapshot_file.value()
              << "\";";
    }

    details = sstream.str();
  }

  // System dialogs may be present if a child process triggers an assert(), for
  // example.
  if (IsSystemDialogClass(class_name)) {
    LOG(ERROR) << (run_type_ == RunType::BEFORE_SHARD ? kDialogFoundBeforeTest
                                                      : kDialogFoundPostTest)
               << details;
    // We don't own the dialog, so we can't destroy it. CloseWindow()
    // results in iconifying the window. An alternative may be to focus it,
    // then send return and wait for close. As we reboot machines running
    // interactive ui tests at least every 12 hours we're going with the
    // simple for now.
    CloseWindow(hwnd);
  } else {
    LOG(ERROR) << (run_type_ == RunType::BEFORE_SHARD ? kWindowFoundBeforeTest
                                                      : kWindowFoundPostTest)
               << details;
    // Try to strip the style and iconify the window.
    if (::SetWindowLongPtr(
            hwnd, GWL_EXSTYLE,
            ::GetWindowLong(hwnd, GWL_EXSTYLE) & ~WS_EX_TOPMOST)) {
      LOG(ERROR) << "Stripped WS_EX_TOPMOST.";
    } else {
      PLOG(ERROR) << "Failed to strip WS_EX_TOPMOST";
    }
    if (::ShowWindow(hwnd, SW_FORCEMINIMIZE))
      LOG(ERROR) << "Minimized window.";
    else
      PLOG(ERROR) << "Failed to minimize window";
  }

  return kContinueIterating;
}

}  // namespace

void KillAlwaysOnTopWindows(RunType run_type,
                            const base::CommandLine* child_command_line) {
  WindowEnumerator(run_type, child_command_line).Run();
}
