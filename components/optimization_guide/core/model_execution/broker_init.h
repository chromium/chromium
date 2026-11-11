// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_BROKER_INIT_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_BROKER_INIT_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "services/on_device_model/public/cpp/capabilities.h"

namespace optimization_guide {

// Queues tasks that depend on initial broker setup until initialization
// completes, then runs queued and subsequent tasks with the device's
// `Capabilities`.
class BrokerInit final {
 public:
  using TaskCallback =
      base::OnceCallback<void(const on_device_model::Capabilities&)>;

  BrokerInit();
  ~BrokerInit();

  BrokerInit(const BrokerInit&) = delete;
  BrokerInit& operator=(const BrokerInit&) = delete;

  bool is_complete() const { return capabilities_.has_value(); }

  // Runs `task` immediately if initialization has completed, or queues it to
  // run when `Complete()` is called.
  void QueueTask(TaskCallback task);

  // Marks initialization complete with `capabilities` and runs any queued
  // tasks. Subsequent calls are a no-op.
  void Complete(on_device_model::Capabilities capabilities);

 private:
  // The device's capabilities once initialization has completed; being set
  // implies initialization is complete.
  std::optional<on_device_model::Capabilities> capabilities_;
  std::vector<TaskCallback> tasks_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_BROKER_INIT_H_
