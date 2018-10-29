// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use_measurement.h"

#include <set>

#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/data_use_measurement/core/data_use_ascriber.h"
#include "components/data_use_measurement/core/data_use_recorder.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/data_use_measurement/core/url_request_classifier.h"
#include "net/base/network_change_notifier.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/features.h"

#if defined(OS_ANDROID)
#include "net/android/traffic_stats.h"
#endif

namespace data_use_measurement {

namespace {

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

#if defined(OS_ANDROID)
void IncrementLatencyHistogramByCount(const std::string& name,
                                      const base::TimeDelta& latency,
                                      int64_t count) {
  base::HistogramBase* histogram_pointer = base::Histogram::FactoryTimeGet(
      name,
      base::TimeDelta::FromMilliseconds(1),  // Minimum sample
      base::TimeDelta::FromHours(1),         // Maximum sample
      50,                                    // Bucket count.
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram_pointer->AddCount(latency.InMilliseconds(), count);
}
#endif

void RecordFavIconDataUse(const net::URLRequest& request) {
  UMA_HISTOGRAM_COUNTS_100000(
      "DataUse.FavIcon.Downstream",
      request.was_cached() ? 0 : request.GetTotalReceivedBytes());
  if (request.status().is_success() &&
      request.GetResponseCode() != net::HTTP_OK) {
    UMA_HISTOGRAM_COUNTS_100000("DataUse.FavIcon.Downstream.Non200Response",
                                request.GetTotalReceivedBytes());
  }
}

}  // namespace

DataUseMeasurement::DataUseMeasurement(
    std::unique_ptr<URLRequestClassifier> url_request_classifier,
    DataUseAscriber* ascriber,
    network::NetworkConnectionTracker* network_connection_tracker)
    : url_request_classifier_(std::move(url_request_classifier)),
      ascriber_(ascriber),
#if defined(OS_ANDROID)
      app_state_(base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES),
      app_listener_(base::android::ApplicationStatusListener::New(
          base::BindRepeating(&DataUseMeasurement::OnApplicationStateChange,
                              base::Unretained(this)))),
      rx_bytes_os_(0),
      tx_bytes_os_(0),
      no_reads_since_background_(false),
#endif
      network_connection_tracker_(network_connection_tracker),
      connection_type_(network::mojom::ConnectionType::CONNECTION_UNKNOWN) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(ascriber_ ||
         base::FeatureList::IsEnabled(network::features::kNetworkService));
  DCHECK(url_request_classifier_ ||
         base::FeatureList::IsEnabled(network::features::kNetworkService));
  DCHECK(network_connection_tracker_ ||
         !base::FeatureList::IsEnabled(network::features::kNetworkService));

  if (network_connection_tracker_) {
    network_connection_tracker_->AddNetworkConnectionObserver(this);
  }

#if defined(OS_ANDROID)
  int64_t bytes = 0;
  // Query Android traffic stats.
  if (net::android::traffic_stats::GetCurrentUidRxBytes(&bytes))
    rx_bytes_os_ = bytes;

  if (net::android::traffic_stats::GetCurrentUidTxBytes(&bytes))
    tx_bytes_os_ = bytes;
#endif
}

DataUseMeasurement::~DataUseMeasurement() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (network_connection_tracker_)
    network_connection_tracker_->RemoveNetworkConnectionObserver(this);
  DCHECK(!services_data_use_observer_list_.might_have_observers());
}

void DataUseMeasurement::OnBeforeURLRequest(net::URLRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DataUseUserData* data_use_user_data = reinterpret_cast<DataUseUserData*>(
      request->GetUserData(DataUseUserData::kUserDataKey));
  if (!data_use_user_data) {
    data_use_user_data = new DataUseUserData(CurrentAppState());
    request->SetUserData(DataUseUserData::kUserDataKey,
                         base::WrapUnique(data_use_user_data));
  } else {
    data_use_user_data->set_app_state(CurrentAppState());
  }
}

