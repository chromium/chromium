// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_LOG_ROUTER_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_LOG_ROUTER_H_

namespace multistep_filter {

struct LogEntry;

// Abstract interface for the log router. This interface is used by the core
// feature logic to route log messages.
class MultistepFilterLogRouter {
 public:
  virtual ~MultistepFilterLogRouter() = default;

  // Returns whether logging is currently active for the session.
  virtual bool IsLoggingEnabled() const = 0;

  // Routes a completed log entry to the appropriate destination.
  virtual void RouteLogMessage(LogEntry entry) = 0;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_LOG_ROUTER_H_
