// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/broker_init.h"

#include <utility>

#include "base/functional/callback.h"

namespace optimization_guide {

BrokerInit::BrokerInit() = default;
BrokerInit::~BrokerInit() = default;

void BrokerInit::QueueTask(TaskCallback task) {
  if (is_complete()) {
    std::move(task).Run(*capabilities_);
    return;
  }
  tasks_.push_back(std::move(task));
}

void BrokerInit::Complete(on_device_model::Capabilities capabilities) {
  if (is_complete()) {
    return;
  }
  capabilities_ = capabilities;
  for (auto& task : std::exchange(tasks_, {})) {
    std::move(task).Run(capabilities);
  }
}

}  // namespace optimization_guide
