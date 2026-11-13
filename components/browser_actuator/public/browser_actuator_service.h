// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_BROWSER_ACTUATOR_SERVICE_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_BROWSER_ACTUATOR_SERVICE_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"

namespace browser_actuator {

class BrowserActuatorTaskClient;

// Service that provides browser actuation capabilities.
//
// The transport it sends and receives actions over is a KeyedService of its
// own: features that only need the transport use
// `stream_transport::TransportService` (`TransportServiceFactory` in
// //chrome) directly.
class BrowserActuatorService : public KeyedService {
 public:
  ~BrowserActuatorService() override;

  BrowserActuatorService(const BrowserActuatorService&) = delete;
  BrowserActuatorService& operator=(const BrowserActuatorService&) = delete;

  // Whether the service is initialized and ready to execute actions.
  virtual bool IsInitialized() const = 0;

  // Creates a new task client for initiating and observing a browser actuation
  // task. Returns `nullptr` if task execution is not available.
  //
  // Returned clients are single-use on the UI sequence and must be destroyed
  // before `Profile` destruction (`Shutdown()` detaches active tasks without
  // `Observer` callbacks).
  virtual std::unique_ptr<BrowserActuatorTaskClient> CreateTaskClient() = 0;

 protected:
  BrowserActuatorService();
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_BROWSER_ACTUATOR_SERVICE_H_
