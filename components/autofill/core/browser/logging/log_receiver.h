// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_RECEIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_RECEIVER_H_

#include "base/values.h"

namespace autofill {

// This interface is used by the password management code to receive and display
// logs about progress of actions like saving a password.
class LogReceiver {
 public:
  LogReceiver() = default;

  LogReceiver(const LogReceiver&) = delete;
  LogReceiver& operator=(const LogReceiver&) = delete;

  virtual ~LogReceiver() = default;

  virtual void LogEntry(const base::Value::Dict& entry) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_RECEIVER_H_
