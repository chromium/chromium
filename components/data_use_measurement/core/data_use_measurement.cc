// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use_measurement.h"

#include <set>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/stringprintf.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/features.h"

#if defined(OS_ANDROID)
#include "net/android/traffic_stats.h"
#endif

namespace data_use_measurement {

namespace {

#if defined(OS_ANDROID)
bool IsInForeground(base::android::ApplicationState state) {
  switch (state) {
    case base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES:
      return true;
    case base::android::APPLICATION_STATE_UNKNOWN:
    case base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES:
    case base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES:
    case base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES:
      return false;
  }
}
#endif

// Records the occurrence of |sample| in |name| histogram. Conventional UMA
// histograms are not used because the |name| is not static.
void RecordUMAHistogramCount(const std::string& name, int64_t sample) {
  base::HistogramBase* histogram_pointer = base::Histogram::FactoryGet(
      name,
      1,        // Minimum sample size in bytes.
      1000000,  // Maximum sample size in bytes. Should cover most of the
                // requests by services.
      50,       // Bucket count.
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram_pointer->Add(sample);
}

}  // namespace

DataUseMeasurement::DataUseMeasurement(
    PrefService* pref_service,
    network::NetworkConnectionTracker* network_connection_tracker)
    : network_connection_tracker_(network_connection_tracker),
      data_use_tracker_prefs_(base::DefaultClock::GetInstance(), pref_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(network_connection_tracker_);

#if defined(OS_ANDROID)
  int64_t bytes = 0;
  // Query Android traffic stats.
  if (net::android::traffic_stats::GetCurrentUidRxBytes(&bytes))
    rx_bytes_os_ = bytes;

  if (net::android::traffic_stats::GetCurrentUidTxBytes(&bytes))
    tx_bytes_os_ = bytes;
#endif

  network_connection_tracker_->AddLeakyNetworkConnectionObserver(this);

  network_connection_tracker_->GetConnectionType(
      &connection_type_,
      base::BindOnce(&DataUseMeasurement::OnConnectionChanged,
                     weak_ptr_factory_.GetWeakPtr()));

#if defined(OS_ANDROID)
  app_state_ = base::android::ApplicationStatusListener::GetState();

  app_listener_ = base::android::ApplicationStatusListener::New(
      base::BindRepeating(&DataUseMeasurement::OnApplicationStateChange,
                          base::Unretained(this)));
#endif
}

DataUseMeasurement::~DataUseMeasurement() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if defined(OS_ANDROID)
  if (app_listener_)
    app_listener_.reset();
#endif
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
  DCHECK(services_data_use_observer_list_.empty());
}

void DataUseMeasurement::RecordDownstreamUserTrafficSizeMetric(
    bool is_tab_visible,
    int64_t bytes) {
  RecordUMAHistogramCount(
      GetHistogramName("DataUse.TrafficSize.User", DOWNSTREAM,
                       CurrentAppState(), IsCurrentNetworkCellular()),
      bytes);
  RecordTabStateHistogram(DOWNSTREAM, CurrentAppState(), is_tab_visible, bytes);
  bytes_transferred_since_last_traffic_stats_query_ += bytes;
  MaybeRecordNetworkBytesOS(/*force_record_metrics=*/false);

  data_use_tracker_prefs_.ReportNetworkServiceDataUse(
      IsCurrentNetworkCellular(),
      CurrentAppState() == DataUseUserData::FOREGROUND,
      /*is_user_traffic=*/true, bytes);
}

#if defined(OS_ANDROID)
void DataUseMeasurement::OnApplicationStateChangeForTesting(
    base::android::ApplicationState application_state) {
  OnApplicationStateChange(application_state);
}
#endif

DataUseUserData::AppState DataUseMeasurement::CurrentAppState() const {
#if defined(OS_ANDROID)
  return IsInForeground(app_state_) ? DataUseUserData::FOREGROUND
                                    : DataUseUserData::BACKGROUND;
#endif
  // If the OS is not Android, all the requests are considered Foreground.
  return DataUseUserData::FOREGROUND;
}

// static
std::string DataUseMeasurement::GetHistogramNameWithConnectionType(
    const char* prefix,
    TrafficDirection dir,
    DataUseUserData::AppState app_state) {
  return base::StringPrintf(
      "%s.%s.%s", prefix, dir == UPSTREAM ? "Upstream" : "Downstream",
      app_state == DataUseUserData::UNKNOWN
          ? "Unknown"
          : (app_state == DataUseUserData::FOREGROUND ? "Foreground"
                                                      : "Background"));
}

// static
std::string DataUseMeasurement::GetHistogramName(
    const char* prefix,
    TrafficDirection dir,
    DataUseUserData::AppState app_state,
    bool is_connection_cellular) {
  return base::StringPrintf(
      "%s.%s.%s.%s", prefix, dir == UPSTREAM ? "Upstream" : "Downstream",
      app_state == DataUseUserData::UNKNOWN
          ? "Unknown"
          : (app_state == DataUseUserData::FOREGROUND ? "Foreground"
                                                      : "Background"),
      is_connection_cellular ? "Cellular" : "NotCellular");
}

#if defined(OS_ANDROID)
void DataUseMeasurement::OnApplicationStateChange(
    base::android::ApplicationState application_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (app_state_ == application_state)
    return;
  MaybeRecordNetworkBytesOS(/*force_record_metrics=*/true);
  app_state_ = application_state;
}
#endif

void DataUseMeasurement::MaybeRecordNetworkBytesOS(bool force_record_metrics) {
#if defined(OS_ANDROID)
  // Minimum number of bytes that should be reported by the network delegate
  // before Android's TrafficStats API is queried (if Chrome is not in
  // background). This reduces the overhead of repeatedly calling the API.
  static const int64_t kMinDelegateBytes = 25000;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!force_record_metrics &&
      bytes_transferred_since_last_traffic_stats_query_ < kMinDelegateBytes &&
      CurrentAppState() == DataUseUserData::FOREGROUND) {
    return;
  }
  bytes_transferred_since_last_traffic_stats_query_ = 0;
  int64_t bytes = 0;
  // Query Android traffic stats directly instead of registering with the
  // DataUseAggregator since the latter does not provide notifications for
  // the incognito traffic.
  if (net::android::traffic_stats::GetCurrentUidRxBytes(&bytes)) {
    if (rx_bytes_os_ != 0) {
      DCHECK_GE(bytes, rx_bytes_os_);
      if (bytes > rx_bytes_os_) {
        int64_t incremental_bytes = bytes - rx_bytes_os_;
        // Do not record samples with value 0.
        base::UmaHistogramCustomCounts("DataUse.BytesReceived2.OS",
                                       incremental_bytes, 50, 10 * 1000 * 1000,
                                       50);
        if (IsInForeground(app_state_)) {
          base::UmaHistogramCustomCounts("DataUse.BytesReceived2.OS.Foreground",
                                         incremental_bytes, 50,
                                         10 * 1000 * 1000, 50);
        } else {
          base::UmaHistogramCustomCounts("DataUse.BytesReceived2.OS.Background",
                                         incremental_bytes, 50,
                                         10 * 1000 * 1000, 50);
        }
      }
    }
    rx_bytes_os_ = bytes;
  }

