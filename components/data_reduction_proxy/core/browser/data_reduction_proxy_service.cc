// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_pingback_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service_observer.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"
#include "components/data_use_measurement/core/data_use_measurement.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/features.h"

namespace data_reduction_proxy {

DataReductionProxyService::DataReductionProxyService(
    DataReductionProxySettings* settings,
    PrefService* prefs,
    net::URLRequestContextGetter* request_context_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<DataStore> store,
    std::unique_ptr<DataReductionProxyPingbackClient> pingback_client,
    network::NetworkQualityTracker* network_quality_tracker,
    network::NetworkConnectionTracker* network_connection_tracker,
    data_use_measurement::DataUseMeasurement* data_use_measurement,
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    const base::TimeDelta& commit_delay)
    : url_request_context_getter_(request_context_getter),
      url_loader_factory_(std::move(url_loader_factory)),
      pingback_client_(std::move(pingback_client)),
      settings_(settings),
      prefs_(prefs),
      db_data_owner_(new DBDataOwner(std::move(store))),
      io_task_runner_(io_task_runner),
      db_task_runner_(db_task_runner),
      initialized_(false),
      network_quality_tracker_(network_quality_tracker),
      network_connection_tracker_(network_connection_tracker),
      data_use_measurement_(data_use_measurement),
      effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
      weak_factory_(this) {
  DCHECK(settings);
  DCHECK(network_quality_tracker_);
  DCHECK(network_connection_tracker_);
  db_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&DBDataOwner::InitializeOnDBThread,
                                           db_data_owner_->GetWeakPtr()));
  if (prefs_) {
    compression_stats_.reset(
        new DataReductionProxyCompressionStats(this, prefs_, commit_delay));
  }
  network_quality_tracker_->AddEffectiveConnectionTypeObserver(this);
  network_quality_tracker_->AddRTTAndThroughputEstimatesObserver(this);
  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    data_use_measurement_->AddServicesDataUseObserver(this);

  // TODO(rajendrant): Combine uses of NetworkConnectionTracker within DRP.
  network_connection_tracker_->AddNetworkConnectionObserver(this);
  network_connection_tracker_->GetConnectionType(
      &connection_type_,
      base::BindOnce(&DataReductionProxyService::OnConnectionChanged,
                     GetWeakPtr()));
}

DataReductionProxyService::~DataReductionProxyService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_quality_tracker_->RemoveEffectiveConnectionTypeObserver(this);
  network_quality_tracker_->RemoveRTTAndThroughputEstimatesObserver(this);
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
  compression_stats_.reset();
  db_task_runner_->DeleteSoon(FROM_HERE, db_data_owner_.release());
  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    data_use_measurement_->RemoveServicesDataUseObserver(this);
}

void DataReductionProxyService::SetIOData(
    base::WeakPtr<DataReductionProxyIOData> io_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  io_data_ = io_data;
  initialized_ = true;

  // Notify IO data of the current network quality estimates.
  OnEffectiveConnectionTypeChanged(effective_connection_type_);
  if (http_rtt_) {
    OnRTTOrThroughputEstimatesComputed(http_rtt_.value(), base::TimeDelta(),
                                       INT32_MAX);
  }

  for (DataReductionProxyServiceObserver& observer : observer_list_)
    observer.OnServiceInitialized();

  ReadPersistedClientConfig();
}

void DataReductionProxyService::ReadPersistedClientConfig() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!prefs_)
    return;

  base::Time last_config_retrieval_time =
      base::Time() + base::TimeDelta::FromMicroseconds(prefs_->GetInt64(
                         prefs::kDataReductionProxyLastConfigRetrievalTime));
  base::TimeDelta time_since_last_config_retrieval =
      base::Time::Now() - last_config_retrieval_time;

  // A config older than 24 hours should not be used.
  bool persisted_config_is_expired =
      GetFieldTrialParamByFeatureAsBool(
          features::kDataReductionProxyRobustConnection,
          "use_24h_config_expiration_time", true) &&
      !last_config_retrieval_time.is_null() &&
      time_since_last_config_retrieval > base::TimeDelta::FromHours(24);

  if (persisted_config_is_expired)
    return;

  const std::string config_value =
      prefs_->GetString(prefs::kDataReductionProxyConfig);

  if (config_value.empty())
    return;

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DataReductionProxyIOData::SetDataReductionProxyConfiguration,
          io_data_, config_value));
}

void DataReductionProxyService::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connection_type_ = type;
}

void DataReductionProxyService::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  effective_connection_type_ = type;

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DataReductionProxyIOData::OnEffectiveConnectionTypeChanged, io_data_,
          type));
}

void DataReductionProxyService::OnRTTOrThroughputEstimatesComputed(
    base::TimeDelta http_rtt,
    base::TimeDelta transport_rtt,
    int32_t downstream_throughput_kbps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  http_rtt_ = http_rtt;

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DataReductionProxyIOData::OnRTTOrThroughputEstimatesComputed,
          io_data_, http_rtt));
}

void DataReductionProxyService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
}

void DataReductionProxyService::UpdateDataUseForHost(int64_t network_bytes,
                                                     int64_t original_bytes,
                                                     const std::string& host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (compression_stats_) {
    compression_stats_->RecordDataUseByHost(host, network_bytes, original_bytes,
                                            base::Time::Now());
  }
}

void DataReductionProxyService::UpdateContentLengths(
    int64_t data_used,
    int64_t original_size,
    bool data_reduction_proxy_enabled,
    DataReductionProxyRequestType request_type,
    const std::string& mime_type,
    bool is_user_traffic,
    data_use_measurement::DataUseUserData::DataUseContentType content_type,
    int32_t service_hash_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (compression_stats_) {
    compression_stats_->RecordDataUseWithMimeType(
        data_used, original_size, data_reduction_proxy_enabled, request_type,
        mime_type, is_user_traffic, content_type, service_hash_code);
  }
}

