// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_LOG_ROUTER_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_LOG_ROUTER_H_

#include "base/observer_list_types.h"

namespace multistep_filter {

struct LogEntry;

// Abstract interface for the log router. This interface is used by the core
// feature logic to route log messages.
class MultistepFilterLogRouter {
 public:
  // Observer interface to listen for log routing events and lifecycle changes.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when a new log entry is added to the router.
    virtual void OnLogEntryAdded(const LogEntry& entry) = 0;

    // Called when the log router is shutting down. Observers should remove
    // themselves and drop any references to the router.
    virtual void OnLogRouterShutdown() = 0;
  };

  virtual ~MultistepFilterLogRouter() = default;

  // Adds or removes an observer. These methods must be called on the same
  // sequence that created the object.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns whether logging is currently active for the session.
  // This method is thread-safe and can be called from any sequence.
  virtual bool IsLoggingEnabled() const = 0;

  // Routes a completed log entry to the appropriate destination.
  virtual void RouteLogMessage(LogEntry entry) = 0;
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_LOG_ROUTER_H_
