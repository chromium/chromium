// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_GLOBAL_CONSTANTS_H_
#define CHROME_ENTERPRISE_COMPANION_GLOBAL_CONSTANTS_H_

#include <optional>

#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <string>
#endif

namespace base {
class FilePath;
}

class GURL;

namespace enterprise_companion {

// The app ID to register to the updater.
extern const char kCompanionAppId[];

// JSON keys for defining overrides. All values are expected to be strings
// unless otherwise noted.
extern const char kCrashUploadUrlKey[];
extern const char kDMEncryptedReportingUrlKey[];
extern const char kDMRealtimeReportingUrlKey[];
extern const char kDMServerUrlKey[];
extern const char kEventLoggingUrlKey[];
// The minimum timeout for the event logger's transmissions in integer seconds.
extern const char kEventLoggerMinTimeoutSecKey[];
#if BUILDFLAG(IS_WIN)
extern const char kNamedPipeSecurityDescriptorKey[];
#endif

// Constants for the application which may be overridden in test builds via a
// JSON file at the path returned by `GetOverridesFilePath`. See above for
// recognized keys.
class GlobalConstants {
 public:
  virtual ~GlobalConstants() = default;

  virtual GURL CrashUploadURL() const = 0;
  virtual GURL DeviceManagementEncryptedReportingURL() const = 0;
  virtual GURL DeviceManagementRealtimeReportingURL() const = 0;
  virtual GURL DeviceManagementServerURL() const = 0;
  virtual GURL EnterpriseCompanionEventLoggingURL() const = 0;
  virtual base::TimeDelta EventLoggerMinTimeout() const = 0;
#if BUILDFLAG(IS_WIN)
  // The security descriptor to be applied to the server's named pipe for Mojo
  // connections.
  virtual std::wstring NamedPipeSecurityDescriptor() const = 0;
#endif
};

// Returns the path to the overrides JSON file.
std::optional<base::FilePath> GetOverridesFilePath();

// Returns the global constants singleton for the process. Initializes constants
// and applies overrides (in test builds) on the first invocation.
const GlobalConstants* GetGlobalConstants();

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_GLOBAL_CONSTANTS_H_
