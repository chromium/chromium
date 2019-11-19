// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "components/autofill/core/browser/logging/log_buffer_submitter.h"

namespace base {
class Value;
}

namespace autofill {

class LogRouter;

// This interface is used by the password management code to receive and display
// logs about progress of actions like saving a password.
class LogManager {
 public:
  virtual ~LogManager() = default;

  // This method is called by a LogRouter, after the LogManager registers with
  // one. If |router_can_be_used| is true, logs can be sent to LogRouter after
  // the return from OnLogRouterAvailabilityChanged and will reach at least one
  // LogReceiver instance. If |router_can_be_used| is false, no logs should be
  // sent to the LogRouter.
  virtual void OnLogRouterAvailabilityChanged(bool router_can_be_used) = 0;

  // The owner of the LogManager can call this to start or end suspending the
  // logging, by setting |suspended| to true or false, respectively.
  virtual void SetSuspended(bool suspended) = 0;

  // Forward |text| for display to the LogRouter (if registered with one).
  virtual void LogTextMessage(const std::string& text) const = 0;

  // Forward a DOM structured log entry to the LogRouter (if registered with
  // one).
  virtual void LogEntry(base::Value&& entry) const = 0;

  // Returns true if logs recorded via LogTextMessage will be displayed, and
  // false otherwise.
  virtual bool IsLoggingActive() const = 0;

  // Returns the production code implementation of LogManager. If |log_router|
  // is null, the manager will do nothing. |notification_callback| will be
  // called every time the activity status of logging changes.
  static std::unique_ptr<LogManager> Create(
      LogRouter* log_router,
      base::Closure notification_callback);

  // This is the preferred way to submitting log entries.
  virtual LogBufferSubmitter Log() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_MANAGER_H_
