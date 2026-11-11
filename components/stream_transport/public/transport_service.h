// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STREAM_TRANSPORT_PUBLIC_TRANSPORT_SERVICE_H_
#define COMPONENTS_STREAM_TRANSPORT_PUBLIC_TRANSPORT_SERVICE_H_

#include <string_view>

#include "components/keyed_service/core/keyed_service.h"
#include "components/stream_transport/public/common.h"
#include "url/gurl.h"

namespace stream_transport {

class SessionStreamRecorderFactory;
class TransportChannel;
class TransportSession;

// Embedder-supplied configuration for `TransportService` (passed to
// `CreateTransportService()` in `//components/stream_transport/public:factory`)
// so the transport does not depend on any of its clients' features or flags.
struct TransportServiceConfig {
  TransportServiceConfig();
  TransportServiceConfig(const TransportServiceConfig&);
  TransportServiceConfig& operator=(const TransportServiceConfig&);
  TransportServiceConfig(TransportServiceConfig&&);
  TransportServiceConfig& operator=(TransportServiceConfig&&);
  ~TransportServiceConfig();

  // The WatchSessions endpoint that the downstream stream connects to.
  GURL watch_sessions_endpoint;

  // The SendSessionMessage endpoint that upstream messages are POSTed to.
  GURL send_session_message_endpoint;

  // Whether to record every session's messages for
  // chrome://browser-actuator-internals. Recording costs memory, so it should
  // only be on while that page is enabled.
  bool record_sessions = false;
};

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
  virtual TransportSession* CreateSession(std::string_view session_id,
                                          InitIngressSource ingress_source) = 0;

  // Returns the session for `session_id`, or nullptr if there is none.
  virtual TransportSession* GetSession(std::string_view session_id) = 0;

  // Returns the recorder of every session's messages, used by
  // chrome://browser-actuator-internals, or nullptr if recording is off.
  virtual SessionStreamRecorderFactory* GetStreamRecorderFactory() = 0;

 protected:
  TransportService();
};

}  // namespace stream_transport

#endif  // COMPONENTS_STREAM_TRANSPORT_PUBLIC_TRANSPORT_SERVICE_H_
