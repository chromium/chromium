// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_logger.h"
#include <utility>
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/policy/core/common/features.h"
#include "components/version_info/version_info.h"

namespace policy {

namespace {

// The base format for the Chromium Code Search URLs.
constexpr char kChromiumCSUrlFormat[] =
    "https://source.chromium.org/chromium/chromium/src/+/main:%s;l=%i;drc:%s";

// Gets the string value for the log source.
std::string GetLogSourceValue(
    const PolicyLogger::Log::Source log_source) {
  switch (log_source) {
    case PolicyLogger::Log::Source::kPolicyProcessing:
      return "Policy Processing";
    case PolicyLogger::Log::Source::kCBCMEnrollment:
      return "CBCM Enrollment";
    case PolicyLogger::Log::Source::kPlatformPolicy:
      return "Platform Policy";
    case PolicyLogger::Log::Source::kPolicyFetching:
      return "Policy Fetching";
    default:
      NOTREACHED();
  }
}

std::string GetLogSeverity(
    const PolicyLogger::Log::Severity log_severity) {
  switch (log_severity) {
    case PolicyLogger::Log::Severity::kInfo:
      return "INFO";
    case PolicyLogger::Log::Severity::kWarning:
      return "WARNING";
    case PolicyLogger::Log::Severity::kError:
      return "ERROR";
    default:
      NOTREACHED();
  }
}

// Constructs the URL for Chromium Code Search that points to the line of code
// that generated the log and the Chromium git revision hash.
std::string GetLineURL(const base::Location location) {
  std::string last_change = version_info::GetLastChange();

  // The substring separates the last change commit hash from the branch name on
  // the '-'.
  return base::StringPrintf(
      kChromiumCSUrlFormat, location.file_name(), location.line_number(),
      last_change.substr(0, last_change.find('-')).c_str());
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
    const PolicyLogger::Log::Source log_source,
    const base::Location location)
    : log_type_(log_type),
      log_severity_(log_severity),
      log_source_(log_source),
      location_(location) {}

PolicyLogger::LogHelper::~LogHelper() {
  if (PolicyLogger::GetInstance()->IsPolicyLoggingEnabled()) {
    policy::PolicyLogger::GetInstance()->AddLog(
        PolicyLogger::Log(log_severity_, log_source_,
                          message_buffer_.str(), location_));
  }
  StreamLog();
}

void PolicyLogger::LogHelper::StreamLog() {
  base::StringPiece filename(location_.file_name());
  size_t last_slash_pos = filename.find_last_of("\\/");
  if (last_slash_pos != base::StringPiece::npos) {
    filename.remove_prefix(last_slash_pos + 1);
  }

  if (log_type_ == PolicyLogger::LogHelper::LogType::kLog &&
      log_severity_ == PolicyLogger::Log::Severity::kInfo) {
    LOG(INFO) << ":" << filename << "(" << location_.line_number() << ") "
              << message_buffer_.str();
  } else if (log_type_ == PolicyLogger::LogHelper::LogType::kLog &&
             log_severity_ == PolicyLogger::Log::Severity::kWarning) {
    LOG(WARNING) << ":" << filename << "(" << location_.line_number() << ") "
                 << message_buffer_.str();
  } else if (log_type_ == PolicyLogger::LogHelper::LogType::kLog &&
             log_severity_ == PolicyLogger::Log::Severity::kError) {
    LOG(ERROR) << ":" << filename << "(" << location_.line_number() << ") "
               << message_buffer_.str();
  }
}

base::Value::Dict PolicyLogger::Log::GetAsDict() const {
  base::Value::Dict log_dict;
  log_dict.Set("message", message_);
  log_dict.Set("log_severity", GetLogSeverity(log_severity_));
  log_dict.Set("log_source", GetLogSourceValue(log_source_));
  log_dict.Set("location", GetLineURL(location_));
  log_dict.Set("timestamp", base::TimeFormatHTTP(timestamp_));
  return log_dict;
}

PolicyLogger::PolicyLogger() = default;
PolicyLogger::~PolicyLogger() = default;

void PolicyLogger::AddLog(PolicyLogger::Log&& new_log) {
  if (IsPolicyLoggingEnabled()) {
    logs_.emplace_back(std::move(new_log));
  }
}

base::Value::List PolicyLogger::GetAsList() const {
  base::Value::List all_logs_list;
  for (const Log& log : logs_) {
    all_logs_list.Append(log.GetAsDict());
  }
  return all_logs_list;
}

bool PolicyLogger::IsPolicyLoggingEnabled() {
#if BUILDFLAG(IS_ANDROID)
  return base::FeatureList::IsEnabled(policy::features::kPolicyLogsPageAndroid);
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

int PolicyLogger::GetPolicyLogsSizeForTesting() {
  return logs_.size();
}

}  // namespace policy
