// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_install_util_win.h"

#include <windows.h>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/updater/app/app_install_progress.h"
#include "chrome/updater/util/win_util.h"

namespace updater {

namespace {

HRESULT LaunchCmdLine(const AppCompletionInfo& app_info) {
  if (app_info.post_install_launch_command_line.empty()) {
    return S_OK;
  }

  if (app_info.completion_code !=
          CompletionCodes::COMPLETION_CODE_LAUNCH_COMMAND &&
      app_info.completion_code !=
          CompletionCodes::COMPLETION_CODE_EXIT_SILENTLY_ON_LAUNCH_COMMAND) {
    return S_OK;
  }

  CHECK(SUCCEEDED(app_info.error_code));
  CHECK(!app_info.is_noupdate);

  return RunDeElevatedCmdLine(
      base::UTF8ToWide(app_info.post_install_launch_command_line));
}

}  // namespace

bool LaunchCmdLines(const ObserverCompletionInfo& info) {
  bool result = true;

  for (size_t i = 0; i < info.apps_info.size(); ++i) {
    const AppCompletionInfo& app_info = info.apps_info[i];
    if (FAILED(app_info.error_code)) {
      continue;
    }
    result &= SUCCEEDED(LaunchCmdLine(app_info));
  }

  return result;
}

}  // namespace updater
