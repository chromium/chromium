// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "chrome/common/chrome_switches.h"

namespace {

constexpr base::FilePath::CharType kChromeExecutable[] =
    FILE_PATH_LITERAL("chrome.exe");

constexpr base::FilePath::CharType kChromeProxyExecutable[] =
    FILE_PATH_LITERAL("chrome_proxy.exe");

}  // namespace

// This binary is a workaround for Windows 10 start menu pinning icon bug:
// https://crbug.com/732357.
//
// When a shortcut is pinned in the Windows 10 start menu Windows will follow
// the shortcut, find the target executable, look for a <target>.manifest file
// in the same directory and use the icon specified in there for the start menu
// pin. Because bookmark app shortcuts are shortcuts to Chrome (plus a few
// command line parameters) Windows ends up using the Chrome icon specified in
// chrome.VisualElementsManifest.xml instead of the site's icon stored inside
// the shortcut.
//
// The chrome_proxy.exe binary workaround "fixes" this by having bookmark app
// shortcuts target chrome_proxy.exe instead of chrome.exe such that Windows
// won't find a manifest and falls back to using the shortcut's icons as
// originally intended.
int WINAPI wWinMain(HINSTANCE instance,
                    HINSTANCE prev_instance,
                    wchar_t* /*command_line*/,
                    int show_command) {
  base::CommandLine::Init(0, nullptr);

  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(logging_settings);

  base::FilePath chrome_dir;
  CHECK(base::PathService::Get(base::DIR_EXE, &chrome_dir));
  base::CommandLine chrome_command_line(chrome_dir.Append(kChromeExecutable));

  // Forward all command line arguments.
  const std::vector<std::wstring>& argv =
      base::CommandLine::ForCurrentProcess()->argv();
  // The first one is always the current executable path.
  CHECK(argv.size() > 0);
  CHECK_EQ(base::FilePath(argv[0]).BaseName().value(), kChromeProxyExecutable);
  for (size_t i = 1; i < argv.size(); ++i)
    chrome_command_line.AppendArgNative(argv[i]);

  // Pass to Chrome the path of the shortcut, if any, that launched
  // chrome_proxy.exe. This is used to record LaunchMode metrics.
  STARTUPINFOW si = {sizeof(si)};
  ::GetStartupInfoW(&si);
  if (si.dwFlags & STARTF_TITLEISLINKNAME) {
    chrome_command_line.AppendSwitchNative(switches::kSourceShortcut,
                                           si.lpTitle);
  }

  base::LaunchOptions launch_options;
  launch_options.current_directory = chrome_dir;
  launch_options.grant_foreground_privilege = true;
  CHECK(base::LaunchProcess(chrome_command_line, launch_options).IsValid());

  return 0;
}
