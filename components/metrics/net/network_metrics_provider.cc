// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/network_metrics_provider.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/nqe/effective_connection_type_observer.h"
#include "net/nqe/network_quality_estimator.h"

#if BUILDFLAG(IS_ANDROID)
#include "services/network/public/cpp/network_connection_tracker.h"
#endif

namespace metrics {

SystemProfileProto::Network::EffectiveConnectionType
ConvertEffectiveConnectionType(
    net::EffectiveConnectionType effective_connection_type) {
  switch (effective_connection_type) {
    case net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN:
      return SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
    case net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G:
      return SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
    case net::EFFECTIVE_CONNECTION_TYPE_2G:
      return SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G;
    case net::EFFECTIVE_CONNECTION_TYPE_3G:
      return SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_3G;
    case net::EFFECTIVE_CONNECTION_TYPE_4G:
      return SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_4G;
    case net::EFFECTIVE_CONNECTION_TYPE_OFFLINE:
      return SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_OFFLINE;
    case net::EFFECTIVE_CONNECTION_TYPE_LAST:
      NOTREACHED_IN_MIGRATION();
      return SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  }
  NOTREACHED_IN_MIGRATION();
  return SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
}

NetworkMetricsProvider::NetworkMetricsProvider(
    network::NetworkConnectionTrackerAsyncGetter
        network_connection_tracker_async_getter,
    std::unique_ptr<NetworkQualityEstimatorProvider>
        network_quality_estimator_provider)
    : network_connection_tracker_(nullptr),
      connection_type_is_ambiguous_(false),
      connection_type_(network::mojom::ConnectionType::CONNECTION_UNKNOWN),
      network_connection_tracker_initialized_(false),
      network_quality_estimator_provider_(
          std::move(network_quality_estimator_provider)),
      effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      min_effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      max_effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
  network_connection_tracker_async_getter.Run(
      base::BindOnce(&NetworkMetricsProvider::SetNetworkConnectionTracker,
                     weak_ptr_factory_.GetWeakPtr()));

