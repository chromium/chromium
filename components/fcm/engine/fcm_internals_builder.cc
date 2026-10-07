// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fcm/engine/fcm_internals_builder.h"

#include <utility>

#include "base/time/default_clock.h"
#include "google_apis/gcm/engine/connection_factory_impl.h"
#include "google_apis/gcm/engine/gcm_store.h"
#include "google_apis/gcm/engine/mcs_client.h"
#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace fcm {

FcmInternalsBuilder::FcmInternalsBuilder() = default;
FcmInternalsBuilder::~FcmInternalsBuilder() = default;

base::Clock* FcmInternalsBuilder::GetClock() {
  return base::DefaultClock::GetInstance();
}

std::unique_ptr<gcm::MCSClient> FcmInternalsBuilder::BuildMCSClient(
    const std::string& version,
    base::Clock* clock,
    gcm::ConnectionFactory* connection_factory,
    gcm::GCMStore* gcm_store,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    gcm::GCMStatsRecorder* recorder) {
  return std::make_unique<gcm::MCSClient>(version, clock, connection_factory,
                                          gcm_store, std::move(io_task_runner),
                                          recorder);
}

std::unique_ptr<gcm::ConnectionFactory>
FcmInternalsBuilder::BuildConnectionFactory(
    const std::vector<GURL>& endpoints,
    const net::BackoffEntry::Policy& backoff_policy,
    base::RepeatingCallback<void(
        mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>)>
        get_socket_factory_callback,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    gcm::GCMStatsRecorder* recorder,
    network::NetworkConnectionTracker* network_connection_tracker) {
  return std::make_unique<gcm::ConnectionFactoryImpl>(
      endpoints, backoff_policy, std::move(get_socket_factory_callback),
      std::move(io_task_runner), recorder, network_connection_tracker);
}

}  // namespace fcm
