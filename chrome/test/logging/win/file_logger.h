// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LOGGING_WIN_FILE_LOGGER_H_
#define CHROME_TEST_LOGGING_WIN_FILE_LOGGER_H_

#include <guiddef.h>
#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/win/event_trace_controller.h"

namespace base {
class FilePath;
}

namespace logging_win {

// A file logger instance captures LOG messages and trace events emitted via
// Event Tracing for Windows (ETW) and sends them to a file.  Events can be
// pulled from the file sometime later with PrintLogFile or ReadLogFile
// (currently in log_file_printer_win.h and log_file_reader_win.h,
// respectively).
//
// Important usage notes (read this):
// - Due to the nature of event generation, only one instance of this class may
//   be initialized at a time.
// - This class is not thread safe.
// - This class uses facilities that require the process to run with admin
//   rights; StartLogging() will return false if this is not the case.
class FileLogger {
 public:
  enum EventProviderBits {
    // Log messages from chrome.exe.
    CHROME_LOG_PROVIDER         = 1 << 0,
    // Log messages from npchrome_frame.dll.
    CHROME_FRAME_LOG_PROVIDER   = 1 << 1,
    // Log messages from the current process.
    CHROME_TESTS_LOG_PROVIDER   = 1 << 2,
  };

  static const uint32_t kAllEventProviders =
      (CHROME_LOG_PROVIDER | CHROME_FRAME_LOG_PROVIDER |
       CHROME_TESTS_LOG_PROVIDER);

  FileLogger();
  ~FileLogger();

  // Initializes the instance to collect logs from all supported providers.
  void Initialize();

  // Initializes the instance to collect logs from the providers present in
  // the given mask; see EventProviderBits.
  void Initialize(uint32_t event_provider_mask);

  // Starts capturing logs from all providers into |log_file|.  The common file
  // extension for such files is .etl.  Returns false if the session could not
  // be started (e.g., if not running as admin) or if no providers could be
  // enabled.
  bool StartLogging(const base::FilePath& log_file);

  // Stops capturing logs.
  void StopLogging();

  // Returns true if logs are being captured.
  bool is_logging() const {
    return controller_.session_name() && *controller_.session_name();
  }

 private:
  bool EnableProviders();
  void DisableProviders();

  static bool is_initialized_;

  base::win::EtwTraceController controller_;
  uint32_t event_provider_mask_;

  DISALLOW_COPY_AND_ASSIGN(FileLogger);
};

}  // namespace logging_win

#endif  // CHROME_TEST_LOGGING_WIN_FILE_LOGGER_H_