void DataUseMeasurement::OnBeforeRedirect(const net::URLRequest& request,
                                          const GURL& new_location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Recording data use of request on redirects.
  // TODO(rajendrant): May not be needed when http://crbug/651957 is fixed.
  UpdateDataUseToMetricsService(
      request.GetTotalSentBytes() + request.GetTotalReceivedBytes(),
      IsCurrentNetworkCellular(),
      IsMetricsServiceRequest(
          request.traffic_annotation().unique_id_hash_code));
  ReportServicesMessageSizeUMA(request);
  if (url_request_classifier_->IsFavIconRequest(request))
    RecordFavIconDataUse(request);
}

void DataUseMeasurement::OnHeadersReceived(
    net::URLRequest* request,
    const net::HttpResponseHeaders* response_headers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DataUseUserData* data_use_user_data = reinterpret_cast<DataUseUserData*>(
      request->GetUserData(DataUseUserData::kUserDataKey));
  if (data_use_user_data) {
    data_use_user_data->set_content_type(
        url_request_classifier_->GetContentType(*request, *response_headers));
  }
}

void DataUseMeasurement::OnNetworkBytesReceived(const net::URLRequest& request,
                                                int64_t bytes_received) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UMA_HISTOGRAM_COUNTS_1M("DataUse.BytesReceived.Delegate", bytes_received);
  ReportDataUseUMA(request, DOWNSTREAM, bytes_received);
#if defined(OS_ANDROID)
  bytes_transferred_since_last_traffic_stats_query_ += bytes_received;
#endif
}

void DataUseMeasurement::OnNetworkBytesSent(const net::URLRequest& request,
                                            int64_t bytes_sent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UMA_HISTOGRAM_COUNTS_1M("DataUse.BytesSent.Delegate", bytes_sent);
  ReportDataUseUMA(request, UPSTREAM, bytes_sent);
#if defined(OS_ANDROID)
  bytes_transferred_since_last_traffic_stats_query_ += bytes_sent;
#endif
}

void DataUseMeasurement::OnCompleted(const net::URLRequest& request,
                                     bool started) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(amohammadkhan): Verify that there is no double recording in data use
  // of redirected requests.
  UpdateDataUseToMetricsService(
      request.GetTotalSentBytes() + request.GetTotalReceivedBytes(),
      IsCurrentNetworkCellular(),
      IsMetricsServiceRequest(
          request.traffic_annotation().unique_id_hash_code));
  ReportServicesMessageSizeUMA(request);
  RecordPageTransitionUMA(request);
#if defined(OS_ANDROID)
  MaybeRecordNetworkBytesOS();
#endif
  if (url_request_classifier_->IsFavIconRequest(request))
    RecordFavIconDataUse(request);
}

// static
DataUseUserData::DataUseContentType
DataUseMeasurement::GetContentTypeForRequest(const net::URLRequest& request) {
  DataUseUserData* attached_user_data = static_cast<DataUseUserData*>(
      request.GetUserData(DataUseUserData::kUserDataKey));
  return attached_user_data ? attached_user_data->content_type()
                            : DataUseUserData::OTHER;
}

