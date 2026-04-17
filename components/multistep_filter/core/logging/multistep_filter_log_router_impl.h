// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_LOG_ROUTER_IMPL_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_LOG_ROUTER_IMPL_H_

#include <atomic>
#include <cstddef>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_log_router.h"

namespace multistep_filter {

// Concrete implementation of MultistepFilterLogRouter that runs in the browser
// process. It manages a buffer of recent logs and notifies observers when new
// logs are added.
class MultistepFilterLogRouterImpl : public KeyedService,
                                     public MultistepFilterLogRouter {
 public:
  static constexpr size_t kMaxBufferSize = 1000;

  MultistepFilterLogRouterImpl();
  MultistepFilterLogRouterImpl(const MultistepFilterLogRouterImpl&) = delete;
  MultistepFilterLogRouterImpl& operator=(const MultistepFilterLogRouterImpl&) =
      delete;
  ~MultistepFilterLogRouterImpl() override;

  // Returns all currently buffered logs.
  base::ListValue GetBufferedLogs() const;

  // MultistepFilterLogRouter:
  void AddObserver(MultistepFilterLogRouter::Observer* observer) override;
  void RemoveObserver(MultistepFilterLogRouter::Observer* observer) override;
  bool IsLoggingEnabled() const override;
  void RouteLogMessage(LogEntry entry) override;

  // KeyedService:
  void Shutdown() override;

  // Returns a thread-safe callback that can be used to route logs from
  // background sequences.
  base::RepeatingCallback<void(LogEntry)> GetLogCallback();

 private:
  base::ObserverList<MultistepFilterLogRouter::Observer> observers_;
  base::circular_deque<LogEntry> buffer_;

  std::atomic<bool> is_logging_enabled_{false};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<MultistepFilterLogRouterImpl> weak_ptr_factory_{this};
};

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_MULTISTEP_FILTER_LOG_ROUTER_IMPL_H_
