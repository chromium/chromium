// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_BROWSER_ACTUATOR_SERVICE_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_BROWSER_ACTUATOR_SERVICE_H_

#include <string_view>

#include "components/keyed_service/core/keyed_service.h"

namespace browser_actuator {

class TransportChannel;
class TransportSession;

// Service that provides browser actuation capabilities.
class BrowserActuatorService : public KeyedService {
 public:
  ~BrowserActuatorService() override;

  BrowserActuatorService(const BrowserActuatorService&) = delete;
  BrowserActuatorService& operator=(const BrowserActuatorService&) = delete;

  // Whether the service is initialized and ready to execute actions.
  virtual bool IsInitialized() const = 0;

  // Exposes the transport channel.
  virtual TransportChannel* GetChannel() = 0;

  // Retrieves an existing session by ID or creates a new one if it does not
  // exist.
  virtual TransportSession* GetOrCreateSession(std::string_view session_id) = 0;

  // Gets an existing transport session for session_id, or nullptr if none
  // exists.
  virtual TransportSession* GetSession(std::string_view session_id) = 0;

 protected:
  BrowserActuatorService();
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_BROWSER_ACTUATOR_SERVICE_H_
