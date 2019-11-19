// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CRASH_CLIENT_H_
#define CHROME_UPDATER_CRASH_CLIENT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/sequence_checker.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace crashpad {
class CrashReportDatabase;
}  // namespace crashpad

namespace updater {

// This class manages interaction with the crash reporter.
class CrashClient {
 public:
  static CrashClient* GetInstance();

  // Retrieves the current guid associated with crashes. The value may be empty
  // if no guid is associated.
  static std::string GetClientId();

  // Returns true if the upload of crashes is enabled.
  static bool IsUploadEnabled();

  // Initializes collection and upload of crash reports.
  bool InitializeCrashReporting();

  // Initializes the crash database only. Used in the crash reporter, which
  // cannot connect to itself to upload its own crashes.
  bool InitializeDatabaseOnly();

  crashpad::CrashReportDatabase* database() { return database_.get(); }

 private:
  friend class base::NoDestructor<CrashClient>;

  CrashClient();
  ~CrashClient();

  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<crashpad::CrashReportDatabase> database_;

  DISALLOW_COPY_AND_ASSIGN(CrashClient);
};

}  // namespace updater

#endif  // CHROME_UPDATER_CRASH_CLIENT_H_
