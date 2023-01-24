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

namespace {

// The base format for the Chromium Code Search URLs.
constexpr char kChromiumCSUrlFormat[] =
    "https://source.chromium.org/chromium/chromium/src/+/main:%s;l=%i;drc:%s";

// Gets the string value for the log source.
std::string GetLogSourceValue(
    const policy::PolicyLogger::Log::LogSource log_source) {
  switch (log_source) {
    case policy::PolicyLogger::Log::LogSource::kCBCMEnrollment:
      return "CBCM Enrollment";
    case policy::PolicyLogger::Log::LogSource::kPlatformPolicy:
      return "Platform Policy";
    case policy::PolicyLogger::Log::LogSource::kPolicyFetching:
      return "Policy Fetching";
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

namespace policy {

PolicyLogger::Log::Log(const LogSource log_source,
                       const std::string& message,
                       const base::Location location)
    : log_source_(log_source),
      message_(message),
      location_(location),
      timestamp_(base::Time::Now()) {}
PolicyLogger* PolicyLogger::GetInstance() {
  static base::NoDestructor<PolicyLogger> instance;
  return instance.get();
}

PolicyLogger::LogHelper::LogHelper(
    const PolicyLogger::Log::LogSource log_source,
    const base::Location location)
    : log_source_(log_source), location_(location) {}

PolicyLogger::LogHelper::~LogHelper() {
  DCHECK(PolicyLogger::GetInstance()->IsPolicyLoggingEnabled());
  policy::PolicyLogger::GetInstance()->AddLog(PolicyLogger::Log(
      this->log_source_, this->message_buffer_.str(), this->location_));
}

base::Value PolicyLogger::Log::GetAsValue() const {
  base::Value log_value(base::Value::Type::DICT);
  log_value.SetStringPath("message", message_);
  log_value.SetStringPath("log_source", GetLogSourceValue(log_source_));
  log_value.SetStringPath("location", GetLineURL(location_));
  log_value.SetStringPath("timestamp", base::TimeFormatHTTP(timestamp_));
  return log_value;
}

PolicyLogger::PolicyLogger() = default;
PolicyLogger::~PolicyLogger() = default;

void PolicyLogger::AddLog(PolicyLogger::Log&& new_log) {
  if (IsPolicyLoggingEnabled()) {
    logs_.emplace_back(std::move(new_log));
  }
}

base::Value PolicyLogger::GetAsValue() const {
  base::Value all_logs_value(base::Value::Type::LIST);
  for (const Log& log : logs_) {
    all_logs_value.Append(log.GetAsValue());
  }
  return all_logs_value;
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
