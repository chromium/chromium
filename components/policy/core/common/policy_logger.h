// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOGGER_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOGGER_H_

#include <sstream>
#include <string>
#include <vector>

#include "base/location.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/policy_export.h"

namespace policy {

#define LOG_POLICY(log_source)                           \
  !PolicyLogger::GetInstance()->IsPolicyLoggingEnabled() \
      ? (void)0                                          \
      : internal::Voidify() & PolicyLogger::LogHelper(log_source, FROM_HERE)

#define CBCM_ENROLLMENT PolicyLogger::Log::LogSource::kCBCMEnrollment
#define POLICY_FETCHING PolicyLogger::Log::LogSource::kPolicyFetching
#define PLATFORM_POLICY PolicyLogger::Log::LogSource::kPlatformPolicy

namespace internal {

// This class is used to explicitly ignore values in the conditional
// logging macros. This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".
class Voidify {
 public:
  Voidify() = default;
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  template <typename U>
  void operator&(const U&) {}
};

}  // namespace internal

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

  // Helper class to temporarily hold log information before adding it as a Log
  // object to the logs list when it is destroyed.
  class LogHelper {
   public:
    LogHelper(const PolicyLogger::Log::LogSource log_source,
              const base::Location location);
    LogHelper(const LogHelper&) = delete;
    LogHelper& operator=(const LogHelper&) = delete;
    LogHelper(LogHelper&&) = delete;
    LogHelper& operator=(LogHelper&&) = delete;
    // Moves the log to the list.
    ~LogHelper();

    template <typename T>
    LogHelper& operator<<(T message) {
      message_buffer_ << message;
      return *this;
    }

   private:
    PolicyLogger::Log::LogSource log_source_;
    std::ostringstream message_buffer_;
    base::Location location_;
  };

  static PolicyLogger* GetInstance();

  PolicyLogger();
  PolicyLogger(const PolicyLogger&) = delete;
  PolicyLogger& operator=(const PolicyLogger&) = delete;
  ~PolicyLogger();

  // Returns the logs list as base::Value to send to UI.
  base::Value GetAsValue() const;

  // Checks if browser is running on Android.
  bool IsPolicyLoggingEnabled();

  // Returns the logs size for testing purposes.
  int GetPolicyLogsSizeForTesting();

  // TODO(b/251799119): delete logs after an expiry period of ~30 minutes.

 private:
  // Adds a new log to the logs_ list.
  void AddLog(Log&& new_log);

  std::vector<Log> logs_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOGGER_H_
