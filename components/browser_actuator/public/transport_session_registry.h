// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_SESSION_REGISTRY_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_SESSION_REGISTRY_H_

#include <string_view>

namespace browser_actuator {

class TransportSession;

// Manages active transport sessions.
class TransportSessionRegistry {
 public:
  virtual ~TransportSessionRegistry() = default;

  // Retrieves an existing session by ID, returning nullptr if it doesn't exist.
  virtual TransportSession* GetSession(std::string_view session_id) = 0;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_SESSION_REGISTRY_H_
