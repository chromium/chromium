// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_logger.h"

#include <deque>
#include <string_view>
#include <utility>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "components/policy/core/common/features.h"
#include "components/version_info/version_info.h"

namespace policy {

namespace {

// The base format for the Chromium Code Search URLs.
constexpr char kChromiumCSUrlFormat[] =
    "https://source.chromium.org/chromium/chromium/src/+/main:%s;l=%i;drc:%s";

// Gets the string value for the log source.
std::string GetLogSourceValue(const PolicyLogger::Log::Source log_source) {
  switch (log_source) {
    case PolicyLogger::Log::Source::kPolicyProcessing:
      return "Policy Processing";
    case PolicyLogger::Log::Source::kCBCMEnrollment:
      return "CBCM Enrollment";
    case PolicyLogger::Log::Source::kPlatformPolicy:
      return "Platform Policy";
    case PolicyLogger::Log::Source::kPolicyFetching:
      return "Policy Fetching";
    case PolicyLogger::Log::Source::kAuthentication:
      return "Authentication";
    case PolicyLogger::Log::Source::kRemoteCommands:
      return "Remote Commands";
    case PolicyLogger::Log::Source::kDeviceTrust:
      return "Device Trust";
    case PolicyLogger::Log::Source::kOidcEnrollment:
      return "OIDC Enrollment";
    case PolicyLogger::Log::Source::kExtensibleSSO:
      return "Extensible SSO";
  }
}

std::string GetLogSeverity(const PolicyLogger::Log::Severity log_severity) {
  switch (log_severity) {
    case PolicyLogger::Log::Severity::kInfo:
      return "INFO";
    case PolicyLogger::Log::Severity::kWarning:
      return "WARNING";
    case PolicyLogger::Log::Severity::kError:
      return "ERROR";
    case PolicyLogger::Log::Severity::kVerbose:
      return "VERBOSE";
  }
}

int GetLogSeverityInt(const PolicyLogger::Log::Severity log_severity) {
  switch (log_severity) {
    case PolicyLogger::Log::Severity::kInfo:
      return ::logging::LOGGING_INFO;
    case PolicyLogger::Log::Severity::kWarning:
      return ::logging::LOGGING_WARNING;
    case PolicyLogger::Log::Severity::kError:
      return ::logging::LOGGING_ERROR;
    case PolicyLogger::Log::Severity::kVerbose:
      return ::logging::LOGGING_VERBOSE;
  }
}

// Constructs the URL for Chromium Code Search that points to the line of code
// that generated the log and the Chromium git revision hash.
std::string GetLineURL(const char* file, int line) {
  std::string last_change(version_info::GetLastChange());

  // The substring separates the last change commit hash from the branch name on
  // the '-'.
  return base::StringPrintf(
      kChromiumCSUrlFormat, file, line,
      last_change.substr(0, last_change.find('-')).c_str());
}

// Checks if the log has been if the list for at least `kTimeToLive` minutes.
bool IsLogExpired(PolicyLogger::Log& log) {
  return base::Time::Now() - log.timestamp() >= PolicyLogger::kTimeToLive;
}

}  // namespace

PolicyLogger::Log::Log(const Severity log_severity,
                       const Source log_source,
                       const std::string& message,
                       std::string_view file,
                       const int line)
    : log_severity_(log_severity),
      log_source_(log_source),
      message_(message),
      file_(file),
      line_(line),
      timestamp_(base::Time::Now()) {}
PolicyLogger* PolicyLogger::GetInstance() {
  static base::NoDestructor<PolicyLogger> instance;
  return instance.get();
}

PolicyLogger::LogHelper::LogHelper(
    const LogType log_type,
    const PolicyLogger::Log::Severity log_severity,
    const int log_verbosity,
    const PolicyLogger::Log::Source log_source,
    std::string_view file,
    const int line)
    : log_type_(log_type),
      log_severity_(log_severity),
      log_verbosity_(log_verbosity),
      log_source_(log_source),
      file_(file),
      line_(line) {}

PolicyLogger::LogHelper::~LogHelper() {
  policy::PolicyLogger::GetInstance()->AddLog(PolicyLogger::Log(
      log_severity_, log_source_, message_buffer_.str(), file_, line_));
  StreamLog();
}

void PolicyLogger::LogHelper::StreamLog() const {
#if !DCHECK_IS_ON()
  if (log_type_ == LogHelper::LogType::kDLog) {
    return;
  }
#endif

  // Check for verbose logging.
  if (log_verbosity_ != policy::PolicyLogger::LogHelper::kNoVerboseLog) {
    LAZY_STREAM(
        ::logging::LogMessage(file_.data(), line_, -(log_verbosity_)).stream(),
        log_verbosity_ <=
            ::logging::GetVlogLevelHelper(file_.data(), file_.size()))
        << message_buffer_.str();
    return;
  }

  int log_severity_int = GetLogSeverityInt(log_severity_);

  LAZY_STREAM(
      ::logging::LogMessage(file_.data(), line_, log_severity_int).stream(),
      ::logging::ShouldCreateLogMessage(log_severity_int))
      << message_buffer_.str();
}

base::Value::Dict PolicyLogger::Log::GetAsDict() const {
  base::Value::Dict log_dict;
  log_dict.Set("message", base::EscapeForHTML(message_));
  log_dict.Set("logSeverity", GetLogSeverity(log_severity_));
  log_dict.Set("logSource", GetLogSourceValue(log_source_));
  log_dict.Set("location", GetLineURL(file_.data(), line_));
  log_dict.Set("timestamp", base::TimeFormatHTTP(timestamp_));
  return log_dict;
}

PolicyLogger::PolicyLogger() = default;
PolicyLogger::~PolicyLogger() = default;

void PolicyLogger::AddLog(PolicyLogger::Log&& new_log) {
    {
      base::AutoLock lock(lock_);

      // The logs deque size should not exceed `kMaxLogsSize`. Remove the first
      // log if the size is reached before adding the new log.
      if (logs_.size() == kMaxLogsSize) {
        logs_.pop_front();
      }

      logs_.emplace_back(std::move(new_log));
    }

    if (!is_log_deletion_scheduled_ && is_log_deletion_enabled_) {
      ScheduleOldLogsDeletion();
    }
}

void PolicyLogger::DeleteOldLogs() {
  // Delete older logs with lifetime `kTimeToLive` mins, set the flag and
  // reschedule the task.
  base::AutoLock lock(lock_);
  std::erase_if(logs_, IsLogExpired);

  if (logs_.size() > 0) {
    ScheduleOldLogsDeletion();
    return;
  }
  is_log_deletion_scheduled_ = false;
}

void PolicyLogger::ScheduleOldLogsDeletion() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PolicyLogger::DeleteOldLogs, weak_factory_.GetWeakPtr()),
      kTimeToLive);
  is_log_deletion_scheduled_ = true;
}

base::Value::List PolicyLogger::GetAsList() {
  base::Value::List all_logs_list;
  base::AutoLock lock(lock_);
  for (const Log& log : logs_) {
    all_logs_list.Append(log.GetAsDict());
  }
  return all_logs_list;
}

void PolicyLogger::EnableLogDeletion() {
  is_log_deletion_enabled_ = true;
}

size_t PolicyLogger::GetPolicyLogsSizeForTesting() {
  CHECK_IS_TEST();
  base::AutoLock lock(lock_);
  return logs_.size();
}

void PolicyLogger::ResetLoggerForTesting() {
  CHECK_IS_TEST();
  base::AutoLock lock(lock_);
  logs_.erase(logs_.begin(), logs_.end());
  is_log_deletion_scheduled_ = false;
  is_log_deletion_enabled_ = false;
}

}  // namespace policy
