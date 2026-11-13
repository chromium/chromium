// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_BROWSER_ACTUATOR_SERVICE_IMPL_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_BROWSER_ACTUATOR_SERVICE_IMPL_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/browser_actuator/public/browser_actuator_service.h"

namespace browser_actuator {

class BrowserActuatorTaskClient;

class BrowserActuatorServiceImpl : public BrowserActuatorService {
 public:
  using TaskClientFactoryCallback =
      base::RepeatingCallback<std::unique_ptr<BrowserActuatorTaskClient>()>;

  BrowserActuatorServiceImpl();
  ~BrowserActuatorServiceImpl() override;

  BrowserActuatorServiceImpl(const BrowserActuatorServiceImpl&) = delete;
  BrowserActuatorServiceImpl& operator=(const BrowserActuatorServiceImpl&) =
      delete;

  void SetTaskClientFactory(TaskClientFactoryCallback task_client_factory);

  // KeyedService:
  void Shutdown() override;

  // BrowserActuatorService:
  bool IsInitialized() const override;
  std::unique_ptr<BrowserActuatorTaskClient> CreateTaskClient() override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  bool is_shut_down_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  TaskClientFactoryCallback task_client_factory_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_BROWSER_ACTUATOR_SERVICE_IMPL_H_
