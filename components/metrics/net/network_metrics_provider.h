// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_NET_NETWORK_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_NET_NETWORK_METRICS_PROVIDER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "components/metrics/metrics_provider.h"
#include "net/base/network_interfaces.h"
#include "net/nqe/effective_connection_type.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

SystemProfileProto::Network::EffectiveConnectionType
ConvertEffectiveConnectionType(
    net::EffectiveConnectionType effective_connection_type);

// Registers as observer with network::NetworkConnectionTracker and keeps track
// of the network environment.
class NetworkMetricsProvider
    : public MetricsProvider,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  // Class that provides |this| with the network quality estimator.
  class NetworkQualityEstimatorProvider {
   public:
    NetworkQualityEstimatorProvider(const NetworkQualityEstimatorProvider&) =
        delete;
    NetworkQualityEstimatorProvider& operator=(
        const NetworkQualityEstimatorProvider&) = delete;

    virtual ~NetworkQualityEstimatorProvider() = default;

    // Provides |this| with |callback| that would be invoked by |this| every
    // time there is a change in the network quality estimates.
    virtual void PostReplyOnNetworkQualityChanged(
        base::RepeatingCallback<void(net::EffectiveConnectionType)>
            callback) = 0;

   protected:
    NetworkQualityEstimatorProvider() = default;
  };

  // Creates a NetworkMetricsProvider, where
  // |network_quality_estimator_provider| should be set if it is useful to
  // attach the quality of the network to the metrics report.
  NetworkMetricsProvider(network::NetworkConnectionTrackerAsyncGetter
                             network_connection_tracker_async_getter,
                         std::unique_ptr<NetworkQualityEstimatorProvider>
                             network_quality_estimator_provider = nullptr);

  NetworkMetricsProvider(const NetworkMetricsProvider&) = delete;
  NetworkMetricsProvider& operator=(const NetworkMetricsProvider&) = delete;

  ~NetworkMetricsProvider() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(NetworkMetricsProviderTest, EffectiveConnectionType);
  FRIEND_TEST_ALL_PREFIXES(NetworkMetricsProviderTest,
                           ECTAmbiguousOnConnectionTypeChange);
  FRIEND_TEST_ALL_PREFIXES(NetworkMetricsProviderTest,
                           ECTNotAmbiguousOnUnknownOrOffline);
  FRIEND_TEST_ALL_PREFIXES(NetworkMetricsProviderTest,
                           ConnectionTypeIsAmbiguous);

  // MetricsProvider:
  void ProvideSystemProfileMetrics(SystemProfileProto* system_profile) override;

  // NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  SystemProfileProto::Network::ConnectionType GetConnectionType() const;

  void OnEffectiveConnectionTypeChanged(net::EffectiveConnectionType type);

  // Used as a callback to be given to NetworkConnectionTracker async getter to
  // set the |network_connection_tracker_|.
  void SetNetworkConnectionTracker(
      network::NetworkConnectionTracker* network_connection_tracker);

  // Watches for network connection changes.
  // This |network_connection_tracker_| raw pointer is not owned by this class.
  // It is obtained from the global |g_network_connection_tracker| pointer in
  // //content/public/browser/network_service_instance.cc and points to the same
  // object.
  raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_;

  // True if |connection_type_| changed during the lifetime of the log.
  bool connection_type_is_ambiguous_;
  // The connection type according to network::NetworkConnectionTracker.
  network::mojom::ConnectionType connection_type_;
  // True if the network connection tracker has been initialized.
  bool network_connection_tracker_initialized_;

  // Provides the network quality estimator. May be null.
  std::unique_ptr<NetworkQualityEstimatorProvider>
      network_quality_estimator_provider_;

  // Last known effective connection type.
  net::EffectiveConnectionType effective_connection_type_;

  // Minimum and maximum effective connection type since the metrics were last
  // provided.
  net::EffectiveConnectionType min_effective_connection_type_;
  net::EffectiveConnectionType max_effective_connection_type_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<NetworkMetricsProvider> weak_ptr_factory_{this};
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_NET_NETWORK_METRICS_PROVIDER_H_
