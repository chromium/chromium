// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_MANAGER_H_

#include <concepts>
#include <memory>

#include "base/functional/callback.h"
#include "base/types/pass_key.h"
#include "components/autofill/core/browser/logging/log_buffer_submitter.h"
#include "components/autofill/core/common/logging/log_macros.h"

namespace base {
class Value;
}

namespace autofill {

class LogRouter;
class RoutingLogManager;
class BufferingLogManager;

// This interface is used by the password management code to receive and display
// logs about progress of actions like saving a password.
class LogManager {
 public:
  // Returns the production code implementation of LogManager. If |log_router|
  // is null, the manager will do nothing. |notification_callback| will be
  // called every time the activity status of logging changes.
  static std::unique_ptr<RoutingLogManager> Create(
      LogRouter* log_router,
      base::RepeatingClosure notification_callback);

  static std::unique_ptr<BufferingLogManager> CreateBuffering();

  virtual ~LogManager() = default;

  // Returns whether logs recorded via `Log()` will be displayed.
  virtual bool IsLoggingActive() const = 0;

  // This is the preferred way to submitting log entries.
  virtual LogBufferSubmitter Log() = 0;

  // Emits the log entry.
  virtual void ProcessLog(base::Value::Dict node,
                          base::PassKey<LogBufferSubmitter>) = 0;
};

// This LogManager subclass can be connected to a LogRouter, which in turn
// passes logs to LogReceivers.
class RoutingLogManager : public LogManager {
 public:
  // This method is called by a LogRouter, after the LogManager registers with
  // one. If |router_can_be_used| is true, logs can be sent to LogRouter after
  // the return from OnLogRouterAvailabilityChanged and will reach at least one
  // LogReceiver instance. If |router_can_be_used| is false, no logs should be
  // sent to the LogRouter.
  virtual void OnLogRouterAvailabilityChanged(bool router_can_be_used) = 0;

  // The owner of the LogManager can call this to start or end suspending the
  // logging, by setting |suspended| to true or false, respectively.
  virtual void SetSuspended(bool suspended) = 0;
};

// This LogManager subclass stores logs in a buffer, which can eventually be
// flushed to another LogManager. It facilitates logging across in sequences
// outside of the main thread in a thread-safe way.
class BufferingLogManager : public LogManager {
 public:
  // Passes the buffering log since the last Flush() to `destination`.
  virtual void Flush(LogManager& destination) = 0;
};

inline LogBuffer::IsActive IsLoggingActive(LogManager* log_manager) {
  return LogBuffer::IsActive(log_manager && log_manager->IsLoggingActive());
}

namespace internal {

// Traits for LOG_AF() macro for `LogManager*`.
template <std::convertible_to<const LogManager*> T>
struct LoggerTraits<T> {
  static bool active(const LogManager* log_manager) {
    return log_manager && log_manager->IsLoggingActive();
  }

  static LogBufferSubmitter get_stream(LogManager* log_manager) {
    return log_manager->Log();
  }
};

// Traits for LOG_AF() macro for `LogManager&`.
template <std::convertible_to<const LogManager&> T>
struct LoggerTraits<T> {
  static bool active(const LogManager& log_manager) {
    return log_manager.IsLoggingActive();
  }

  static LogBufferSubmitter get_stream(LogManager& log_manager) {
    return log_manager.Log();
  }
};

}  // namespace internal

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_MANAGER_H_
