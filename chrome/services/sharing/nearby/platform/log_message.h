// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_LOG_MESSAGE_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_LOG_MESSAGE_H_

#include "base/logging.h"
#include "third_party/nearby/src/internal/platform/implementation/log_message.h"

namespace nearby::chrome {

// Concrete LogMessage implementation
class LogMessage : public api::LogMessage {
 public:
  LogMessage(const char* file, int line, Severity severity);
  ~LogMessage() override;

  LogMessage(const LogMessage&) = delete;
  LogMessage& operator=(const LogMessage&) = delete;

  void Print(const char* format, ...) override;

  std::ostream& Stream() override;

 private:
  logging::LogMessage log_message_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_LOG_MESSAGE_H_