void DataReductionProxyService::SetUnreachable(bool unreachable) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  settings_->SetUnreachable(unreachable);
}

void DataReductionProxyService::SetInt64Pref(const std::string& pref_path,
                                             int64_t value) {
  if (prefs_)
    prefs_->SetInt64(pref_path, value);
}

void DataReductionProxyService::SetStringPref(const std::string& pref_path,
                                              const std::string& value) {
  if (prefs_)
    prefs_->SetString(pref_path, value);
}

void DataReductionProxyService::SetProxyPrefs(bool enabled, bool at_startup) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (io_task_runner_->BelongsToCurrentThread()) {
    io_data_->SetProxyPrefs(enabled, at_startup);
    return;
  }
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DataReductionProxyIOData::SetProxyPrefs,
                                io_data_, enabled, at_startup));
}

void DataReductionProxyService::SetPingbackReportingFraction(
    float pingback_reporting_fraction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pingback_client_->SetPingbackReportingFraction(pingback_reporting_fraction);
}

void DataReductionProxyService::OnCacheCleared(const base::Time start,
                                               const base::Time end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DataReductionProxyIOData::OnCacheCleared,
                                io_data_, start, end));
}

net::EffectiveConnectionType
DataReductionProxyService::GetEffectiveConnectionType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return effective_connection_type_;
}

network::mojom::ConnectionType DataReductionProxyService::GetConnectionType()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return connection_type_;
}

base::Optional<base::TimeDelta> DataReductionProxyService::GetHttpRttEstimate()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return http_rtt_;
}

void DataReductionProxyService::SetProxyRequestHeadersOnUI(
    const net::HttpRequestHeaders& headers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  settings_->SetProxyRequestHeaders(headers);
}

void DataReductionProxyService::SetConfiguredProxiesOnUI(
    const net::ProxyList& proxies) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  settings_->SetConfiguredProxies(proxies);
}

void DataReductionProxyService::SetIgnoreLongTermBlackListRules(
    bool ignore_long_term_black_list_rules) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  settings_->SetIgnoreLongTermBlackListRules(ignore_long_term_black_list_rules);
}

void DataReductionProxyService::SetCustomProxyConfigClient(
    network::mojom::CustomProxyConfigClientPtrInfo config_client_info) {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DataReductionProxyIOData::SetCustomProxyConfigClient,
                     io_data_, std::move(config_client_info)));
}

void DataReductionProxyService::LoadHistoricalDataUsage(
    const HistoricalDataUsageCallback& load_data_usage_callback) {
  std::unique_ptr<std::vector<DataUsageBucket>> data_usage(
      new std::vector<DataUsageBucket>());
  std::vector<DataUsageBucket>* data_usage_ptr = data_usage.get();
  db_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&DBDataOwner::LoadHistoricalDataUsage,
                     db_data_owner_->GetWeakPtr(),
                     base::Unretained(data_usage_ptr)),
      base::BindOnce(load_data_usage_callback, std::move(data_usage)));
}

void DataReductionProxyService::LoadCurrentDataUsageBucket(
    const LoadCurrentDataUsageCallback& load_current_data_usage_callback) {
  std::unique_ptr<DataUsageBucket> bucket(new DataUsageBucket());
  DataUsageBucket* bucket_ptr = bucket.get();
  db_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&DBDataOwner::LoadCurrentDataUsageBucket,
                     db_data_owner_->GetWeakPtr(),
                     base::Unretained(bucket_ptr)),
      base::BindOnce(load_current_data_usage_callback, std::move(bucket)));
}

void DataReductionProxyService::StoreCurrentDataUsageBucket(
    std::unique_ptr<DataUsageBucket> current) {
  db_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DBDataOwner::StoreCurrentDataUsageBucket,
                     db_data_owner_->GetWeakPtr(), std::move(current)));
}

void DataReductionProxyService::DeleteHistoricalDataUsage() {
  db_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DBDataOwner::DeleteHistoricalDataUsage,
                                db_data_owner_->GetWeakPtr()));
}

void DataReductionProxyService::DeleteBrowsingHistory(const base::Time& start,
                                                      const base::Time& end) {
  DCHECK_LE(start, end);
  db_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DBDataOwner::DeleteBrowsingHistory,
                                db_data_owner_->GetWeakPtr(), start, end));

  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DataReductionProxyIOData::DeleteBrowsingHistory, io_data_,
                     start, end));
}

void DataReductionProxyService::AddObserver(
    DataReductionProxyServiceObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);
}

void DataReductionProxyService::RemoveObserver(
    DataReductionProxyServiceObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
}

bool DataReductionProxyService::Initialized() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return initialized_;
}

base::WeakPtr<DataReductionProxyService>
DataReductionProxyService::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void DataReductionProxyService::OnServicesDataUse(int64_t recv_bytes,
                                                  int64_t sent_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (compression_stats_) {
    // Record non-content initiated traffic to the Other bucket for data saver
    // site-breakdown.
    DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
    compression_stats_->RecordDataUseByHost(
        util::GetSiteBreakdownOtherHostName(), sent_bytes, sent_bytes,
        base::Time::Now());
    compression_stats_->RecordDataUseByHost(
        util::GetSiteBreakdownOtherHostName(), recv_bytes, recv_bytes,
        base::Time::Now());
  }
}

}  // namespace data_reduction_proxy
