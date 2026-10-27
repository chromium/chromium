// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_TRANSPORT_SESSION_REGISTRY_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_TRANSPORT_SESSION_REGISTRY_H_

#include <string_view>

#include "base/observer_list_types.h"

namespace browser_actuator::transport {

class TransportSession;

// Manages active transport sessions.
class TransportSessionRegistry {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when a new session is registered in the registry.
    virtual void OnSessionRegistered(TransportSession* session) {}

    // Called when a session is removed from the registry.
    virtual void OnSessionRemoved(std::string_view session_id) {}
  };

  virtual ~TransportSessionRegistry() = default;

  // Retrieves an existing session by ID, returning nullptr if it doesn't exist.
  virtual TransportSession* GetSession(std::string_view session_id) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Creates a new session by ID, returning nullptr if the session already
  // exists or if session creation fails (e.g. max capacity reached).
  virtual TransportSession* CreateSession(std::string_view session_id) = 0;

  // Destroys the session identified by `session_id` if it exists.
  virtual void DestroySession(std::string_view session_id) = 0;
};

}  // namespace browser_actuator::transport

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_TRANSPORT_SESSION_REGISTRY_H_
