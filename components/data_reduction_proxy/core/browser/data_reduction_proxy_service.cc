// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"
#include "components/data_use_measurement/core/data_use_measurement.h"
#include "components/prefs/pref_service.h"
#include "components/previews/core/previews_experiments.h"

namespace data_reduction_proxy {

namespace {

base::Optional<base::Value> GetSaveDataSavingsPercentEstimateFromFieldTrial() {
  if (!base::FeatureList::IsEnabled(features::kReportSaveDataSavings))
    return base::nullopt;
  const auto origin_savings_estimate_json =
      base::GetFieldTrialParamValueByFeature(features::kReportSaveDataSavings,
                                             "origin_savings_estimate");
  if (origin_savings_estimate_json.empty())
    return base::nullopt;

  auto origin_savings_estimates =
      base::JSONReader::Read(origin_savings_estimate_json);

  UMA_HISTOGRAM_BOOLEAN(
      "DataReductionProxy.ReportSaveDataSavings.ParseResult",
      origin_savings_estimates && origin_savings_estimates->is_dict());

  return origin_savings_estimates;
}

}  // namespace

DataReductionProxyService::DataReductionProxyService(
    DataReductionProxySettings* settings,
    PrefService* prefs,
    std::unique_ptr<DataStore> store,
    data_use_measurement::DataUseMeasurement* data_use_measurement,
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    const base::TimeDelta& commit_delay,
    Client client,
    const std::string& channel,
    const std::string& user_agent)
    : settings_(settings),
      prefs_(prefs),
      db_data_owner_(new DBDataOwner(std::move(store))),
      db_task_runner_(db_task_runner),
      data_use_measurement_(data_use_measurement),
      client_(client),
      channel_(channel),
      save_data_savings_estimate_dict_(
          GetSaveDataSavingsPercentEstimateFromFieldTrial()) {
  DCHECK(data_use_measurement_);
  DCHECK(settings);

  request_options_ =
      std::make_unique<DataReductionProxyRequestOptions>(client_);
  request_options_->Init();
  // It is safe to use base::Unretained here, since it gets executed
  // synchronously on the UI thread, and |this| outlives the caller (since the
  // caller is owned by |this|.
  request_options_->SetUpdateHeaderCallback(
      base::BindRepeating(&DataReductionProxyService::UpdateProxyRequestHeaders,
                          base::Unretained(this)));

  db_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&DBDataOwner::InitializeOnDBThread,
                                           db_data_owner_->GetWeakPtr()));
  if (prefs_) {
    compression_stats_.reset(
        new DataReductionProxyCompressionStats(this, prefs_, commit_delay));
  }
  if (data_use_measurement_) {  // null in unit tests.
    data_use_measurement_->AddServicesDataUseObserver(this);
  }
}

DataReductionProxyService::~DataReductionProxyService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  compression_stats_.reset();
  db_task_runner_->DeleteSoon(FROM_HERE, db_data_owner_.release());
  if (data_use_measurement_) {  // null in unit tests.
    data_use_measurement_->RemoveServicesDataUseObserver(this);
  }
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


void DataReductionProxyService::UpdateProxyRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  settings_->SetProxyRequestHeaders(headers);
}


void DataReductionProxyService::LoadHistoricalDataUsage(
    HistoricalDataUsageCallback load_data_usage_callback) {
  std::unique_ptr<std::vector<DataUsageBucket>> data_usage(
      new std::vector<DataUsageBucket>());
  std::vector<DataUsageBucket>* data_usage_ptr = data_usage.get();
  db_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&DBDataOwner::LoadHistoricalDataUsage,
                     db_data_owner_->GetWeakPtr(),
                     base::Unretained(data_usage_ptr)),
      base::BindOnce(std::move(load_data_usage_callback),
                     std::move(data_usage)));
}

void DataReductionProxyService::LoadCurrentDataUsageBucket(
    LoadCurrentDataUsageCallback load_current_data_usage_callback) {
  std::unique_ptr<DataUsageBucket> bucket(new DataUsageBucket());
  DataUsageBucket* bucket_ptr = bucket.get();
  db_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&DBDataOwner::LoadCurrentDataUsageBucket,
                     db_data_owner_->GetWeakPtr(),
                     base::Unretained(bucket_ptr)),
      base::BindOnce(std::move(load_current_data_usage_callback),
                     std::move(bucket)));
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
}

base::WeakPtr<DataReductionProxyService>
DataReductionProxyService::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void DataReductionProxyService::OnServicesDataUse(int32_t service_hash_code,
                                                  int64_t recv_bytes,
                                                  int64_t sent_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (compression_stats_) {
    // Record non-content initiated traffic to the Other bucket for data saver
    // site-breakdown.
    compression_stats_->RecordDataUseByHost(
        util::GetSiteBreakdownOtherHostName(), sent_bytes, sent_bytes,
        base::Time::Now());
    compression_stats_->RecordDataUseByHost(
        util::GetSiteBreakdownOtherHostName(), recv_bytes, recv_bytes,
        base::Time::Now());
    compression_stats_->RecordDataUseWithMimeType(
        recv_bytes, recv_bytes, settings_->IsDataReductionProxyEnabled(), HTTPS,
        std::string(), false, data_use_measurement::DataUseUserData::OTHER,
        service_hash_code);
  }
}

void DataReductionProxyService::SetDependenciesForTesting(
    std::unique_ptr<DataReductionProxyRequestOptions> request_options) {
  request_options_ = std::move(request_options);
  request_options_->SetUpdateHeaderCallback(
      base::BindRepeating(&DataReductionProxyService::UpdateProxyRequestHeaders,
                          base::Unretained(this)));
}

double DataReductionProxyService::GetSaveDataSavingsPercentEstimate(
    const std::string& origin) const {
  if (origin.empty() || !save_data_savings_estimate_dict_ ||
      !save_data_savings_estimate_dict_->is_dict()) {
    return 0;
  }
  const auto savings_percent =
      save_data_savings_estimate_dict_->FindDoubleKey(origin);
  if (!savings_percent)
    return 0;
  return *savings_percent;
}

}  // namespace data_reduction_proxy