  if (net::android::traffic_stats::GetCurrentUidTxBytes(&bytes)) {
    if (tx_bytes_os_ != 0) {
      DCHECK_GE(bytes, tx_bytes_os_);
      if (bytes > tx_bytes_os_) {
        int64_t incremental_bytes = bytes - tx_bytes_os_;
        // Do not record samples with value 0.
        UMA_HISTOGRAM_COUNTS_1M("DataUse.BytesSent.OS", incremental_bytes);
        if (IsInForeground(app_state_)) {
          UMA_HISTOGRAM_COUNTS_1M("DataUse.BytesSent.OS.Foreground",
                                  incremental_bytes);
        } else {
          UMA_HISTOGRAM_COUNTS_1M("DataUse.BytesSent.OS.Background",
                                  incremental_bytes);
        }
      }
    }
    tx_bytes_os_ = bytes;
  }
#endif
}

void DataUseMeasurement::ReportDataUsageServices(
    int32_t traffic_annotation_hash,
    TrafficDirection dir,
    DataUseUserData::AppState app_state,
    int64_t message_size_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (message_size_bytes <= 0)
    return;

  // Conventional UMA histograms are not used because name is not static.
  base::HistogramBase* histogram = base::SparseHistogram::FactoryGet(
      GetHistogramNameWithConnectionType("DataUse.AllServicesKB", dir,
                                         app_state),
      base::HistogramBase::kUmaTargetedHistogramFlag);
  // AddKiB method takes value in bytes.
  histogram->AddKiB(traffic_annotation_hash,
                    base::saturated_cast<int>(message_size_bytes));

  bytes_transferred_since_last_traffic_stats_query_ += message_size_bytes;
  MaybeRecordNetworkBytesOS(/*force_record_metrics=*/false);

  data_use_tracker_prefs_.ReportNetworkServiceDataUse(
      IsCurrentNetworkCellular(),
      CurrentAppState() == DataUseUserData::FOREGROUND,
      /*is_user_traffic=*/false, message_size_bytes);
}

