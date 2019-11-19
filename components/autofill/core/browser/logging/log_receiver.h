// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_RECEIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_RECEIVER_H_

#include "base/macros.h"
#include "base/values.h"

namespace autofill {

// This interface is used by the password management code to receive and display
// logs about progress of actions like saving a password.
class LogReceiver {
 public:
  LogReceiver() {}
  virtual ~LogReceiver() {}

  virtual void LogEntry(const base::Value& entry) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(LogReceiver);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_RECEIVER_H_
