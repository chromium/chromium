// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_APP_RUN_AS_CRASHPAD_HANDLER_WIN_H_
#define COMPONENTS_CRASH_CORE_APP_RUN_AS_CRASHPAD_HANDLER_WIN_H_

namespace base {
class CommandLine;
class FilePath;
}

namespace crash_reporter {

// Helper for running an embedded copy of crashpad_handler. Searches for and
// removes --(process_type_switch|user_data_dir_switch)=xyz arguments in the
// command line, and all options starting with '/' (for "/prefetch:N"), and then
// runs crashpad::HandlerMain with the remaining arguments. If user_data_dir is
// non-empty, a Crashpad extension to collect stability instrumentation on crash
// is used.
//
// Normally, pass switches::kProcessType and switches::kCrashpadHandler for
// process_type_switch and user_data_dir_switch. These are accepted as
// parameters because this component does not have access to content/, where
// those variables live.
int RunAsCrashpadHandler(const base::CommandLine& command_line,
                         const base::FilePath& user_data_dir,
                         const char* process_type_switch,
                         const char* user_data_dir_switch);

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_APP_RUN_AS_CRASHPAD_HANDLER_WIN_H_