  if (network_quality_estimator_provider_) {
    // Use |network_quality_estimator_provider_| to get network quality
    // tracker.
    network_quality_estimator_provider_->PostReplyOnNetworkQualityChanged(
        base::BindRepeating(
            &NetworkMetricsProvider::OnEffectiveConnectionTypeChanged,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

NetworkMetricsProvider::~NetworkMetricsProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (network_connection_tracker_)
    network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

void NetworkMetricsProvider::SetNetworkConnectionTracker(
    network::NetworkConnectionTracker* network_connection_tracker) {
  DCHECK(network_connection_tracker);
  network_connection_tracker_ = network_connection_tracker;
  network_connection_tracker_->AddNetworkConnectionObserver(this);
  network_connection_tracker_->GetConnectionType(
      &connection_type_,
      base::BindOnce(&NetworkMetricsProvider::OnConnectionChanged,
                     weak_ptr_factory_.GetWeakPtr()));
  if (connection_type_ != network::mojom::ConnectionType::CONNECTION_UNKNOWN)
    network_connection_tracker_initialized_ = true;
}

void NetworkMetricsProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!connection_type_is_ambiguous_ ||
         network_connection_tracker_initialized_);
  SystemProfileProto::Network* network = system_profile->mutable_network();
  network->set_connection_type_is_ambiguous(connection_type_is_ambiguous_);
  network->set_connection_type(GetConnectionType());

  network->set_min_effective_connection_type(
      ConvertEffectiveConnectionType(min_effective_connection_type_));
  network->set_max_effective_connection_type(
      ConvertEffectiveConnectionType(max_effective_connection_type_));

  // Note: We get the initial connection type when it becomes available and it
  // is handled at SetNetworkConnectionTracker() when GetConnectionType() is
  // called.
  //
  // Update the connection type. Note that this is necessary to set the network
  // type to "none" if there is no network connection for an entire UMA logging
  // window, since OnConnectionTypeChanged() ignores transitions to the "none"
  // state, and that is ok since it just deals with the current known state.
  if (network_connection_tracker_) {
    network_connection_tracker_->GetConnectionType(&connection_type_,
                                                   base::DoNothing());
  }

  if (connection_type_ != network::mojom::ConnectionType::CONNECTION_UNKNOWN)
    network_connection_tracker_initialized_ = true;
  // Reset the "ambiguous" flags, since a new metrics log session has started.
  connection_type_is_ambiguous_ = false;
  min_effective_connection_type_ = effective_connection_type_;
  max_effective_connection_type_ = effective_connection_type_;
}

void NetworkMetricsProvider::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // To avoid reporting an ambiguous connection type for users on flaky
  // connections, ignore transitions to the "none" state. Note that the
  // connection type is refreshed in ProvideSystemProfileMetrics() each time a
  // new UMA logging window begins, so users who genuinely transition to offline
  // mode for an extended duration will still be at least partially represented
  // in the metrics logs.
  if (type == network::mojom::ConnectionType::CONNECTION_NONE) {
    network_connection_tracker_initialized_ = true;
    return;
  }

  DCHECK(network_connection_tracker_initialized_ ||
         connection_type_ ==
             network::mojom::ConnectionType::CONNECTION_UNKNOWN);

  if (type != connection_type_ &&
      connection_type_ != network::mojom::ConnectionType::CONNECTION_NONE &&
      network_connection_tracker_initialized_) {
    // If |network_connection_tracker_initialized_| is false, it implies that
    // this is the first connection change callback received from network
    // connection tracker, and the previous connection type was
    // CONNECTION_UNKNOWN. In that case, connection type should not be marked as
    // ambiguous since there was no actual change in the connection type.
    connection_type_is_ambiguous_ = true;
  }

  network_connection_tracker_initialized_ = true;
  connection_type_ = type;
}

SystemProfileProto::Network::ConnectionType
NetworkMetricsProvider::GetConnectionType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (connection_type_) {
    case network::mojom::ConnectionType::CONNECTION_NONE:
      return SystemProfileProto::Network::CONNECTION_NONE;
    case network::mojom::ConnectionType::CONNECTION_UNKNOWN:
      return SystemProfileProto::Network::CONNECTION_UNKNOWN;
    case network::mojom::ConnectionType::CONNECTION_ETHERNET:
      return SystemProfileProto::Network::CONNECTION_ETHERNET;
    case network::mojom::ConnectionType::CONNECTION_WIFI:
      return SystemProfileProto::Network::CONNECTION_WIFI;
    case network::mojom::ConnectionType::CONNECTION_2G:
      return SystemProfileProto::Network::CONNECTION_2G;
    case network::mojom::ConnectionType::CONNECTION_3G:
      return SystemProfileProto::Network::CONNECTION_3G;
    case network::mojom::ConnectionType::CONNECTION_4G:
      return SystemProfileProto::Network::CONNECTION_4G;
    case network::mojom::ConnectionType::CONNECTION_5G:
      return SystemProfileProto::Network::CONNECTION_5G;
    case network::mojom::ConnectionType::CONNECTION_BLUETOOTH:
      return SystemProfileProto::Network::CONNECTION_BLUETOOTH;
  }
  NOTREACHED_IN_MIGRATION();
  return SystemProfileProto::Network::CONNECTION_UNKNOWN;
}

void NetworkMetricsProvider::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  effective_connection_type_ = type;

  if (effective_connection_type_ == net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN ||
      effective_connection_type_ == net::EFFECTIVE_CONNECTION_TYPE_OFFLINE) {
    // The effective connection type may be reported as Unknown if there is a
    // change in the connection type. Disregard it since network requests can't
    // be send during the changes in connection type. Similarly, disregard
    // offline as the type since it may be reported as the effective connection
    // type for a short period when there is a change in the connection type.
    return;
  }

  if (min_effective_connection_type_ ==
          net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN &&
      max_effective_connection_type_ ==
          net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    min_effective_connection_type_ = type;
    max_effective_connection_type_ = type;
    return;
  }

  if (min_effective_connection_type_ ==
          net::EFFECTIVE_CONNECTION_TYPE_OFFLINE &&
      max_effective_connection_type_ ==
          net::EFFECTIVE_CONNECTION_TYPE_OFFLINE) {
    min_effective_connection_type_ = type;
    max_effective_connection_type_ = type;
    return;
  }

  min_effective_connection_type_ =
      std::min(min_effective_connection_type_, effective_connection_type_);
  max_effective_connection_type_ =
      std::max(max_effective_connection_type_, effective_connection_type_);

  DCHECK_EQ(
      min_effective_connection_type_ == net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
      max_effective_connection_type_ == net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  DCHECK_EQ(
      min_effective_connection_type_ == net::EFFECTIVE_CONNECTION_TYPE_OFFLINE,
      max_effective_connection_type_ == net::EFFECTIVE_CONNECTION_TYPE_OFFLINE);
}

}  // namespace metrics
