// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_CRASH_CRASH_CLIENT_H_
#define CHROME_CHROME_CLEANER_CRASH_CRASH_CLIENT_H_

#include <map>
#include <string>

#include "base/synchronization/lock.h"
#include "chrome/chrome_cleaner/settings/settings_types.h"

namespace chrome_cleaner {

// This class manages interaction with the crash reporter.
class CrashClient {
 public:
  enum class Mode { REPORTER, CLEANER, MODE_COUNT };

  static CrashClient* GetInstance();

  // Set |client_id| to the current guid associated with crashes. |client_id|
  // may be empty if no guid is associated.
  static void GetClientId(std::wstring* client_id);

  // Returns whether upload of crashes is enabled or not.
  static bool IsUploadEnabled();

  CrashClient() = default;

  CrashClient(const CrashClient&) = delete;
  CrashClient& operator=(const CrashClient&) = delete;

  virtual ~CrashClient() = default;

  // Initializes collection and upload of crash reports. This will only be done
  // if the user has agreed to crash dump reporting.
  //
  // Crash reporting has to be initialized as early as possible (e.g., the first
  // thing in main()) to catch crashes occurring during process startup. Crashes
  // which occur during the global static construction phase will not be caught
  // and reported. This should not be a problem as static non-POD objects are
  // not allowed by the style guide and exceptions to this rule are rare.
  //
  // |mode| controls a custom info entry present in the generated dumps to allow
  // distinguishing between cleaner and reporter crashes on the backend.
  //
  // |process_type| identifies the type of process that reported the crash.
  virtual bool InitializeCrashReporting(Mode mode,
                                        SandboxType process_type) = 0;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_CRASH_CRASH_CLIENT_H_
