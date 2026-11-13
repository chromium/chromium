// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/browser_actuator_service_impl.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/sequence_checker.h"
#include "components/browser_actuator/public/browser_actuator_task_client.h"

namespace browser_actuator {

BrowserActuatorServiceImpl::BrowserActuatorServiceImpl() = default;

BrowserActuatorServiceImpl::~BrowserActuatorServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Shutdown();
}

void BrowserActuatorServiceImpl::SetTaskClientFactory(
    TaskClientFactoryCallback task_client_factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_shut_down_);
  task_client_factory_ = std::move(task_client_factory);
}

void BrowserActuatorServiceImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_shut_down_ = true;
  task_client_factory_.Reset();
}

bool BrowserActuatorServiceImpl::IsInitialized() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

std::unique_ptr<BrowserActuatorTaskClient>
BrowserActuatorServiceImpl::CreateTaskClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!task_client_factory_) {
    return nullptr;
  }
  return task_client_factory_.Run();
}

}  // namespace browser_actuator
