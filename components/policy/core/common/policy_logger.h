// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOGGER_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOGGER_H_

#include <string>
#include <vector>

#include "base/location.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/policy_export.h"

namespace policy {

// Collects logs to be displayed in chrome://policy-logs.
class POLICY_EXPORT PolicyLogger {
 public:
  class POLICY_EXPORT Log {
   public:
    // The categories for policy log events.
    enum class LogSource { kCBCMEnrollment, kPolicyFetching, kPlatformPolicy };

    Log(const LogSource log_source,
        const std::string& message,
        const base::Location location);
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    Log(Log&&) = default;
    Log& operator=(Log&&) = default;
    ~Log() = default;

    LogSource log_source() const { return log_source_; }
    const std::string& message() const { return message_; }
    base::Location location() const { return location_; }
    base::Time timestamp() const { return timestamp_; }

    base::Value GetAsValue() const;

   private:
    LogSource log_source_;
    std::string message_;
    base::Location location_;
    base::Time timestamp_;
  };

  static PolicyLogger* GetInstance();

  PolicyLogger();
  PolicyLogger(const PolicyLogger&) = delete;
  PolicyLogger& operator=(const PolicyLogger&) = delete;
  ~PolicyLogger();

  // Adds a new log and calls OnLogsChanged for observers.
  void AddLog(Log&& new_log);

  // Returns the logs list as base::Value to send to UI.
  base::Value GetAsValue() const;

  // Checks if browser is running on Android.
  bool IsPolicyLoggingEnabled();

  // Returns the logs size for testing purposes.
  int GetPolicyLogsSizeForTesting();

  // TODO(b/251799119): delete logs after an expiry period of ~30 minutes.

 private:
  std::vector<Log> logs_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOGGER_H_
