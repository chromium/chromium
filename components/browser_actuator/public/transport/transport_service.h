// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_TRANSPORT_SERVICE_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_TRANSPORT_SERVICE_H_

#include <string_view>

#include "components/keyed_service/core/keyed_service.h"

namespace browser_actuator::transport {

class SessionStreamRecorderFactory;
class TransportChannel;
class TransportSession;

// The per-profile owner of the bi-directional transport between Chrome and the
// server: one `TransportChannel` (the downstream stream plus upstream requests)
// and the sessions multiplexed over it.
//
// Features use the transport by registering a `TransportHandlerFactory` for
// their `PayloadType` with `GetChannel()->GetHandlerFactoryRegistry()`, and
// unregistering it before they shut down. The service knows nothing about its
// clients.
//
// Lives on the UI thread. Created by `TransportServiceFactory` in //chrome.
class TransportService : public KeyedService {
 public:
  ~TransportService() override;

  TransportService(const TransportService&) = delete;
  TransportService& operator=(const TransportService&) = delete;

  // Returns the channel, or nullptr if the channel is disabled by
  // `kBrowserActuatorChannelEnabled`.
  virtual TransportChannel* GetChannel() = 0;

  // Creates a new session for `session_id`. Returns nullptr if there is no
  // channel, the session already exists, or the session cannot be created.
  virtual TransportSession* CreateSession(std::string_view session_id) = 0;

  // Returns the session for `session_id`, or nullptr if there is none.
  virtual TransportSession* GetSession(std::string_view session_id) = 0;

  // Returns the recorder of every session's messages, used by
  // chrome://browser-actuator-internals, or nullptr if recording is off.
  virtual SessionStreamRecorderFactory* GetStreamRecorderFactory() = 0;

 protected:
  TransportService();
};

}  // namespace browser_actuator::transport

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_TRANSPORT_SERVICE_H_
