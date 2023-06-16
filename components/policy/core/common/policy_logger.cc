// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_logger.h"

#include <utility>

#include "base/functional/bind.h"
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
    default:
      NOTREACHED();
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
    default:
      NOTREACHED();
  }
}

// Constructs the URL for Chromium Code Search that points to the line of code
// that generated the log and the Chromium git revision hash.
std::string GetLineURL(const base::Location location) {
  std::string last_change(version_info::GetLastChange());

  // The substring separates the last change commit hash from the branch name on
  // the '-'.
  return base::StringPrintf(
      kChromiumCSUrlFormat, location.file_name(), location.line_number(),
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
                       const base::Location location)
    : log_severity_(log_severity),
      log_source_(log_source),
      message_(message),
      location_(location),
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
    const base::Location location)
    : log_type_(log_type),
      log_severity_(log_severity),
      log_verbosity_(log_verbosity),
      log_source_(log_source),
      location_(location) {}

PolicyLogger::LogHelper::~LogHelper() {
  if (PolicyLogger::GetInstance()->IsPolicyLoggingEnabled()) {
    policy::PolicyLogger::GetInstance()->AddLog(PolicyLogger::Log(
        log_severity_, log_source_, message_buffer_.str(), location_));
  }
  StreamLog();
}

void PolicyLogger::LogHelper::StreamLog() const {
  base::StringPiece filename(location_.file_name());
  std::ostringstream message;

  // Create the message to be logged to the terminal.
  // The `:` is needed as the location of the message logged to the terminal
  // would be policy_logger.cc (from one the lines below), but we need to see
  // the original location where xLOG_POLICY was called.
  message << ":" << filename << "(" << location_.line_number() << ") "
          << message_buffer_.str();

  size_t last_slash_pos = filename.find_last_of("\\/");
  if (last_slash_pos != base::StringPiece::npos) {
    filename.remove_prefix(last_slash_pos + 1);
  }

  // Check for verbose logging.
  if (log_verbosity_ != policy::PolicyLogger::LogHelper::kNoVerboseLog) {
    if (log_type_ == LogHelper::LogType::kDLog) {
      DVLOG(log_verbosity_) << message.str();
      return;
    }
    VLOG(log_verbosity_) << message.str();
    return;
  }

  // Non-verbose logging.
  if (log_severity_ == PolicyLogger::Log::Severity::kInfo) {
    if (log_type_ == PolicyLogger::LogHelper::LogType::kLog) {
      LOG(INFO) << message.str();
    } else if (log_type_ == PolicyLogger::LogHelper::LogType::kDLog) {
      DLOG(INFO) << message.str();
    }
  } else if (log_severity_ == PolicyLogger::Log::Severity::kWarning) {
    if (log_type_ == PolicyLogger::LogHelper::LogType::kLog) {
      LOG(WARNING) << message.str();
    } else if (log_type_ == PolicyLogger::LogHelper::LogType::kDLog) {
      DLOG(WARNING) << message.str();
    }
  } else if (log_severity_ == PolicyLogger::Log::Severity::kError) {
    if (log_type_ == PolicyLogger::LogHelper::LogType::kLog) {
      LOG(ERROR) << message.str();
    } else if (log_type_ == PolicyLogger::LogHelper::LogType::kDLog) {
      DLOG(ERROR) << message.str();
    }
  }
}

base::Value::Dict PolicyLogger::Log::GetAsDict() const {
  base::Value::Dict log_dict;
  log_dict.Set("message", base::EscapeForHTML(message_));
  log_dict.Set("log_severity", GetLogSeverity(log_severity_));
  log_dict.Set("log_source", GetLogSourceValue(log_source_));
  log_dict.Set("location", GetLineURL(location_));
  log_dict.Set("timestamp", base::TimeFormatHTTP(timestamp_));
  return log_dict;
}

PolicyLogger::PolicyLogger() = default;

PolicyLogger::~PolicyLogger() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(logs_list_sequence_checker_);
}

void PolicyLogger::AddLog(PolicyLogger::Log&& new_log) {
  if (IsPolicyLoggingEnabled()) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(logs_list_sequence_checker_);
    logs_.emplace_back(std::move(new_log));

    if (!is_log_deletion_scheduled_ && is_log_deletion_enabled_) {
      ScheduleOldLogsDeletion();
    }
  }
}

void PolicyLogger::DeleteOldLogs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(logs_list_sequence_checker_);
  // Delete older logs with lifetime `kTimeToLive` mins, set the flag and
  // reschedule the task.
  logs_.erase(std::remove_if(logs_.begin(), logs_.end(), IsLogExpired),
              logs_.end());
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

base::Value::List PolicyLogger::GetAsList() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(logs_list_sequence_checker_);
  base::Value::List all_logs_list;
  for (const Log& log : logs_) {
    all_logs_list.Append(log.GetAsDict());
  }
  return all_logs_list;
}

bool PolicyLogger::IsPolicyLoggingEnabled() const {
#if BUILDFLAG(IS_ANDROID)
  return base::FeatureList::IsEnabled(policy::features::kPolicyLogsPageAndroid);
#elif BUILDFLAG(IS_IOS)
  return base::FeatureList::IsEnabled(policy::features::kPolicyLogsPageIOS);
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

void PolicyLogger::EnableLogDeletion() {
  is_log_deletion_enabled_ = true;
}

size_t PolicyLogger::GetPolicyLogsSizeForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(logs_list_sequence_checker_);
  return logs_.size();
}

void PolicyLogger::ResetLoggerAfterTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(logs_list_sequence_checker_);
  logs_.erase(logs_.begin(), logs_.end());
  is_log_deletion_scheduled_ = false;
  is_log_deletion_enabled_ = false;
}

}  // namespace policy
