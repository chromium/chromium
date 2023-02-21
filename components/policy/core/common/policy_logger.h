// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOGGER_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOGGER_H_

#include <sstream>
#include <string>
#include <vector>

#include "base/location.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/policy_export.h"

namespace policy {

// Note: the DLOG_POLICY macro has no "#if DCHECK_IS_ON()" check because some
// messages logged with DLOG are still important to be seen on the
// chrome://policy/logs page in release mode. The DLOG call in StreamLog() will
// do the check as usual for command line logging.
#if BUILDFLAG(IS_ANDROID)
#define LOG_POLICY(log_severity, log_source) \
  LOG_POLICY_##log_severity(PolicyLogger::LogHelper::LogType::kLog, log_source)
#define DLOG_POLICY(log_severity, log_source) \
  LOG_POLICY_##log_severity(PolicyLogger::LogHelper::LogType::kDLog, log_source)
#define VLOG_POLICY(log_verbosity, log_source)                     \
  PolicyLogger::LogHelper(PolicyLogger::LogHelper::LogType::kVLog, \
                          PolicyLogger::Log::Severity::kVerbose,   \
                          log_verbosity, log_source, FROM_HERE)
#define DVLOG_POLICY(log_verbosity, log_source)                    \
  PolicyLogger::LogHelper(PolicyLogger::LogHelper::LogType::kDLog, \
                          PolicyLogger::Log::Severity::kVerbose,   \
                          log_verbosity, log_source, FROM_HERE)
#define LOG_POLICY_INFO(log_type, log_source)                                 \
  PolicyLogger::LogHelper(log_type, PolicyLogger::Log::Severity::kInfo,       \
                          PolicyLogger::LogHelper::kNoVerboseLog, log_source, \
                          FROM_HERE)
#define LOG_POLICY_WARNING(log_type, log_source)                              \
  PolicyLogger::LogHelper(log_type, PolicyLogger::Log::Severity::kWarning,    \
                          PolicyLogger::LogHelper::kNoVerboseLog, log_source, \
                          FROM_HERE)
#define LOG_POLICY_ERROR(log_type, log_source)                                \
  PolicyLogger::LogHelper(log_type, PolicyLogger::Log::Severity::kError,      \
                          PolicyLogger::LogHelper::kNoVerboseLog, log_source, \
                          FROM_HERE)
#else
#define LOG_POLICY(log_severity, log_source) LOG(log_severity)
#define DLOG_POLICY(log_severity, log_source) DLOG(log_severity)
#define VLOG_POLICY(log_verbosity, log_source) VLOG(log_verbosity)
#define DVLOG_POLICY(log_verbosity, log_source) DVLOG(log_verbosity)
#endif  // BUILDFLAG(IS_ANDROID)

#define POLICY_AUTH PolicyLogger::Log::Source::kAuthentication
#define POLICY_PROCESSING PolicyLogger::Log::Source::kPolicyProcessing
#define CBCM_ENROLLMENT PolicyLogger::Log::Source::kCBCMEnrollment
#define POLICY_FETCHING PolicyLogger::Log::Source::kPolicyFetching
#define PLATFORM_POLICY PolicyLogger::Log::Source::kPlatformPolicy

// Collects logs to be displayed in chrome://policy-logs.
class POLICY_EXPORT PolicyLogger {
 public:
  class POLICY_EXPORT Log {
   public:
    // The categories for policy log events.
    enum class Source {
      kPolicyProcessing,
      kCBCMEnrollment,
      kPolicyFetching,
      kPlatformPolicy,
      kAuthentication
    };
    enum class Severity { kInfo, kWarning, kError, kVerbose };

    Log(const Severity log_severity,
        const Source log_source,
        const std::string& message,
        const base::Location location);
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    Log(Log&&) = default;
    Log& operator=(Log&&) = default;
    ~Log() = default;

    Severity log_severity() const { return log_severity_; }
    Source log_source() const { return log_source_; }
    const std::string& message() const { return message_; }
    base::Location location() const { return location_; }
    base::Time timestamp() const { return timestamp_; }

    base::Value::Dict GetAsDict() const;

   private:
    Severity log_severity_;
    Source log_source_;
    std::string message_;
    base::Location location_;
    base::Time timestamp_;
  };

  // Helper class to temporarily hold log information before adding it as a Log
  // object to the logs list when it is destroyed.
  class LogHelper {
   public:
    // Value indicating that the log is not from VLOG, DVLOG, and other verbose
    // log macros.
    const static int kNoVerboseLog = -1;

    enum class LogType { kLog, kDLog, kVLog };

    LogHelper(const LogType log_type,
              const PolicyLogger::Log::Severity log_severity,
              const int log_verbosity,
              const PolicyLogger::Log::Source log_source,
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

    // Calls the appropriate base/logging macro.
    void StreamLog() const;

   private:
    LogType log_type_;
    PolicyLogger::Log::Severity log_severity_;
    int log_verbosity_;
    PolicyLogger::Log::Source log_source_;
    std::ostringstream message_buffer_;
    base::Location location_;
  };

  static PolicyLogger* GetInstance();

  PolicyLogger();
  PolicyLogger(const PolicyLogger&) = delete;
  PolicyLogger& operator=(const PolicyLogger&) = delete;
  ~PolicyLogger();

  // Returns the logs list as base::Value::List to send to UI.
  base::Value::List GetAsList() const;

  // Checks if browser is running on Android.
  bool IsPolicyLoggingEnabled() const;

  // Returns the logs size for testing purposes.
  size_t GetPolicyLogsSizeForTesting() const;

  // TODO(b/251799119): delete logs after an expiry period of ~30 minutes.

 private:
  // Adds a new log to the logs_ list.
  void AddLog(Log&& new_log);

  std::vector<Log> logs_ GUARDED_BY_CONTEXT(logs_list_sequence_checker_);

  SEQUENCE_CHECKER(logs_list_sequence_checker_);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOGGER_H_
