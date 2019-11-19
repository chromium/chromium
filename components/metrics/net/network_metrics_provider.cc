// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/net/network_metrics_provider.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/nqe/effective_connection_type_observer.h"
#include "net/nqe/network_quality_estimator.h"

#if defined(OS_ANDROID)
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "net/android/network_library.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#endif

#if defined(OS_CHROMEOS)
#include "components/metrics/net/wifi_access_point_info_provider_chromeos.h"
#endif  // OS_CHROMEOS

namespace {

#if defined(OS_ANDROID)
// Log the |NCN.NetworkOperatorMCCMNC| histogram.
void LogOperatorCodeHistogram(network::mojom::ConnectionType type) {
  // On a connection type change to cellular, log the network operator MCC/MNC.
  // Log zero in other cases.
  unsigned mcc_mnc = 0;
  if (network::NetworkConnectionTracker::IsConnectionCellular(type)) {
    // Log zero if not perfectly converted.
    if (!base::StringToUint(net::android::GetTelephonyNetworkOperator(),
                            &mcc_mnc)) {
      mcc_mnc = 0;
    }
  }
  base::UmaHistogramSparse("NCN.NetworkOperatorMCCMNC", mcc_mnc);
}
#endif

}  // namespace

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
      NOTREACHED();
      return SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  }
  NOTREACHED();
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
      wifi_phy_layer_protocol_is_ambiguous_(false),
      wifi_phy_layer_protocol_(net::WIFI_PHY_LAYER_PROTOCOL_UNKNOWN),
      total_aborts_(0),
      total_codes_(0),
      network_quality_estimator_provider_(
          std::move(network_quality_estimator_provider)),
      effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      min_effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      max_effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
  network_connection_tracker_async_getter.Run(
      base::BindOnce(&NetworkMetricsProvider::SetNetworkConnectionTracker,
                     weak_ptr_factory_.GetWeakPtr()));
  ProbeWifiPHYLayerProtocol();

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

void NetworkMetricsProvider::FinalizingMetricsLogRecord() {
#if defined(OS_ANDROID)
  // Metrics logged here will be included in every metrics log record.  It's not
  // yet clear if these metrics are generally useful enough to warrant being
  // added to the SystemProfile proto, so they are logged here as histograms for
  // now.
  LogOperatorCodeHistogram(connection_type_);
  if (network::NetworkConnectionTracker::IsConnectionCellular(
          connection_type_)) {
    UMA_HISTOGRAM_ENUMERATION(
        "NCN.CellularConnectionSubtype",
        net::NetworkChangeNotifier::GetConnectionSubtype(),
        net::NetworkChangeNotifier::ConnectionSubtype::SUBTYPE_LAST + 1);
  }
#endif
}

void NetworkMetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension*) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // ProvideCurrentSessionData is called on the main thread, at the time a
  // metrics record is being finalized.
  FinalizingMetricsLogRecord();
  LogAggregatedMetrics();
}

void NetworkMetricsProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!connection_type_is_ambiguous_ ||
         network_connection_tracker_initialized_);
  SystemProfileProto::Network* network = system_profile->mutable_network();
  network->set_connection_type_is_ambiguous(connection_type_is_ambiguous_);
  network->set_connection_type(GetConnectionType());
  network->set_wifi_phy_layer_protocol_is_ambiguous(
      wifi_phy_layer_protocol_is_ambiguous_);
  network->set_wifi_phy_layer_protocol(GetWifiPHYLayerProtocol());

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
  wifi_phy_layer_protocol_is_ambiguous_ = false;
  min_effective_connection_type_ = effective_connection_type_;
  max_effective_connection_type_ = effective_connection_type_;

  if (!wifi_access_point_info_provider_) {
#if defined(OS_CHROMEOS)
    wifi_access_point_info_provider_.reset(
        new WifiAccessPointInfoProviderChromeos());
#else
    wifi_access_point_info_provider_.reset(
        new WifiAccessPointInfoProvider());
#endif  // OS_CHROMEOS
  }

  // Connected wifi access point information.
  WifiAccessPointInfoProvider::WifiAccessPointInfo info;
  if (wifi_access_point_info_provider_->GetInfo(&info))
    WriteWifiAccessPointProto(info, network);
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

  ProbeWifiPHYLayerProtocol();
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
    case network::mojom::ConnectionType::CONNECTION_BLUETOOTH:
      return SystemProfileProto::Network::CONNECTION_BLUETOOTH;
  }
  NOTREACHED();
  return SystemProfileProto::Network::CONNECTION_UNKNOWN;
}

SystemProfileProto::Network::WifiPHYLayerProtocol
NetworkMetricsProvider::GetWifiPHYLayerProtocol() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (wifi_phy_layer_protocol_) {
    case net::WIFI_PHY_LAYER_PROTOCOL_NONE:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_NONE;
    case net::WIFI_PHY_LAYER_PROTOCOL_ANCIENT:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_ANCIENT;
    case net::WIFI_PHY_LAYER_PROTOCOL_A:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_A;
    case net::WIFI_PHY_LAYER_PROTOCOL_B:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_B;
    case net::WIFI_PHY_LAYER_PROTOCOL_G:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_G;
    case net::WIFI_PHY_LAYER_PROTOCOL_N:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_N;
    case net::WIFI_PHY_LAYER_PROTOCOL_UNKNOWN:
      return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_UNKNOWN;
  }
  NOTREACHED();
  return SystemProfileProto::Network::WIFI_PHY_LAYER_PROTOCOL_UNKNOWN;
}

