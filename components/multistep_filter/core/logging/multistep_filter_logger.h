// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_LOGGER_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_LOGGER_H_

#include <string>
#include <string_view>
#include <utility>

#include "base/memory/stack_allocated.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_log_router.h"

namespace multistep_filter {

// A helper struct to enable adding key-value pairs to log entries via
// operator<<.
template <typename T>
struct LogDetail {
  std::string_view key;
  T value;
};

// A scoped helper for constructing a LogEntry and routing it to the
// MultistepFilterLogRouter when it goes out of scope.
class ScopedLogMessage {
  STACK_ALLOCATED();

 public:
  ScopedLogMessage(MultistepFilterLogRouter* logger,
                   int64_t navigation_id,
                   LogEventType type,
                   std::string_view source_etld_plus_1);
  ~ScopedLogMessage();

  // Access the details dictionary to add additional metadata to the log entry.
  base::DictValue& details() { return entry_.details; }

  template <typename T>
  ScopedLogMessage& operator<<(LogDetail<T> detail) {
    entry_.details.Set(detail.key, std::move(detail.value));
    return *this;
  }

 private:
  // `raw_ptr` is not needed because this class is STACK_ALLOCATED().
  MultistepFilterLogRouter* logger_;
  // The log entry being constructed and to be routed upon destruction.
  LogEntry entry_;
};

}  // namespace multistep_filter

// Convenience macro for logging messages with a MultistepFilterLogRouter.
// Only executes the ScopedLogMessage construction if logging is enabled.
// Uses a for loop to avoid dangling-else and empty-if-block warnings while
// supporting chaining of << operator calls.
// Evaluates 'logger' only once.
#define MULTISTEP_FILTER_LOG(logger, navigation_id, type, source_etld_plus_1) \
  for (multistep_filter::MultistepFilterLogRouter* multistep_logger_ =        \
           (logger);                                                          \
       multistep_logger_ && multistep_logger_->IsLoggingEnabled();            \
       multistep_logger_ = nullptr)                                           \
  multistep_filter::ScopedLogMessage(multistep_logger_, navigation_id, type,  \
                                     source_etld_plus_1)

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_LOGGER_H_
