// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/service_program/logging_support.h"

#include <windows.h>

#include <stdint.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "chrome/windows_services/service_program/switches.h"

void InitializeLogging(const base::CommandLine& command_line) {
  // Log to stderr and an attached debugger by default.
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;

  // Get a handle to a potential test process that wishes to read logs from this
  // service process.
  base::Process source_process;
  if (uint32_t source_pid; base::StringToUint(command_line.GetSwitchValueASCII(
                                                  switches::kLogFileSource),
                                              &source_pid) &&
                           source_pid) {
    source_process =
        base::Process::OpenWithAccess(source_pid, PROCESS_DUP_HANDLE);
  }

  // If a handle to a test process was acquired and it provided a handle to
  // which this service process should write, duplicate that file handle into
  // the service process.
  base::win::ScopedHandle log_file_handle;
  if (uint32_t file_handle_value;
      source_process.IsValid() &&
      base::StringToUint(command_line.GetSwitchValueASCII(switches::kLogFile),
                         &file_handle_value) &&
      file_handle_value) {
    HANDLE file_handle = base::win::Uint32ToHandle(file_handle_value);
    if (file_handle != INVALID_HANDLE_VALUE) {
      HANDLE duplicate;
      if (::DuplicateHandle(source_process.Handle(), file_handle,
                            ::GetCurrentProcess(), &duplicate,
                            /*dwDesiredAccess=*/0, /*bInheritHandle=*/FALSE,
                            DUPLICATE_SAME_ACCESS)) {
        log_file_handle.Set(duplicate);
      }
    }
  }

  // Direct logging to a file handle rather than stderr, if provided.
  if (log_file_handle.is_valid()) {
    settings.logging_dest &= ~logging::LOG_TO_STDERR;
    settings.logging_dest |= logging::LOG_TO_FILE;
    // Pass ownership of this handle to logging.
    settings.log_file = log_file_handle.release();
  }

  logging::InitLogging(settings);
}
