// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_WATCHER_EXTENDED_CRASH_REPORTING_H_
#define COMPONENTS_BROWSER_WATCHER_EXTENDED_CRASH_REPORTING_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"

namespace base {
namespace debug {
class GlobalActivityTracker;
}  // namespace debug
}  // namespace base

namespace browser_watcher {

class ExtendedCrashReporting {
 public:
  enum ProcessType { kBrowserProcess, kOther };

  ~ExtendedCrashReporting();

  // Initializes extended crash reporting for this process if enabled.
  // Returns nullptr if extended crash reporting is disabled.
  // Should only be called once in any one process.
  static ExtendedCrashReporting* SetUpIfEnabled(ProcessType process_type);

  // Retrieves the extended crash reporting instance for this process if
  // it exists, or nullptr if it does not.
  static ExtendedCrashReporting* GetInstance();

  // Records identifying strings for the product and version for an extended
  // crash report. This function is threadsafe.
  void SetProductStrings(const std::u16string& product_name,
                         const std::u16string& product_version,
                         const std::u16string& channel_name,
                         const std::u16string& special_build);

  // Adds or updates the global extended crash reporting data.
  // These functions are threadsafe.
  void SetBool(base::StringPiece name, bool value);
  void SetInt(base::StringPiece name, int64_t value);

  // Adds or updates the global extended crash reporting data, if enabled.
  static void SetDataBool(base::StringPiece name, bool value);
  static void SetDataInt(base::StringPiece name, int64_t value);

  // Allows tests to initialize and teardown the global instance.
  static void SetUpForTesting();
  static void TearDownForTesting();

 private:
  explicit ExtendedCrashReporting(base::debug::GlobalActivityTracker* tracker);
  static ExtendedCrashReporting* SetUpImpl(ProcessType process_type);
  void Initialize(ProcessType process_type);

  // Registers a vectored exception handler that stores exception details to the
  // activity report on exception - handled or not.
  void RegisterVEH();

  raw_ptr<void> veh_handle_ = nullptr;
  const raw_ptr<base::debug::GlobalActivityTracker> tracker_;
};

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_EXTENDED_CRASH_REPORTING_H_
