// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_CRASH_REPORTING_H_
#define CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_CRASH_REPORTING_H_

#include <memory>
#include <string_view>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"

class UserCrashState;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace windows_services {

// Launches a crashpad handler in a child process. `directory_name`, which
// should be distinct for each process type, names the parent directory for the
// crash database. An individual user's crash databases will therefore be in a
// directory such as C:\Windows\SystemTemp\<directory_name>\Crashpad\<USER_SID>.
// `process_type` will be put in the `ptype` annotation of crashes. If not null,
// `task_runner` indicates the sequence on which the launch will take place.
// This function returns to the caller only after the handler has started.
void StartCrashHandler(std::unique_ptr<UserCrashState> user_crash_state,
                       base::FilePath::StringViewType directory_name,
                       std::string_view process_type,
                       scoped_refptr<base::SequencedTaskRunner> task_runner);

}  // namespace windows_services

#endif  // CHROME_WINDOWS_SERVICES_SERVICE_PROGRAM_CRASH_REPORTING_H_
