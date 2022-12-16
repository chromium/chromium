// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_logger.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/version_info/version_info.h"

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

PolicyLogger::PolicyLogger() = default;

PolicyLogger::~PolicyLogger() = default;

void PolicyLogger::AddLog(PolicyLogger::Log&& new_log) {
  if (IsPolicyLoggingEnabled()) {
    logs_.emplace_back(std::move(new_log));
    NotifyChanged();
  }
}

void PolicyLogger::AddObserver(Observer* observer) {
  if (IsPolicyLoggingEnabled()) {
    observers_.AddObserver(observer);
    observer->OnLogsChanged(logs_);
  }
}

void PolicyLogger::RemoveObserver(Observer* observer) {
  if (IsPolicyLoggingEnabled()) {
    observers_.RemoveObserver(observer);
  }
}

void PolicyLogger::NotifyChanged() {
  for (Observer& observer : observers_) {
    observer.OnLogsChanged(logs_);
  }
}

bool PolicyLogger::IsPolicyLoggingEnabled() {
#if BUILDFLAG(IS_ANDROID)
  return true;
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace policy
