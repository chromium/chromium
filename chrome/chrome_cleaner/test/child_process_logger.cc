// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/child_process_logger.h"

#include <vector>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"

#include <windows.h>

namespace chrome_cleaner {

ChildProcessLogger::ChildProcessLogger() = default;

ChildProcessLogger::~ChildProcessLogger() = default;

bool ChildProcessLogger::Initialize() {
  // Adapted from
  // https://cs.chromium.org/chromium/src/sandbox/win/src/handle_inheritance_test.cc
  if (!temp_dir_.CreateUniqueTempDir()) {
    PLOG(ERROR) << "Could not create temp dir for child stdout";
    return false;
  }

  if (!CreateTemporaryFileInDir(temp_dir_.GetPath(), &temp_file_name_)) {
    PLOG(ERROR) << "Could not create temp file for child stdout";
    return false;
  }

  SECURITY_ATTRIBUTES attrs = {};
  attrs.nLength = sizeof(attrs);
  attrs.bInheritHandle = true;

  child_stdout_handle_.Set(
      ::CreateFile(temp_file_name_.value().c_str(), GENERIC_WRITE,
                   FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
                   &attrs, OPEN_EXISTING, 0, nullptr));
  if (!child_stdout_handle_.IsValid()) {
    PLOG(ERROR) << "Could not open child stdout file";
    return false;
  }

  return true;
}

void ChildProcessLogger::UpdateLaunchOptions(
    base::LaunchOptions* options) const {
  DCHECK(child_stdout_handle_.IsValid());
  options->handles_to_inherit.push_back(child_stdout_handle_.Get());
  options->stdin_handle = INVALID_HANDLE_VALUE;
  options->stdout_handle = child_stdout_handle_.Get();
  options->stderr_handle = child_stdout_handle_.Get();
}

void ChildProcessLogger::UpdateSandboxPolicy(
    sandbox::TargetPolicy* policy) const {
  DCHECK(child_stdout_handle_.IsValid());
  policy->SetStdoutHandle(child_stdout_handle_.Get());
  policy->SetStderrHandle(child_stdout_handle_.Get());
}

void ChildProcessLogger::DumpLogs() const {
  DCHECK(!temp_file_name_.empty());

  if (!base::PathExists(temp_file_name_)) {
    LOG(ERROR) << "Child process log file doesn't exist";
    return;
  }

  // Collect the child process log file, and dump the contents, to help
  // debugging failures.
  std::string log_file_contents;
  if (!base::ReadFileToString(temp_file_name_, &log_file_contents)) {
    LOG(ERROR) << "Failed to read child process log file";
    return;
  }

  std::vector<base::StringPiece> lines =
      base::SplitStringPiece(log_file_contents, "\n", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  if (lines.empty()) {
    LOG(ERROR) << "Child process log file is empty";
    return;
  }

  LOG(ERROR) << "Dumping child process logs";
  for (const auto& line : lines) {
    LOG(ERROR) << "Child process: " << line;
  }
  LOG(ERROR) << "Finished dumping child process logs";
}

}  // namespace chrome_cleaner
