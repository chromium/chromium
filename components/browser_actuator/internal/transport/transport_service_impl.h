// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_TRANSPORT_SERVICE_IMPL_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_TRANSPORT_SERVICE_IMPL_H_

#include <memory>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "components/browser_actuator/public/transport/transport_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace browser_actuator::transport {

class SessionStreamRecorderFactory;
class TransportChannelImpl;

// What the embedder decides for the transport, so that the transport does not
// depend on any of its clients' features or flags.
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

class TransportServiceImpl : public TransportService,
                             public signin::IdentityManager::Observer {
 public:
  // `identity_manager` must outlive this service; `TransportServiceFactory`
  // depends on `IdentityManagerFactory` to guarantee that.
  TransportServiceImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      TransportServiceConfig config);
  ~TransportServiceImpl() override;

  TransportServiceImpl(const TransportServiceImpl&) = delete;
  TransportServiceImpl& operator=(const TransportServiceImpl&) = delete;

  // KeyedService:
  void Shutdown() override;

  // TransportService:
  TransportChannel* GetChannel() override;
  TransportSession* CreateSession(std::string_view session_id,
                                  InitIngressSource ingress_source) override;
  TransportSession* GetSession(std::string_view session_id) override;
  SessionStreamRecorderFactory* GetStreamRecorderFactory() override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 private:
  void ResetForAccountChange();
  void TearDownChannel();

  SEQUENCE_CHECKER(sequence_checker_);

  // Declared before `channel_` on purpose. C++ destroys members in reverse
  // declaration order, so `channel_` -- and the session registry it owns, which
  // holds a raw observer pointer to `session_stream_recorder_factory_` -- is
  // torn down first.
  std::unique_ptr<SessionStreamRecorderFactory>
      session_stream_recorder_factory_;

  std::unique_ptr<TransportChannelImpl> channel_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
};

}  // namespace browser_actuator::transport

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_TRANSPORT_SERVICE_IMPL_H_