void DataUseMeasurement::RecordTabStateHistogram(
    TrafficDirection dir,
    DataUseUserData::AppState app_state,
    bool is_tab_visible,
    int64_t bytes) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (app_state == DataUseUserData::UNKNOWN)
    return;

  std::string histogram_name = "DataUse.AppTabState.";
  histogram_name.append(dir == UPSTREAM ? "Upstream." : "Downstream.");
  if (app_state == DataUseUserData::BACKGROUND) {
    histogram_name.append("AppBackground");
  } else if (is_tab_visible) {
    histogram_name.append("AppForeground.TabForeground");
  } else {
    histogram_name.append("AppForeground.TabBackground");
  }
  RecordUMAHistogramCount(histogram_name, bytes);
}

// static
bool DataUseMeasurement::IsUserRequest(
    int32_t network_traffic_annotation_hash_id) {
  static const std::set<int32_t> kUserInitiatedTrafficAnnotations = {
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH(
          "blink_extension_resource_loader"),
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH("blink_resource_loader"),
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH("parallel_download_job"),
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH("renderer_initiated_download"),
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH("drag_download_file"),
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH(
          "download_web_contents_frame"), /*save page action*/
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH(
          "render_view_context_menu"), /* save link as*/
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH("webstore_installer"),
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH("pdf_plugin_placeholder"),
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH(
          "downloads_api_run_async"), /* Can be user request or
                                         autonomous request from extensions*/
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH("resource_dispatcher_host"),
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH("navigation_url_loader"),
  };
  return kUserInitiatedTrafficAnnotations.find(
             network_traffic_annotation_hash_id) !=
         kUserInitiatedTrafficAnnotations.end();
}

// static
bool DataUseMeasurement::IsUserDownloadsRequest(
    int32_t network_traffic_annotation_hash_id) {
  static const std::set<int32_t> kUserDownloadsTrafficAnnotations = {
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH("parallel_download_job"),
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH("renderer_initiated_download"),
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH("drag_download_file"),
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH("download_web_contents_frame"),
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH("downloads_api_run_async"),
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH("resumed_downloads"),
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH("download_via_context_menu"),
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH("offline_pages_download_file"),
  };
  return kUserDownloadsTrafficAnnotations.find(
             network_traffic_annotation_hash_id) !=
         kUserDownloadsTrafficAnnotations.end();
}

// static
bool DataUseMeasurement::IsMetricsServiceRequest(
    int32_t network_traffic_annotation_hash_id) {
  static const std::set<int32_t> kMetricsServiceTrafficAnnotations = {
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH("metrics_report_uma"),
      COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH("metrics_report_ukm"),
  };
  return kMetricsServiceTrafficAnnotations.find(
             network_traffic_annotation_hash_id) !=
         kMetricsServiceTrafficAnnotations.end();
}

bool DataUseMeasurement::IsCurrentNetworkCellular() const {
  return network::NetworkConnectionTracker::IsConnectionCellular(
      connection_type_);
}

void DataUseMeasurement::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (connection_type_ != network::mojom::ConnectionType::CONNECTION_UNKNOWN)
    MaybeRecordNetworkBytesOS(/*force_record_metrics=*/true);

  connection_type_ = type;
}

void DataUseMeasurement::AddServicesDataUseObserver(
    ServicesDataUseObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  services_data_use_observer_list_.AddObserver(observer);
}

void DataUseMeasurement::RemoveServicesDataUseObserver(
    ServicesDataUseObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  services_data_use_observer_list_.RemoveObserver(observer);
}

// static
void DataUseMeasurement::RegisterDataUseComponentLocalStatePrefs(
    PrefRegistrySimple* registry) {
  DataUseTrackerPrefs::RegisterDataUseTrackerLocalStatePrefs(registry);
}

}  // namespace data_use_measurement
