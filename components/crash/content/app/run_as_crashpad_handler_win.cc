// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/app/run_as_crashpad_handler_win.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/memory.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "components/browser_watcher/stability_report_user_stream_data_source.h"
#include "components/gwp_asan/buildflags/buildflags.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"
#include "third_party/crashpad/crashpad/client/simple_string_dictionary.h"
#include "third_party/crashpad/crashpad/handler/handler_main.h"
#include "third_party/crashpad/crashpad/handler/user_stream_data_source.h"

#if BUILDFLAG(ENABLE_GWP_ASAN)
#include "components/gwp_asan/crash_handler/crash_handler.h"  // nogncheck
#endif

namespace crash_reporter {

int RunAsCrashpadHandler(const base::CommandLine& command_line,
                         const base::FilePath& user_data_dir,
                         const char* process_type_switch,
                         const char* user_data_dir_switch) {
  // Make sure this process terminates on OOM in the same mode as other Chrome
  // processes.
  base::EnableTerminationOnOutOfMemory();

  base::PlatformThread::SetName("CrashpadMainThread");

  // If the handler is started with --monitor-self, it'll need a ptype
  // annotation set. It'll normally set one itself by being invoked with
  // --monitor-self-annotation=ptype=crashpad-handler, but that leaves a window
  // during self-monitoring initialization when the ptype is not set at all, so
  // provide one here.
  const std::string process_type =
      command_line.GetSwitchValueASCII(process_type_switch);
  if (!process_type.empty()) {
    crashpad::SimpleStringDictionary* annotations =
        new crashpad::SimpleStringDictionary();
    annotations->SetKeyValue("ptype", process_type.c_str());
    crashpad::CrashpadInfo* crashpad_info =
        crashpad::CrashpadInfo::GetCrashpadInfo();
    DCHECK(!crashpad_info->simple_annotations());
    crashpad_info->set_simple_annotations(annotations);
  }

  std::vector<base::string16> argv = command_line.argv();
  const base::string16 process_type_arg_prefix =
      base::string16(L"--") + base::UTF8ToUTF16(process_type_switch) + L"=";
  const base::string16 user_data_dir_arg_prefix =
      base::string16(L"--") + base::UTF8ToUTF16(user_data_dir_switch) + L"=";
  base::EraseIf(argv, [&process_type_arg_prefix,
                       &user_data_dir_arg_prefix](const base::string16& str) {
    return base::StartsWith(str, process_type_arg_prefix,
                            base::CompareCase::SENSITIVE) ||
           base::StartsWith(str, user_data_dir_arg_prefix,
                            base::CompareCase::SENSITIVE) ||
           (!str.empty() && str[0] == L'/');
  });

  std::unique_ptr<char* []> argv_as_utf8(new char*[argv.size() + 1]);
  std::vector<std::string> storage;
  storage.reserve(argv.size());
  for (size_t i = 0; i < argv.size(); ++i) {
    storage.push_back(base::UTF16ToUTF8(argv[i]));
    argv_as_utf8[i] = &storage[i][0];
  }
  argv_as_utf8[argv.size()] = nullptr;
  argv.clear();

  crashpad::UserStreamDataSources user_stream_data_sources;
  // Interpret an empty user data directory as a missing value.
  if (!user_data_dir.empty()) {
    // Register an extension to collect stability information. The extension
    // will be invoked for any registered process' crashes, but information only
    // exists for instrumented browser processes.
    user_stream_data_sources.push_back(
        std::make_unique<browser_watcher::StabilityReportUserStreamDataSource>(
            user_data_dir));
  }

#if BUILDFLAG(ENABLE_GWP_ASAN)
  user_stream_data_sources.push_back(
      std::make_unique<gwp_asan::UserStreamDataSource>());
#endif

  return crashpad::HandlerMain(static_cast<int>(storage.size()),
                               argv_as_utf8.get(), &user_stream_data_sources);
}

}  // namespace crash_reporter