void DataUseMeasurement::ReportDataUseUMA(const net::URLRequest& request,
                                          TrafficDirection dir,
                                          int64_t bytes) {
  bool is_user_traffic =
      IsUserRequest(request.traffic_annotation().unique_id_hash_code);

  DataUseUserData* attached_service_data = static_cast<DataUseUserData*>(
      request.GetUserData(DataUseUserData::kUserDataKey));
  DataUseUserData::AppState old_app_state = DataUseUserData::FOREGROUND;
  DataUseUserData::AppState new_app_state = DataUseUserData::UNKNOWN;

  if (attached_service_data)
    old_app_state = attached_service_data->app_state();

  if (old_app_state == CurrentAppState())
    new_app_state = old_app_state;

  if (attached_service_data && old_app_state != new_app_state)
    attached_service_data->set_app_state(CurrentAppState());

  RecordUMAHistogramCount(
      GetHistogramName(is_user_traffic ? "DataUse.TrafficSize.User"
                                       : "DataUse.TrafficSize.System",
                       dir, new_app_state, IsCurrentNetworkCellular()),
      bytes);

#if defined(OS_ANDROID)
  if (dir == DOWNSTREAM && CurrentAppState() == DataUseUserData::BACKGROUND) {
    DCHECK(!last_app_background_time_.is_null());

    const base::TimeDelta time_since_background =
        base::TimeTicks::Now() - last_app_background_time_;
    IncrementLatencyHistogramByCount(
        is_user_traffic ? "DataUse.BackgroundToDataRecievedPerByte.User"
                        : "DataUse.BackgroundToDataRecievedPerByte.System",
        time_since_background, bytes);
    if (no_reads_since_background_) {
      no_reads_since_background_ = false;
      IncrementLatencyHistogramByCount(
          is_user_traffic ? "DataUse.BackgroundToFirstDownstream.User"
                          : "DataUse.BackgroundToFirstDownstream.System",
          time_since_background, 1);
    }
  }
#endif

  bool is_tab_visible = false;

  if (is_user_traffic) {
    const DataUseRecorder* recorder = ascriber_->GetDataUseRecorder(request);
    if (recorder) {
      is_tab_visible = recorder->is_visible();
      RecordTabStateHistogram(dir, new_app_state, recorder->is_visible(),
                              bytes);
    }
  }
  if (attached_service_data && dir == DOWNSTREAM &&
      new_app_state != DataUseUserData::UNKNOWN) {
    RecordContentTypeHistogram(attached_service_data->content_type(),
                               is_user_traffic, new_app_state, is_tab_visible,
                               bytes);
  }
}

#if defined(OS_ANDROID)
void DataUseMeasurement::OnApplicationStateChangeForTesting(
    base::android::ApplicationState application_state) {
  OnApplicationStateChange(application_state);
}
#endif

DataUseUserData::AppState DataUseMeasurement::CurrentAppState() const {
#if defined(OS_ANDROID)
  if (app_state_ != base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES)
    return DataUseUserData::BACKGROUND;
#endif
  // If the OS is not Android, all the requests are considered Foreground.
  return DataUseUserData::FOREGROUND;
}

std::string DataUseMeasurement::GetHistogramNameWithConnectionType(
    const char* prefix,
    TrafficDirection dir,
    DataUseUserData::AppState app_state) const {
  return base::StringPrintf(
      "%s.%s.%s", prefix, dir == UPSTREAM ? "Upstream" : "Downstream",
      app_state == DataUseUserData::UNKNOWN
          ? "Unknown"
          : (app_state == DataUseUserData::FOREGROUND ? "Foreground"
                                                      : "Background"));
}

std::string DataUseMeasurement::GetHistogramName(
    const char* prefix,
    TrafficDirection dir,
    DataUseUserData::AppState app_state,
    bool is_connection_cellular) const {
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
  app_state_ = application_state;
  if (app_state_ != base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES) {
    last_app_background_time_ = base::TimeTicks::Now();
    no_reads_since_background_ = true;
    MaybeRecordNetworkBytesOS();
  } else {
    last_app_background_time_ = base::TimeTicks();
  }
}

void DataUseMeasurement::MaybeRecordNetworkBytesOS() {
  // Minimum number of bytes that should be reported by the network delegate
  // before Android's TrafficStats API is queried (if Chrome is not in
  // background). This reduces the overhead of repeatedly calling the API.
  static const int64_t kMinDelegateBytes = 25000;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (bytes_transferred_since_last_traffic_stats_query_ < kMinDelegateBytes &&
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
        // Do not record samples with value 0.
        UMA_HISTOGRAM_COUNTS_1M("DataUse.BytesReceived.OS",
                                bytes - rx_bytes_os_);
      }
    }
    rx_bytes_os_ = bytes;
  }

  if (net::android::traffic_stats::GetCurrentUidTxBytes(&bytes)) {
    if (tx_bytes_os_ != 0) {
      DCHECK_GE(bytes, tx_bytes_os_);
      if (bytes > tx_bytes_os_) {
        // Do not record samples with value 0.
        UMA_HISTOGRAM_COUNTS_1M("DataUse.BytesSent.OS", bytes - tx_bytes_os_);
      }
    }
    tx_bytes_os_ = bytes;
  }
}
#endif

