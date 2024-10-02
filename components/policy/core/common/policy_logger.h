// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOGGER_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOGGER_H_

#include <deque>
#include <sstream>
#include <string>
#include <string_view>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/policy_export.h"

// Note: the DLOG_POLICY macro has no "#if DCHECK_IS_ON()" check because some
// messages logged with DLOG are still important to be seen on the
// chrome://policy/logs page in release mode. The DLOG call in StreamLog() will
// do the check as usual for command line logging.
#define LOG_POLICY(log_severity, log_source)                                  \
  LOG_POLICY_##log_severity(::policy::PolicyLogger::LogHelper::LogType::kLog, \
                            log_source)
#define DLOG_POLICY(log_severity, log_source)                                  \
  LOG_POLICY_##log_severity(::policy::PolicyLogger::LogHelper::LogType::kDLog, \
                            log_source)
#define VLOG_POLICY(log_verbosity, log_source)                        \
  ::policy::PolicyLogger::LogHelper(                                  \
      ::policy::PolicyLogger::LogHelper::LogType::kVLog,              \
      ::policy::PolicyLogger::Log::Severity::kVerbose, log_verbosity, \
      log_source, std::string_view(__FILE__, std::size(__FILE__)), __LINE__)
#define DVLOG_POLICY(log_verbosity, log_source)                       \
  ::policy::PolicyLogger::LogHelper(                                  \
      ::policy::PolicyLogger::LogHelper::LogType::kDLog,              \
      ::policy::PolicyLogger::Log::Severity::kVerbose, log_verbosity, \
      log_source, std::string_view(__FILE__, std::size(__FILE__)), __LINE__)
#define LOG_POLICY_INFO(log_type, log_source)                       \
  ::policy::PolicyLogger::LogHelper(                                \
      log_type, ::policy::PolicyLogger::Log::Severity::kInfo,       \
      ::policy::PolicyLogger::LogHelper::kNoVerboseLog, log_source, \
      std::string_view(__FILE__, std::size(__FILE__)), __LINE__)
#define LOG_POLICY_WARNING(log_type, log_source)                    \
  ::policy::PolicyLogger::LogHelper(                                \
      log_type, ::policy::PolicyLogger::Log::Severity::kWarning,    \
      ::policy::PolicyLogger::LogHelper::kNoVerboseLog, log_source, \
      std::string_view(__FILE__, std::size(__FILE__)), __LINE__)
#define LOG_POLICY_ERROR(log_type, log_source)                      \
  ::policy::PolicyLogger::LogHelper(                                \
      log_type, ::policy::PolicyLogger::Log::Severity::kError,      \
      ::policy::PolicyLogger::LogHelper::kNoVerboseLog, log_source, \
      std::string_view(__FILE__, std::size(__FILE__)), __LINE__)

#define POLICY_AUTH ::policy::PolicyLogger::Log::Source::kAuthentication
#define POLICY_PROCESSING ::policy::PolicyLogger::Log::Source::kPolicyProcessing
#define CBCM_ENROLLMENT ::policy::PolicyLogger::Log::Source::kCBCMEnrollment
#define POLICY_FETCHING ::policy::PolicyLogger::Log::Source::kPolicyFetching
#define PLATFORM_POLICY ::policy::PolicyLogger::Log::Source::kPlatformPolicy
#define REMOTE_COMMANDS ::policy::PolicyLogger::Log::Source::kRemoteCommands
#define DEVICE_TRUST ::policy::PolicyLogger::Log::Source::kDeviceTrust
#define OIDC_ENROLLMENT ::policy::PolicyLogger::Log::Source::kOidcEnrollment
#define EXTENSIBLE_SSO ::policy::PolicyLogger::Log::Source::kExtensibleSSO

namespace policy {

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
      kAuthentication,
      kRemoteCommands,
      kDeviceTrust,
      kOidcEnrollment,
      kExtensibleSSO
    };
    enum class Severity { kInfo, kWarning, kError, kVerbose };

    Log(const Severity log_severity,
        const Source log_source,
        const std::string& message,
        std::string_view file,
        const int line);
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    Log(Log&&) = default;
    Log& operator=(Log&&) = default;
    ~Log() = default;

    Severity log_severity() const { return log_severity_; }
    Source log_source() const { return log_source_; }
    const std::string& message() const { return message_; }
    const std::string_view& file() const { return file_; }
    int line() const { return line_; }
    base::Time timestamp() const { return timestamp_; }

    base::Value::Dict GetAsDict() const;

   private:
    Severity log_severity_;
    Source log_source_;
    std::string message_;
    std::string_view file_;
    int line_;
    base::Time timestamp_;
  };

  // Helper class to temporarily hold log information before adding it as a Log
  // object to the logs list when it is destroyed.
  class POLICY_EXPORT LogHelper {
   public:
    // Value indicating that the log is not from VLOG, DVLOG, and other verbose
    // log macros.
    const static int kNoVerboseLog = -1;

    enum class LogType { kLog, kDLog, kVLog };

    LogHelper(const LogType log_type,
              const PolicyLogger::Log::Severity log_severity,
              const int log_verbosity,
              const PolicyLogger::Log::Source log_source,
              std::string_view file,
              const int line);
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
    std::string_view file_;
    int line_;
  };

  static constexpr base::TimeDelta kTimeToLive = base::Minutes(30);
  static constexpr size_t kMaxLogsSize = 200;

  static PolicyLogger* GetInstance();

  PolicyLogger();
  PolicyLogger(const PolicyLogger&) = delete;
  PolicyLogger& operator=(const PolicyLogger&) = delete;
  ~PolicyLogger();

  // Returns the logs list as base::Value::List to send to UI.
  base::Value::List GetAsList();

  // Checks if browser is running on Android.
  bool IsPolicyLoggingEnabled() const;

  // Sets `is_log_deletion_enabled_` to allow scheduling old log deletion.
  void EnableLogDeletion();

  // Returns the logs size for testing purposes.
  size_t GetPolicyLogsSizeForTesting();

  // Clears `logs_` and sets `is_log_deletion_scheduled_` as cleanup after every
  // test.
  void ResetLoggerForTesting();

 private:
  // Adds a new log to the logs_ list and calls `ScheduleOldLogsDeletion` if
  // there is no deletion task scheduled.
  void AddLog(Log&& new_log);

  // Deletes logs in the list that have been in the list for `kTimeToLive`
  // minutes to an hour.
  void DeleteOldLogs();

  // Posts a new log deletion task and sets the `is_log_deletion_scheduled_`
  // flag.
  void ScheduleOldLogsDeletion();

  // Log deletion scheduling fails in unit tests when there is no task
  // environment (See crbug.com/1434241). To avoid having  a task environment in
  // every existing and new unit test that calls a function with logs, this flag
  // is disabled in unit tests, and enabled everywhere else early in the policy
  // stack initialization from `BrowserPolicyConnector::Init`.
  bool is_log_deletion_enabled_{false};

  bool is_log_deletion_scheduled_{false};

  base::Lock lock_;

  std::deque<Log> logs_ GUARDED_BY(lock_);

  base::WeakPtrFactory<PolicyLogger> weak_factory_{this};
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOGGER_H_
