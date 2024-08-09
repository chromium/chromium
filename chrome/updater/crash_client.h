// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CRASH_CLIENT_H_
#define CHROME_UPDATER_CRASH_CLIENT_H_

#include <memory>
#include <string>

#include "base/no_destructor.h"
#include "base/sequence_checker.h"

namespace crashpad {
class CrashReportDatabase;
}  // namespace crashpad

namespace updater {

enum class UpdaterScope;

// This class manages interaction with the crash reporter.
class CrashClient {
 public:
  CrashClient(const CrashClient&) = delete;
  CrashClient& operator=(const CrashClient&) = delete;

  static CrashClient* GetInstance();

  // Retrieves the current guid associated with crashes. The value may be empty
  // if no guid is associated.
  static std::string GetClientId();

  // Initializes collection and upload of crash reports.
  [[nodiscard]] bool InitializeCrashReporting(UpdaterScope updater_scope);

  // Initializes the crash database only. Used in the crash reporter, which
  // cannot connect to itself to upload its own crashes.
  [[nodiscard]] bool InitializeDatabaseOnly(UpdaterScope updater_scope);

  // Enables uploading of crashes if `enabled` is true.
  bool SetUploadsEnabled(bool enabled);

 private:
  friend class base::NoDestructor<CrashClient>;

  CrashClient();
  ~CrashClient();

  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<crashpad::CrashReportDatabase> database_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_CRASH_CLIENT_H_
