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

   private:
    LogSource log_source_;
    std::string message_;
    base::Location location_;
    base::Time timestamp_;
  };

  // Observer interface to be implemented by page handlers. Handler will need to
  // observe changes in the logs and notify the chrome://policy-logs tabs opened
  // to update UI.
  class Observer : public base::CheckedObserver {
   public:
    // Called to inform observers when logs are added or deleted.
    virtual void OnLogsChanged(const std::vector<Log>& logs) = 0;
  };

  static PolicyLogger* GetInstance();

  PolicyLogger();
  PolicyLogger(const PolicyLogger&) = delete;
  PolicyLogger& operator=(const PolicyLogger&) = delete;
  ~PolicyLogger();

  // Adds a new log and calls OnLogsChanged for observers.
  void AddLog(Log&& new_log);

  // Adds observer to the list and calls its OnLogsChanged.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Notifies all observers in observers list when logs are added or deleted.
  void NotifyChanged();

  // TODO(b/251799119): delete logs after an expiry period of ~30 minutes.

 private:
  std::vector<Log> logs_;
  base::ObserverList<Observer> observers_;

  // Checks if browser is running on Android.
  bool IsPolicyLoggingEnabled();
};

}  // namespace policy
#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_LOGGER_H_
