// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_BROWSER_ACTUATOR_SERVICE_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_BROWSER_ACTUATOR_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace browser_actuator {

// Service that provides browser actuation capabilities.
class BrowserActuatorService : public KeyedService {
 public:
  ~BrowserActuatorService() override;

  BrowserActuatorService(const BrowserActuatorService&) = delete;
  BrowserActuatorService& operator=(const BrowserActuatorService&) = delete;

  // Whether the service is initialized and ready to execute actions.
  virtual bool IsInitialized() const = 0;

 protected:
  BrowserActuatorService();
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_BROWSER_ACTUATOR_SERVICE_H_
