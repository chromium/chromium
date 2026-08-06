// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_SESSION_REGISTRY_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_SESSION_REGISTRY_H_

#include <string_view>

#include "base/observer_list_types.h"

namespace browser_actuator {

class TransportSession;

// Manages active transport sessions.
class TransportSessionRegistry {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when a new session is registered in the registry.
    virtual void OnSessionRegistered(TransportSession* session) {}
  };

  virtual ~TransportSessionRegistry() = default;

  // Retrieves an existing session by ID, returning nullptr if it doesn't exist.
  virtual TransportSession* GetSession(std::string_view session_id) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Retrieves an existing session by ID or creates a new one if it does not
  // exist. Returns nullptr if session creation fails (e.g. max capacity
  // reached).
  virtual TransportSession* GetOrCreateSession(std::string_view session_id) = 0;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_SESSION_REGISTRY_H_
