// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/always_on_top_window_killer_win.h"

#include <Windows.h>

#include <ios>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/win/window_enumerator.h"
#include "chrome/test/base/process_lineage_win.h"
#include "chrome/test/base/save_desktop_snapshot.h"
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

}  // namespace

void KillAlwaysOnTopWindows(RunType run_type,
                            const base::CommandLine* child_command_line) {
  const base::FilePath output_dir =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kSnapshotOutputDir);
  bool saved_snapshot = false;
  if (run_type == RunType::AFTER_TEST_TIMEOUT && !output_dir.empty()) {
    base::FilePath snapshot_file = SaveDesktopSnapshot(output_dir);
    if (!snapshot_file.empty()) {
      saved_snapshot = true;

      std::wostringstream sstream;
      sstream << "Screen snapshot saved to file: \"" << snapshot_file.value()
              << "\" after timeout of test";
      if (child_command_line) {
        sstream << " process with command line: \""
                << child_command_line->GetCommandLineString() << "\".";
      } else {
        sstream << ".";
      }
      LOG(ERROR) << sstream.str();
    }
  }

  base::win::EnumerateChildWindows(
      ::GetDesktopWindow(), base::BindLambdaForTesting([&](HWND hwnd) {
        const bool kContinueIterating = false;

        if (!::IsWindowVisible(hwnd) || ::IsIconic(hwnd) ||
            !base::win::IsTopmostWindow(hwnd)) {
          return kContinueIterating;
        }

        const std::wstring class_name = base::win::GetWindowClass(hwnd);
        if (class_name.empty()) {
          return kContinueIterating;
        }

        // Ignore specific windows owned by the shell.
        if (base::win::IsShellWindow(hwnd)) {
          return kContinueIterating;
        }

        // All other always-on-top windows may be problematic, but in theory
        // tests should not be creating an always on top window that outlives
        // the test. Prepare details of the command line of the test that timed
        // out (if provided), the process owning the window, and the location of
        // a snapshot taken of the screen.
        std::wstring details;
        if (LOG_IS_ON(ERROR)) {
          std::wostringstream sstream;

          if (!base::win::IsSystemDialog(hwnd)) {
            sstream << " window class name: " << class_name << ";";
          }

          if (child_command_line) {
            sstream << " subprocess command line: \""
                    << child_command_line->GetCommandLineString() << "\";";
          }

          // Save a snapshot of the screen if one hasn't already been saved and
          // an output directory was specified.
          base::FilePath snapshot_file;
          if (!saved_snapshot && !output_dir.empty()) {
            snapshot_file = SaveDesktopSnapshot(output_dir);
            if (!snapshot_file.empty()) {
              saved_snapshot = true;
            }
          }

          DWORD process_id = 0;
          GetWindowThreadProcessId(hwnd, &process_id);
          ProcessLineage lineage = ProcessLineage::Create(process_id);
          if (!lineage.IsEmpty()) {
            sstream << " owning process lineage: " << lineage.ToString() << ";";
          }

          if (!snapshot_file.empty()) {
            sstream << " screen snapshot saved to file: \""
                    << snapshot_file.value() << "\";";
          }

          details = sstream.str();
        }

        // System dialogs may be present if a child process triggers an
        // assert(), for example.
        if (base::win::IsSystemDialog(hwnd)) {
          LOG(ERROR) << (run_type == RunType::BEFORE_SHARD
                             ? kDialogFoundBeforeTest
                             : kDialogFoundPostTest)
                     << details;
          // We don't own the dialog, so we can't destroy it. CloseWindow()
          // results in iconifying the window. An alternative may be to focus
          // it, then send return and wait for close. As we reboot machines
          // running interactive ui tests at least every 12 hours we're going
          // with the simple for now.
          CloseWindow(hwnd);
        } else {
          LOG(ERROR) << (run_type == RunType::BEFORE_SHARD
                             ? kWindowFoundBeforeTest
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
          if (::ShowWindow(hwnd, SW_FORCEMINIMIZE)) {
            LOG(ERROR) << "Minimized window.";
          } else {
            PLOG(ERROR) << "Failed to minimize window";
          }
        }

        return kContinueIterating;
      }));
}