void NetworkMetricsProvider::ProbeWifiPHYLayerProtocol() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&net::GetWifiPHYLayerProtocol),
      base::BindOnce(&NetworkMetricsProvider::OnWifiPHYLayerProtocolResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NetworkMetricsProvider::OnWifiPHYLayerProtocolResult(
    net::WifiPHYLayerProtocol mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (wifi_phy_layer_protocol_ != net::WIFI_PHY_LAYER_PROTOCOL_UNKNOWN &&
      mode != wifi_phy_layer_protocol_) {
    wifi_phy_layer_protocol_is_ambiguous_ = true;
  }
  wifi_phy_layer_protocol_ = mode;
}

void NetworkMetricsProvider::WriteWifiAccessPointProto(
    const WifiAccessPointInfoProvider::WifiAccessPointInfo& info,
    SystemProfileProto::Network* network_proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SystemProfileProto::Network::WifiAccessPoint* access_point_info =
      network_proto->mutable_access_point_info();
  SystemProfileProto::Network::WifiAccessPoint::SecurityMode security =
      SystemProfileProto::Network::WifiAccessPoint::SECURITY_UNKNOWN;
  switch (info.security) {
    case WifiAccessPointInfoProvider::WIFI_SECURITY_NONE:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_NONE;
      break;
    case WifiAccessPointInfoProvider::WIFI_SECURITY_WPA:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_WPA;
      break;
    case WifiAccessPointInfoProvider::WIFI_SECURITY_WEP:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_WEP;
      break;
    case WifiAccessPointInfoProvider::WIFI_SECURITY_RSN:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_RSN;
      break;
    case WifiAccessPointInfoProvider::WIFI_SECURITY_802_1X:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_802_1X;
      break;
    case WifiAccessPointInfoProvider::WIFI_SECURITY_PSK:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_PSK;
      break;
    case WifiAccessPointInfoProvider::WIFI_SECURITY_UNKNOWN:
      security = SystemProfileProto::Network::WifiAccessPoint::SECURITY_UNKNOWN;
      break;
  }
  access_point_info->set_security_mode(security);

  // |bssid| is xx:xx:xx:xx:xx:xx, extract the first three components and
  // pack into a uint32_t.
  std::string bssid = info.bssid;
  if (bssid.size() == 17 && bssid[2] == ':' && bssid[5] == ':' &&
      bssid[8] == ':' && bssid[11] == ':' && bssid[14] == ':') {
    std::string vendor_prefix_str;
    uint32_t vendor_prefix;

    base::RemoveChars(bssid.substr(0, 9), ":", &vendor_prefix_str);
    DCHECK_EQ(6U, vendor_prefix_str.size());
    if (base::HexStringToUInt(vendor_prefix_str, &vendor_prefix))
      access_point_info->set_vendor_prefix(vendor_prefix);
    else
      NOTREACHED();
  }

  // Return if vendor information is not provided.
  if (info.model_number.empty() && info.model_name.empty() &&
      info.device_name.empty() && info.oui_list.empty())
    return;

  SystemProfileProto::Network::WifiAccessPoint::VendorInformation* vendor =
      access_point_info->mutable_vendor_info();
  if (!info.model_number.empty())
    vendor->set_model_number(info.model_number);
  if (!info.model_name.empty())
    vendor->set_model_name(info.model_name);
  if (!info.device_name.empty())
    vendor->set_device_name(info.device_name);

  // Return if OUI list is not provided.
  if (info.oui_list.empty())
    return;

  // Parse OUI list.
  for (const base::StringPiece& oui_str : base::SplitStringPiece(
           info.oui_list, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    uint32_t oui;
    if (base::HexStringToUInt(oui_str, &oui)) {
      vendor->add_element_identifier(oui);
    } else {
      DLOG(WARNING) << "Error when parsing OUI list of the WiFi access point";
    }
  }
}

void NetworkMetricsProvider::LogAggregatedMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::HistogramBase* error_codes = base::SparseHistogram::FactoryGet(
      "Net.ErrorCodesForMainFrame4",
      base::HistogramBase::kUmaTargetedHistogramFlag);
  std::unique_ptr<base::HistogramSamples> samples =
      error_codes->SnapshotSamples();
  base::HistogramBase::Count new_aborts =
      samples->GetCount(-net::ERR_ABORTED) - total_aborts_;
  base::HistogramBase::Count new_codes = samples->TotalCount() - total_codes_;
  if (new_codes > 0) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Net.ErrAborted.CountPerUpload2", new_aborts, 1,
                                100000000, 50);
    UMA_HISTOGRAM_PERCENTAGE("Net.ErrAborted.ProportionPerUpload",
                             (100 * new_aborts) / new_codes);
    total_codes_ += new_codes;
    total_aborts_ += new_aborts;
  }
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