void DataUseMeasurement::ReportServicesMessageSizeUMA(
    const net::URLRequest& request) {
  if (!IsUserRequest(request.traffic_annotation().unique_id_hash_code)) {
    ReportDataUsageServices(request.traffic_annotation().unique_id_hash_code,
                            UPSTREAM, CurrentAppState(),
                            request.GetTotalSentBytes());
    ReportDataUsageServices(request.traffic_annotation().unique_id_hash_code,
                            DOWNSTREAM, CurrentAppState(),
                            request.GetTotalReceivedBytes());
  }
}

void DataUseMeasurement::ReportDataUsageServices(
    int32_t traffic_annotation_hash,
    TrafficDirection dir,
    DataUseUserData::AppState app_state,
    int64_t message_size_bytes) const {
  if (message_size_bytes > 0) {
    // Conventional UMA histograms are not used because name is not static.
    base::HistogramBase* histogram = base::SparseHistogram::FactoryGet(
        GetHistogramNameWithConnectionType("DataUse.AllServicesKB", dir,
                                           app_state),
        base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->AddKiB(traffic_annotation_hash,
                      base::saturated_cast<int>(message_size_bytes));
  }
}

void DataUseMeasurement::RecordTabStateHistogram(
    TrafficDirection dir,
    DataUseUserData::AppState app_state,
    bool is_tab_visible,
    int64_t bytes) const {
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

void DataUseMeasurement::RecordContentTypeHistogram(
    DataUseUserData::DataUseContentType content_type,
    bool is_user_traffic,
    DataUseUserData::AppState app_state,
    bool is_tab_visible,
    int64_t bytes) {
  if (content_type == DataUseUserData::AUDIO) {
    content_type = app_state != DataUseUserData::FOREGROUND
                       ? DataUseUserData::AUDIO_APPBACKGROUND
                       : (!is_tab_visible ? DataUseUserData::AUDIO_TABBACKGROUND
                                          : DataUseUserData::AUDIO);
  } else if (content_type == DataUseUserData::VIDEO) {
    content_type = app_state != DataUseUserData::FOREGROUND
                       ? DataUseUserData::VIDEO_APPBACKGROUND
                       : (!is_tab_visible ? DataUseUserData::VIDEO_TABBACKGROUND
                                          : DataUseUserData::VIDEO);
  }
  if (is_user_traffic) {
    UMA_HISTOGRAM_SCALED_ENUMERATION("DataUse.ContentType.UserTrafficKB",
                                     content_type, bytes, 1024);
  } else {
    UMA_HISTOGRAM_SCALED_ENUMERATION("DataUse.ContentType.ServicesKB",
                                     content_type, bytes, 1024);
  }
}

void DataUseMeasurement::RecordPageTransitionUMA(
    const net::URLRequest& request) const {
  if (!IsUserRequest(request.traffic_annotation().unique_id_hash_code))
    return;

  const DataUseRecorder* recorder = ascriber_->GetDataUseRecorder(request);
  if (recorder) {
    url_request_classifier_->RecordPageTransitionUMA(
        recorder->page_transition(), request.GetTotalReceivedBytes());
  }
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
  if (network_connection_tracker_) {
    DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
    return network::NetworkConnectionTracker::IsConnectionCellular(
        connection_type_);
  }
  return net::NetworkChangeNotifier::IsConnectionCellular(
      net::NetworkChangeNotifier::GetConnectionType());
}

void DataUseMeasurement::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connection_type_ = type;
}

void DataUseMeasurement::AddServicesDataUseObserver(
    ServicesDataUseObserver* observer) {
  services_data_use_observer_list_.AddObserver(observer);
}

void DataUseMeasurement::RemoveServicesDataUseObserver(
    ServicesDataUseObserver* observer) {
  services_data_use_observer_list_.RemoveObserver(observer);
}

}  // namespace data_use_measurement
