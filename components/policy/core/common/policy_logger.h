// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_LOGGER_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_LOGGER_H_

#include <atomic>
#include <deque>
#include <sstream>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/policy_export.h"
#include "components/policy/resources/webui/mojom/policy.mojom-forward.h"

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
      log_source, std::string_view(__FILE__), __LINE__)
#define DVLOG_POLICY(log_verbosity, log_source)                       \
  ::policy::PolicyLogger::LogHelper(                                  \
      ::policy::PolicyLogger::LogHelper::LogType::kDLog,              \
      ::policy::PolicyLogger::Log::Severity::kVerbose, log_verbosity, \
      log_source, std::string_view(__FILE__), __LINE__)
#define LOG_POLICY_INFO(log_type, log_source)                       \
  ::policy::PolicyLogger::LogHelper(                                \
      log_type, ::policy::PolicyLogger::Log::Severity::kInfo,       \
      ::policy::PolicyLogger::LogHelper::kNoVerboseLog, log_source, \
      std::string_view(__FILE__), __LINE__)
#define LOG_POLICY_WARNING(log_type, log_source)                    \
  ::policy::PolicyLogger::LogHelper(                                \
      log_type, ::policy::PolicyLogger::Log::Severity::kWarning,    \
      ::policy::PolicyLogger::LogHelper::kNoVerboseLog, log_source, \
      std::string_view(__FILE__), __LINE__)
#define LOG_POLICY_ERROR(log_type, log_source)                      \
  ::policy::PolicyLogger::LogHelper(                                \
      log_type, ::policy::PolicyLogger::Log::Severity::kError,      \
      ::policy::PolicyLogger::LogHelper::kNoVerboseLog, log_source, \
      std::string_view(__FILE__), __LINE__)

#define POLICY_AUTH ::policy::PolicyLogger::Log::Source::kAuthentication
#define POLICY_PROCESSING ::policy::PolicyLogger::Log::Source::kPolicyProcessing
#define CBCM_ENROLLMENT ::policy::PolicyLogger::Log::Source::kCBCMEnrollment
#define POLICY_FETCHING ::policy::PolicyLogger::Log::Source::kPolicyFetching
#define PLATFORM_POLICY ::policy::PolicyLogger::Log::Source::kPlatformPolicy
#define REMOTE_COMMANDS ::policy::PolicyLogger::Log::Source::kRemoteCommands
#define DEVICE_TRUST ::policy::PolicyLogger::Log::Source::kDeviceTrust
#define OIDC_ENROLLMENT ::policy::PolicyLogger::Log::Source::kOidcEnrollment
#define EXTENSIBLE_SSO ::policy::PolicyLogger::Log::Source::kExtensibleSSO
#define REPORTING ::policy::PolicyLogger::Log::Source::kReporting

namespace policy {

// Collects logs to be displayed in chrome://policy/logs.
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
      kExtensibleSSO,
      kReporting,
      kMaxValue = kReporting,
    };
    enum class Severity {
      kInfo,
      kWarning,
      kError,
      kVerbose,
      kMaxValue = kVerbose,
    };

    Log(const Severity log_severity,
        const Source log_source,
        std::string message,
        std::string_view file,
        const int line,
        base::Time timestamp = base::Time::Now());
    Log(const Log&);
    Log& operator=(const Log&) = delete;
    Log(Log&&);
    Log& operator=(Log&&);
    ~Log();

    Severity log_severity() const { return log_severity_; }
    Source log_source() const { return log_source_; }
    const std::string& message() const { return message_; }
    const std::string& file() const { return file_; }
    int line() const { return line_; }
    base::Time timestamp() const { return timestamp_; }

    base::DictValue GetAsDict() const;

    policy::mojom::LogPtr GetAsMojoLog() const;

   private:
    Severity log_severity_;
    Source log_source_;
    std::string message_;
    std::string file_;
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
    LogHelper& operator<<(const T& message) {
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

  using GetAsListCallback = base::OnceCallback<void(base::ListValue)>;
  using GetAsMojoListCallback =
      base::OnceCallback<void(std::vector<policy::mojom::LogPtr>)>;

  // PolicyLogger stores 2 kinds of logs: uncompressed and compressed. All new
  // logs go into `logs_` by default. When `logs_` would overflow, they are
  // serialized, compressed, and stored in an in-memory buffer.
  //
  // The fundamental tradeoff is:
  // - kMaxUncompressedLogCount too low: PolicyLogger needs to re-serialize and
  //   re-compress too often, eating up CPU for no reason.
  // - kMaxUncompressedLogCount too high: PolicyLogger uses too much
  //   memory. Keep in mind that 99.99% of users never use chrome://policy/logs
  //   anyways.
  //
  // With compression, we can store a couple days' logs without using much
  // memory. The logs compress extremely well in practice. For instance, a
  // 24-hour test with real-life stores 1146 messages in a 11kB buffer.
  static constexpr size_t kMaxUncompressedLogCount = 100;
  static constexpr size_t kMaxCompressedLogCount = 2000;

  static PolicyLogger* GetInstance();

  static bool IsPolicyLoggingEnabled();

  PolicyLogger();
  PolicyLogger(const PolicyLogger&) = delete;
  PolicyLogger& operator=(const PolicyLogger&) = delete;
  ~PolicyLogger();

  // Enables compressing logs into an in-memory buffer. Uses
  // `compression_task_runner` to not block the main thread with CPU-bound
  // work. Log compression is disabled until you call this.
  void EnableLogCompression(
      scoped_refptr<base::SequencedTaskRunner> compression_task_runner);

  // Returns the logs list as base::ListValue to send to UI asynchronously.
  void GetAsList(GetAsListCallback callback);

  // Returns the logs in the mojo format asynchronously.
  void GetAsMojoList(GetAsMojoListCallback callback);

  // Records memory usage and log count UMA metrics.
  void RecordPerformanceMetrics();

  // Clears logs and disables compression as cleanup after every test.
  void ResetLoggerForTesting();

 private:
  // Adds a new log, and triggers compression if needed.
  void AddLog(Log&& new_log);

  // Retrieves all logs asynchronously, also decompressing from memory if
  // compression is enabled.
  void GetLogs(base::OnceCallback<void(std::vector<Log>)> callback);

  void CompressAndAppendLogs(std::vector<Log> logs);
  std::vector<Log> DecompressAndReadLogs();

  // Lock for `logs_`. Note that `compressed_buffer_` is not guarded by this
  // lock, because it should always be accessed from `compression_task_runner_`.
  base::Lock lock_;
  // Uncompressed logs are stored in this deque.
  std::deque<Log> logs_ GUARDED_BY(lock_);

  SEQUENCE_CHECKER(compression_sequence_checker_);
  scoped_refptr<base::SequencedTaskRunner> compression_task_runner_
      GUARDED_BY(lock_);
  // Compressed logs are stored in this buffer.
  std::string compressed_buffer_
      GUARDED_BY_CONTEXT(compression_sequence_checker_);
  std::atomic<size_t> compressed_buffer_size_{0};
  std::atomic<size_t> compressed_log_count_{0};
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOGGER_H_
