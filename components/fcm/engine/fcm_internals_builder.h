// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FCM_ENGINE_FCM_INTERNALS_BUILDER_H_
#define COMPONENTS_FCM_ENGINE_FCM_INTERNALS_BUILDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom-forward.h"
#include "url/gurl.h"

namespace base {
class Clock;
}  // namespace base

namespace gcm {
class ConnectionFactory;
class GCMStatsRecorder;
class GCMStore;
class MCSClient;
}  // namespace gcm

namespace network {
class NetworkConnectionTracker;
}  // namespace network

namespace fcm {

// Helper class for building FCM/GCM engine internals (MCSClient,
// ConnectionFactory, Clock). Allows unit tests to inject fake versions.
// Constructed and used on the IO sequence (`io_task_runner`).
class FcmInternalsBuilder {
 public:
  FcmInternalsBuilder();
  FcmInternalsBuilder(const FcmInternalsBuilder&) = delete;
  FcmInternalsBuilder& operator=(const FcmInternalsBuilder&) = delete;
  virtual ~FcmInternalsBuilder();

  virtual base::Clock* GetClock();
  virtual std::unique_ptr<gcm::MCSClient> BuildMCSClient(
      const std::string& version,
      base::Clock* clock,
      gcm::ConnectionFactory* connection_factory,
      gcm::GCMStore* gcm_store,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      gcm::GCMStatsRecorder* recorder);
  virtual std::unique_ptr<gcm::ConnectionFactory> BuildConnectionFactory(
      const std::vector<GURL>& endpoints,
      const net::BackoffEntry::Policy& backoff_policy,
      base::RepeatingCallback<void(
          mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
          get_socket_factory_callback,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      gcm::GCMStatsRecorder* recorder,
      network::NetworkConnectionTracker* network_connection_tracker);
};

}  // namespace fcm

#endif  // COMPONENTS_FCM_ENGINE_FCM_INTERNALS_BUILDER_H_
