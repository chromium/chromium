// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_install_util_win.h"

#include <shellapi.h>
#include <windows.h>

#include <string>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_localalloc.h"
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

  if (!IsElevatedWithUACOn()) {
    auto process = base::LaunchProcess(
        base::UTF8ToWide(app_info.post_install_launch_command_line), {});
    return process.IsValid() ? S_OK : HRESULTFromLastError();
  }

  std::wstring command_format =
      base::UTF8ToWide(app_info.post_install_launch_command_line);
  int num_args = 0;
  base::win::ScopedLocalAllocTyped<wchar_t*> argv(
      ::CommandLineToArgvW(&command_format[0], &num_args));
  if (!argv || num_args < 1) {
    LOG(ERROR) << __func__ << "!argv || num_args < 1: " << num_args;
    return E_INVALIDARG;
  }

  return RunDeElevated(
      argv.get()[0],
      base::JoinString(
          [&]() -> std::vector<std::wstring> {
            if (num_args <= 1) {
              return {};
            }

            std::vector<std::wstring> parameters;
            base::ranges::for_each(
                argv.get() + 1, argv.get() + num_args,
                [&](const auto& parameter) {
                  parameters.push_back(
                      base::CommandLine::QuoteForCommandLineToArgvW(parameter));
                });
            return parameters;
          }(),
          L" "));
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
