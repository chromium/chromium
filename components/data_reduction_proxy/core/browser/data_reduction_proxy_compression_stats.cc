// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_usage_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "net/base/mime_util.h"

namespace data_reduction_proxy {

namespace {

#define CONCAT(a, b) a##b
// CONCAT1 provides extra level of indirection so that __LINE__ macro expands.
#define CONCAT1(a, b) CONCAT(a, b)
#define UNIQUE_VARNAME CONCAT1(var_, __LINE__)
// We need to use a macro instead of a method because UMA_HISTOGRAM_COUNTS_1M
// requires its first argument to be an inline string and not a variable.
#define RECORD_INT64PREF_TO_HISTOGRAM(pref, uma)        \
  int64_t UNIQUE_VARNAME = GetInt64(pref);              \
  if (UNIQUE_VARNAME > 0) {                             \
    UMA_HISTOGRAM_COUNTS_1M(uma, UNIQUE_VARNAME >> 10); \
  }

const double kSecondsPerWeek =
    base::Time::kMicrosecondsPerWeek / base::Time::kMicrosecondsPerSecond;

// Returns the value at |index| of |list_value| as an int64_t.
int64_t GetInt64PrefValue(const base::ListValue& list_value, size_t index) {
  int64_t val = 0;
  std::string pref_value;
  bool rv = list_value.GetString(index, &pref_value);
  DCHECK(rv);
  if (rv) {
    rv = base::StringToInt64(pref_value, &val);
    DCHECK(rv);
  }
  return val;
}

// Ensure list has exactly |length| elements, either by truncating at the
// front, or appending "0"'s to the back.
void MaintainContentLengthPrefsWindow(base::ListValue* list, size_t length) {
  // Remove data for old days from the front.
  while (list->GetSize() > length)
    list->Remove(0, nullptr);
  // Newly added lists are empty. Add entries to back to fill the window,
  // each initialized to zero.
  while (list->GetSize() < length)
    list->AppendString(base::NumberToString(0));
  DCHECK_EQ(length, list->GetSize());
}

// Increments an int64_t, stored as a string, in a ListPref at the specified
// index.  The value must already exist and be a string representation of a
// number.
void AddInt64ToListPref(size_t index,
                        int64_t length,
                        base::ListValue* list_update) {
  int64_t value = GetInt64PrefValue(*list_update, index) + length;
  list_update->Set(index,
                   std::make_unique<base::Value>(base::NumberToString(value)));
}

void RecordSavingsClearedMetric(DataReductionProxySavingsClearedReason reason) {
  DCHECK_GT(DataReductionProxySavingsClearedReason::REASON_COUNT, reason);
  UMA_HISTOGRAM_ENUMERATION(
      "DataReductionProxy.SavingsCleared.Reason", reason,
      DataReductionProxySavingsClearedReason::REASON_COUNT);
}

// Returns the week number for the current time. The epoch time is treated as
// week=0.
int32_t GetCurrentWeekNumber(const base::Time& now) {
  double now_in_seconds = now.ToDoubleT();
  return now_in_seconds / kSecondsPerWeek;
}

// Adds |value| to the item at |key| in the preference dictionary found at
// |pref|. If |key| is not found it will be inserted.
void AddToDictionaryPref(PrefService* pref_service,
                         const std::string& pref,
                         int key,
                         int value) {
  DictionaryPrefUpdate pref_update(pref_service, pref);
  base::DictionaryValue* pref_dict = pref_update.Get();
  const std::string key_str = base::NumberToString(key);
  base::Value* dict_value = pref_dict->FindKey(key_str);
  if (dict_value)
    value += dict_value->GetInt();
  pref_dict->SetKey(key_str, base::Value(value));
}

// Moves the dictionary stored in preference |pref_src| to |pref_dst|, and
// clears the preference |pref_src|.
void MoveAndClearDictionaryPrefs(PrefService* pref_service,
                                 const std::string& pref_dst,
                                 const std::string& pref_src) {
  DictionaryPrefUpdate pref_update_dst(pref_service, pref_dst);
  base::DictionaryValue* pref_dict_dst = pref_update_dst.Get();
  DictionaryPrefUpdate pref_update_src(pref_service, pref_src);
  base::DictionaryValue* pref_dict_src = pref_update_src.Get();
  pref_dict_dst->Clear();
  pref_dict_dst->Swap(pref_dict_src);
  DCHECK(pref_dict_src->empty());
}

void MaybeInitWeeklyAggregateDataUsePrefs(const base::Time& now,
                                          PrefService* pref_service) {
  int saved_week = pref_service->GetInteger(prefs::kThisWeekNumber);
  int current_week = GetCurrentWeekNumber(now);

  if (saved_week == current_week)
    return;

  pref_service->SetInteger(prefs::kThisWeekNumber, current_week);
  if (current_week == saved_week + 1) {
    // The next week has just started. Move the data use aggregate prefs for
    // this week to last week, and clear the prefs for this week.
    MoveAndClearDictionaryPrefs(pref_service,
                                prefs::kLastWeekServicesDownstreamBackgroundKB,
                                prefs::kThisWeekServicesDownstreamBackgroundKB);
    MoveAndClearDictionaryPrefs(pref_service,
                                prefs::kLastWeekServicesDownstreamForegroundKB,
                                prefs::kThisWeekServicesDownstreamForegroundKB);
    MoveAndClearDictionaryPrefs(
        pref_service, prefs::kLastWeekUserTrafficContentTypeDownstreamKB,
        prefs::kThisWeekUserTrafficContentTypeDownstreamKB);
  } else {
    // Current week is too different than the last time data use aggregate prefs
    // were updated. This may happen if Chrome was opened after a long time, or
    // due to system clock being changed backward or forward. Clear all prefs in
    // this case.
    pref_service->ClearPref(prefs::kLastWeekServicesDownstreamBackgroundKB);
    pref_service->ClearPref(prefs::kLastWeekServicesDownstreamForegroundKB);
    pref_service->ClearPref(prefs::kLastWeekUserTrafficContentTypeDownstreamKB);
    pref_service->ClearPref(prefs::kThisWeekServicesDownstreamBackgroundKB);
    pref_service->ClearPref(prefs::kThisWeekServicesDownstreamForegroundKB);
    pref_service->ClearPref(prefs::kThisWeekUserTrafficContentTypeDownstreamKB);
  }
}

// Records the key-value pairs in the dictionary in a sparse histogram.
void RecordDictionaryToHistogram(const std::string& histogram_name,
                                 const base::DictionaryValue* dictionary) {
  base::HistogramBase* histogram = base::SparseHistogram::FactoryGet(
      histogram_name, base::HistogramBase::kUmaTargetedHistogramFlag);
  for (const auto& entry : *dictionary) {
    int key;
    int value = entry.second->GetInt();
    if (value > 0 && base::StringToInt(entry.first, &key)) {
      histogram->AddCount(key, value);
    }
  }
}

}  // namespace

class DataReductionProxyCompressionStats::DailyContentLengthUpdate {
 public:
  DailyContentLengthUpdate(
      DataReductionProxyCompressionStats* compression_stats,
      const char* pref_path)
      : update_(nullptr),
        compression_stats_(compression_stats),
        pref_path_(pref_path) {}

  void UpdateForDateChange(int days_since_last_update) {
    if (days_since_last_update) {
      MaybeInitialize();
      MaintainContentLengthPrefForDateChange(days_since_last_update);
    }
  }

  // Update the lengths for the current day.
  void Add(int64_t content_length) {
    if (content_length != 0) {
      MaybeInitialize();
      AddInt64ToListPref(kNumDaysInHistory - 1, content_length, update_);
    }
  }

  int64_t GetListPrefValue(size_t index) {
    MaybeInitialize();
    return std::max(GetInt64PrefValue(*update_, index),
                    static_cast<int64_t>(0));
  }

 private:
  void MaybeInitialize() {
    if (update_)
      return;

    update_ = compression_stats_->GetList(pref_path_);
    // New empty lists may have been created. Maintain the invariant that
    // there should be exactly |kNumDaysInHistory| days in the histories.
    MaintainContentLengthPrefsWindow(update_, kNumDaysInHistory);
  }

  // Update the list for date change and ensure the list has exactly |length|
  // elements. The last entry in the list will be for the current day after
  // the update.
  void MaintainContentLengthPrefForDateChange(int days_since_last_update) {
    if (days_since_last_update == -1) {
      // The system may go backwards in time by up to a day for legitimate
      // reasons, such as with changes to the time zone. In such cases, we
      // keep adding to the current day.
      // Note: we accept the fact that some reported data is shifted to
      // the adjacent day if users travel back and forth across time zones.
      days_since_last_update = 0;
    } else if (days_since_last_update < -1) {
      // Erase all entries if the system went backwards in time by more than
      // a day.
      update_->Clear();

      days_since_last_update = kNumDaysInHistory;
    }
    DCHECK_GE(days_since_last_update, 0);

    // Add entries for days since last update event. This will make the
    // lists longer than kNumDaysInHistory. The additional items will be cut off
    // from the head of the lists by |MaintainContentLengthPrefsWindow|, below.
    for (int i = 0;
         i < days_since_last_update && i < static_cast<int>(kNumDaysInHistory);
         ++i) {
      update_->AppendString(base::NumberToString(0));
    }

    // Entries for new days may have been appended. Maintain the invariant that
    // there should be exactly |kNumDaysInHistory| days in the histories.
    MaintainContentLengthPrefsWindow(update_, kNumDaysInHistory);
  }

  // Non-owned. Lazily initialized, set to nullptr until initialized.
  base::ListValue* update_;
  // Non-owned pointer.
  DataReductionProxyCompressionStats* compression_stats_;
  // The path of the content length pref for |this|.
  const char* pref_path_;

  DISALLOW_COPY_AND_ASSIGN(DailyContentLengthUpdate);
};

// DailyDataSavingUpdate maintains a pair of data saving prefs, original_update_
// and received_update_. pref_original is a list of |kNumDaysInHistory| elements
// of daily total original content lengths for the past |kNumDaysInHistory|
// days. pref_received is the corresponding list of the daily total received
// content lengths.
class DataReductionProxyCompressionStats::DailyDataSavingUpdate {
 public:
  DailyDataSavingUpdate(DataReductionProxyCompressionStats* compression_stats,
                        const char* original_pref_path,
                        const char* received_pref_path)
      : original_(compression_stats, original_pref_path),
        received_(compression_stats, received_pref_path) {}

  void UpdateForDateChange(int days_since_last_update) {
    original_.UpdateForDateChange(days_since_last_update);
    received_.UpdateForDateChange(days_since_last_update);
  }

  // Update the lengths for the current day.
  void Add(int64_t original_content_length, int64_t received_content_length) {
    original_.Add(original_content_length);
    received_.Add(received_content_length);
  }

  int64_t GetOriginalListPrefValue(size_t index) {
    return original_.GetListPrefValue(index);
  }
  int64_t GetReceivedListPrefValue(size_t index) {
    return received_.GetListPrefValue(index);
  }

 private:
  DailyContentLengthUpdate original_;
  DailyContentLengthUpdate received_;

  DISALLOW_COPY_AND_ASSIGN(DailyDataSavingUpdate);
};

DataReductionProxyCompressionStats::DataReductionProxyCompressionStats(
    DataReductionProxyService* service,
    PrefService* prefs,
    const base::TimeDelta& delay)
    : service_(service),
      pref_service_(prefs),
      delay_(delay),
      data_usage_map_is_dirty_(false),
      current_data_usage_load_status_(NOT_LOADED) {
  DCHECK(service);
  DCHECK(prefs);
  DCHECK_GE(delay.InMilliseconds(), 0);
  Init();
}

DataReductionProxyCompressionStats::~DataReductionProxyCompressionStats() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (current_data_usage_load_status_ == LOADED)
    PersistDataUsage();

  WritePrefs();
}

void DataReductionProxyCompressionStats::Init() {
  DCHECK(thread_checker_.CalledOnValidThread());

  data_usage_reporting_enabled_.Init(
      prefs::kDataUsageReportingEnabled, pref_service_,
      base::Bind(
          &DataReductionProxyCompressionStats::OnDataUsageReportingPrefChanged,
          weak_factory_.GetWeakPtr()));

  if (data_usage_reporting_enabled_.GetValue()) {
    current_data_usage_load_status_ = LOADING;
    service_->LoadCurrentDataUsageBucket(base::Bind(
        &DataReductionProxyCompressionStats::OnCurrentDataUsageLoaded,
        weak_factory_.GetWeakPtr()));
  }

  InitializeWeeklyAggregateDataUse(base::Time::Now());

  if (delay_.is_zero())
    return;

  // Init all int64_t prefs.
  InitInt64Pref(prefs::kDailyHttpContentLengthLastUpdateDate);
  InitInt64Pref(prefs::kHttpReceivedContentLength);
  InitInt64Pref(prefs::kHttpOriginalContentLength);

  // Init all list prefs.
  InitListPref(prefs::kDailyHttpOriginalContentLength);
  InitListPref(prefs::kDailyHttpReceivedContentLength);
}

void DataReductionProxyCompressionStats::RecordDataUseWithMimeType(
    int64_t data_used,
    int64_t original_size,
    bool data_saver_enabled,
    DataReductionProxyRequestType request_type,
    const std::string& mime_type,
    bool is_user_traffic,
    data_use_measurement::DataUseUserData::DataUseContentType content_type,
    int32_t service_hash_code) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("loading",
               "DataReductionProxyCompressionStats::RecordDataUseWithMimeType")

  IncreaseInt64Pref(data_reduction_proxy::prefs::kHttpReceivedContentLength,
                    data_used);
  IncreaseInt64Pref(data_reduction_proxy::prefs::kHttpOriginalContentLength,
                    original_size);

  RecordRequestSizePrefs(data_used, original_size, data_saver_enabled,
                         request_type, mime_type, base::Time::Now());
  RecordWeeklyAggregateDataUse(
      base::Time::Now(), std::round(static_cast<double>(data_used) / 1024),
      is_user_traffic, content_type, service_hash_code);
}

void DataReductionProxyCompressionStats::InitInt64Pref(const char* pref) {
  int64_t pref_value = pref_service_->GetInt64(pref);
  pref_map_[pref] = pref_value;
}

void DataReductionProxyCompressionStats::InitListPref(const char* pref) {
  std::unique_ptr<base::ListValue> pref_value =
      std::unique_ptr<base::ListValue>(
          pref_service_->GetList(pref)->DeepCopy());
  list_pref_map_[pref] = std::move(pref_value);
}

int64_t DataReductionProxyCompressionStats::GetInt64(const char* pref_path) {
  if (delay_.is_zero())
    return pref_service_->GetInt64(pref_path);

  auto iter = pref_map_.find(pref_path);
  return iter->second;
}

void DataReductionProxyCompressionStats::SetInt64(const char* pref_path,
                                                  int64_t pref_value) {
  if (delay_.is_zero()) {
    pref_service_->SetInt64(pref_path, pref_value);
    return;
  }

  DelayedWritePrefs();
  pref_map_[pref_path] = pref_value;
}

void DataReductionProxyCompressionStats::IncreaseInt64Pref(
    const char* pref_path,
    int64_t delta) {
  SetInt64(pref_path, GetInt64(pref_path) + delta);
}

base::ListValue* DataReductionProxyCompressionStats::GetList(
    const char* pref_path) {
  if (delay_.is_zero())
    return ListPrefUpdate(pref_service_, pref_path).Get();

  DelayedWritePrefs();
  auto it = list_pref_map_.find(pref_path);
  if (it == list_pref_map_.end())
    return nullptr;
  return it->second.get();
}

void DataReductionProxyCompressionStats::WritePrefs() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (delay_.is_zero())
    return;

  for (auto iter = pref_map_.begin(); iter != pref_map_.end(); ++iter) {
    pref_service_->SetInt64(iter->first, iter->second);
  }

  for (auto iter = list_pref_map_.begin(); iter != list_pref_map_.end();
       ++iter) {
    TransferList(*(iter->second.get()),
                 ListPrefUpdate(pref_service_, iter->first).Get());
  }
}

int64_t DataReductionProxyCompressionStats::GetLastUpdateTime() {
  int64_t last_update_internal =
      GetInt64(prefs::kDailyHttpContentLengthLastUpdateDate);
  base::Time last_update = base::Time::FromInternalValue(last_update_internal);
  return static_cast<int64_t>(last_update.ToJsTime());
}

void DataReductionProxyCompressionStats::ResetStatistics() {
  base::ListValue* original_update =
      GetList(prefs::kDailyHttpOriginalContentLength);
  base::ListValue* received_update =
      GetList(prefs::kDailyHttpReceivedContentLength);
  original_update->Clear();
  received_update->Clear();
  for (size_t i = 0; i < kNumDaysInHistory; ++i) {
    original_update->AppendString(base::NumberToString(0));
    received_update->AppendString(base::NumberToString(0));
  }
}

int64_t DataReductionProxyCompressionStats::GetHttpReceivedContentLength() {
  return GetInt64(prefs::kHttpReceivedContentLength);
}

int64_t DataReductionProxyCompressionStats::GetHttpOriginalContentLength() {
  return GetInt64(prefs::kHttpOriginalContentLength);
}

ContentLengthList DataReductionProxyCompressionStats::GetDailyContentLengths(
    const char* pref_name) {
  ContentLengthList content_lengths;
  const base::ListValue* list_value = GetList(pref_name);
  if (list_value->GetSize() == kNumDaysInHistory) {
    for (size_t i = 0; i < kNumDaysInHistory; ++i)
      content_lengths.push_back(GetInt64PrefValue(*list_value, i));
  }
  return content_lengths;
}

void DataReductionProxyCompressionStats::GetContentLengths(
    unsigned int days,
    int64_t* original_content_length,
    int64_t* received_content_length,
    int64_t* last_update_time) {
  DCHECK_LE(days, kNumDaysInHistory);

  const base::ListValue* original_list =
      GetList(prefs::kDailyHttpOriginalContentLength);
  const base::ListValue* received_list =
      GetList(prefs::kDailyHttpReceivedContentLength);

  if (original_list->GetSize() != kNumDaysInHistory ||
      received_list->GetSize() != kNumDaysInHistory) {
    *original_content_length = 0L;
    *received_content_length = 0L;
    *last_update_time = 0L;
    return;
  }

  int64_t orig = 0L;
  int64_t recv = 0L;
  // Include days from the end of the list going backwards.
  for (size_t i = kNumDaysInHistory - days;
       i < kNumDaysInHistory; ++i) {
    orig += GetInt64PrefValue(*original_list, i);
    recv += GetInt64PrefValue(*received_list, i);
  }
  *original_content_length = orig;
  *received_content_length = recv;
  *last_update_time = GetInt64(prefs::kDailyHttpContentLengthLastUpdateDate);
}

void DataReductionProxyCompressionStats::GetHistoricalDataUsage(
    const HistoricalDataUsageCallback& get_data_usage_callback) {
  GetHistoricalDataUsageImpl(get_data_usage_callback, base::Time::Now());
}

void DataReductionProxyCompressionStats::DeleteBrowsingHistory(
    const base::Time& start,
    const base::Time& end) {
  DCHECK_NE(LOADING, current_data_usage_load_status_);

  if (!data_usage_map_last_updated_.is_null() &&
      DataUsageStore::BucketOverlapsInterval(data_usage_map_last_updated_,
                                             start, end)) {
    data_usage_map_.clear();
    data_usage_map_last_updated_ = base::Time();
    data_usage_map_is_dirty_ = false;
  }

  service_->DeleteBrowsingHistory(start, end);

  RecordSavingsClearedMetric(DataReductionProxySavingsClearedReason::
                                 USER_ACTION_DELETE_BROWSING_HISTORY);
}

void DataReductionProxyCompressionStats::OnCurrentDataUsageLoaded(
    std::unique_ptr<DataUsageBucket> data_usage) {
  // Exit early if the pref was turned off before loading from storage
  // completed.
  if (!data_usage_reporting_enabled_.GetValue()) {
    DCHECK_EQ(NOT_LOADED, current_data_usage_load_status_);
    DCHECK(data_usage_map_.empty());
    current_data_usage_load_status_ = NOT_LOADED;
    return;
  } else {
    DCHECK_EQ(LOADING, current_data_usage_load_status_);
  }

  DCHECK(data_usage_map_last_updated_.is_null());
  DCHECK(data_usage_map_.empty());

  // We currently do not break down by connection type. However, we use a schema
  // that makes it easy to transition to a connection based breakdown without
  // requiring a data migration.
  DCHECK(data_usage->connection_usage_size() == 0 ||
         data_usage->connection_usage_size() == 1);
  for (const auto& connection_usage : data_usage->connection_usage()) {
    for (const auto& site_usage : connection_usage.site_usage()) {
      data_usage_map_[site_usage.hostname()] =
          std::make_unique<PerSiteDataUsage>(site_usage);
    }
  }

  data_usage_map_last_updated_ =
      base::Time::FromInternalValue(data_usage->last_updated_timestamp());
  // Record if there was a read error.
  if (data_usage->had_read_error()) {
    RecordSavingsClearedMetric(
        DataReductionProxySavingsClearedReason::PREFS_PARSE_ERROR);
  }

  current_data_usage_load_status_ = LOADED;
}

void DataReductionProxyCompressionStats::SetDataUsageReportingEnabled(
    bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (data_usage_reporting_enabled_.GetValue() != enabled) {
    data_usage_reporting_enabled_.SetValue(enabled);
    OnDataUsageReportingPrefChanged();
  }
}

void DataReductionProxyCompressionStats::ClearDataSavingStatistics(
    DataReductionProxySavingsClearedReason reason) {
  DeleteHistoricalDataUsage();

  pref_service_->ClearPref(prefs::kDailyHttpContentLengthLastUpdateDate);
  pref_service_->ClearPref(prefs::kHttpReceivedContentLength);
  pref_service_->ClearPref(prefs::kHttpOriginalContentLength);

  pref_service_->ClearPref(prefs::kDailyHttpOriginalContentLength);
  pref_service_->ClearPref(prefs::kDailyHttpReceivedContentLength);

  for (auto iter = list_pref_map_.begin(); iter != list_pref_map_.end();
       ++iter) {
    iter->second->Clear();
  }

  RecordSavingsClearedMetric(reason);
}

void DataReductionProxyCompressionStats::DelayedWritePrefs() {
  if (pref_writer_timer_.IsRunning())
    return;

  pref_writer_timer_.Start(FROM_HERE, delay_, this,
                           &DataReductionProxyCompressionStats::WritePrefs);
}

void DataReductionProxyCompressionStats::TransferList(
    const base::ListValue& from_list,
    base::ListValue* to_list) {
  to_list->Clear();
  from_list.CreateDeepCopy()->Swap(to_list);
}

void DataReductionProxyCompressionStats::RecordRequestSizePrefs(
    int64_t data_used,
    int64_t original_size,
    bool with_data_saver_enabled,
    DataReductionProxyRequestType request_type,
    const std::string& mime_type,
    const base::Time& now) {
  // TODO(bengr): Remove this check once the underlying cause of
  // http://crbug.com/287821 is fixed. For now, only continue if the current
  // year is reported as being between 1972 and 2970.
  base::TimeDelta time_since_unix_epoch = now - base::Time::UnixEpoch();
  const int kMinDaysSinceUnixEpoch = 365 * 2;  // 2 years.
  const int kMaxDaysSinceUnixEpoch = 365 * 1000;  // 1000 years.
  if (time_since_unix_epoch.InDays() < kMinDaysSinceUnixEpoch ||
      time_since_unix_epoch.InDays() > kMaxDaysSinceUnixEpoch) {
    return;
  }

  // Determine how many days it has been since the last update.
  int64_t then_internal = GetInt64(
      data_reduction_proxy::prefs::kDailyHttpContentLengthLastUpdateDate);

  // Local midnight could have been shifted due to time zone change.
  // If time is null then don't care if midnight will be wrong shifted due to
  // time zone change because it's still too much time ago.
  base::Time then_midnight = base::Time::FromInternalValue(then_internal);
  if (!then_midnight.is_null()) {
    then_midnight = then_midnight.LocalMidnight();
  }
  base::Time midnight = now.LocalMidnight();

  DailyDataSavingUpdate total(
      this, data_reduction_proxy::prefs::kDailyHttpOriginalContentLength,
      data_reduction_proxy::prefs::kDailyHttpReceivedContentLength);

  int days_since_last_update = (midnight - then_midnight).InDays();
  if (days_since_last_update) {
    // Record the last update time in microseconds in UTC.
    SetInt64(data_reduction_proxy::prefs::kDailyHttpContentLengthLastUpdateDate,
             midnight.ToInternalValue());

    // The system may go backwards in time by up to a day for legitimate
    // reasons, such as with changes to the time zone. In such cases, we
    // keep adding to the current day.
    // (Actually resetting the numbers when we're more than a day off
    // happens elsewhere.)
    if (days_since_last_update < -1) {
      RecordSavingsClearedMetric(
          DataReductionProxySavingsClearedReason::SYSTEM_CLOCK_MOVED_BACK);
    }
  }

  total.UpdateForDateChange(days_since_last_update);
  total.Add(original_size, data_used);
}

void DataReductionProxyCompressionStats::RecordDataUseByHost(
    const std::string& data_usage_host,
    int64_t data_used,
    int64_t original_size,
    const base::Time time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (current_data_usage_load_status_ != LOADED)
    return;

  DCHECK(data_usage_reporting_enabled_.GetValue());

  if (!DataUsageStore::AreInSameInterval(data_usage_map_last_updated_, time)) {
    PersistDataUsage();
    data_usage_map_.clear();
    data_usage_map_last_updated_ = base::Time();
  }

  std::string normalized_host = NormalizeHostname(data_usage_host);
  auto j = data_usage_map_.insert(
      std::make_pair(normalized_host, std::make_unique<PerSiteDataUsage>()));
  PerSiteDataUsage* per_site_usage = j.first->second.get();
  per_site_usage->set_hostname(normalized_host);
  per_site_usage->set_original_size(per_site_usage->original_size() +
                                    original_size);
  per_site_usage->set_data_used(per_site_usage->data_used() + data_used);

  data_usage_map_last_updated_ = time;
  data_usage_map_is_dirty_ = true;
}

void DataReductionProxyCompressionStats::PersistDataUsage() {
  DCHECK(current_data_usage_load_status_ == LOADED);

  if (data_usage_map_is_dirty_) {
    std::unique_ptr<DataUsageBucket> data_usage_bucket(new DataUsageBucket());
    data_usage_bucket->set_last_updated_timestamp(
        data_usage_map_last_updated_.ToInternalValue());
    PerConnectionDataUsage* connection_usage =
        data_usage_bucket->add_connection_usage();
    for (auto i = data_usage_map_.begin(); i != data_usage_map_.end(); ++i) {
        PerSiteDataUsage* per_site_usage = connection_usage->add_site_usage();
        per_site_usage->CopyFrom(*(i->second.get()));
    }
    service_->StoreCurrentDataUsageBucket(std::move(data_usage_bucket));
  }

  data_usage_map_is_dirty_ = false;
}

void DataReductionProxyCompressionStats::DeleteHistoricalDataUsage() {
  // This method does not support being called in |LOADING| status since this
  // means that the in-memory data usage will get populated when data usage
  // loads, which will undo the clear below. This method is called when users
  // click on the "Clear Data" button, or when user deletes the extension. In
  // both cases, enough time has passed since startup to load current data
  // usage. Technically, this could occur, and will have the effect of not
  // clearing data from the current bucket.
  // TODO(kundaji): Use cancellable tasks and remove this DCHECK.
  DCHECK(current_data_usage_load_status_ != LOADING);

  data_usage_map_.clear();
  data_usage_map_last_updated_ = base::Time();
  data_usage_map_is_dirty_ = false;

  service_->DeleteHistoricalDataUsage();
}

void DataReductionProxyCompressionStats::GetHistoricalDataUsageImpl(
    const HistoricalDataUsageCallback& get_data_usage_callback,
    const base::Time& now) {
#if !defined(OS_ANDROID)
  if (current_data_usage_load_status_ != LOADED) {
    // If current data usage has not yet loaded, we return an empty array. The
    // extension can retry after a slight delay.
    // This use case is unlikely to occur in practice since current data usage
    // should have sufficient time to load before user tries to view data usage.
    get_data_usage_callback.Run(
        std::make_unique<std::vector<DataUsageBucket>>());
    return;
  }
#endif

  if (current_data_usage_load_status_ == LOADED)
    PersistDataUsage();

  if (!data_usage_map_last_updated_.is_null() &&
      !DataUsageStore::AreInSameInterval(data_usage_map_last_updated_, now)) {
    data_usage_map_.clear();
    data_usage_map_last_updated_ = base::Time();

    // Force the last bucket to be for the current interval.
    std::unique_ptr<DataUsageBucket> data_usage_bucket(new DataUsageBucket());
    data_usage_bucket->set_last_updated_timestamp(now.ToInternalValue());
    service_->StoreCurrentDataUsageBucket(std::move(data_usage_bucket));
  }

  service_->LoadHistoricalDataUsage(get_data_usage_callback);
}

void DataReductionProxyCompressionStats::OnDataUsageReportingPrefChanged() {
  if (data_usage_reporting_enabled_.GetValue()) {
    if (current_data_usage_load_status_ == NOT_LOADED) {
      current_data_usage_load_status_ = LOADING;
      service_->LoadCurrentDataUsageBucket(base::Bind(
          &DataReductionProxyCompressionStats::OnCurrentDataUsageLoaded,
          weak_factory_.GetWeakPtr()));
    }
  } else {
// Don't delete the historical data on Android, but clear the map.
#if defined(OS_ANDROID)
    if (current_data_usage_load_status_ == LOADED)
      PersistDataUsage();

    data_usage_map_.clear();
    data_usage_map_last_updated_ = base::Time();
    data_usage_map_is_dirty_ = false;
#else
    DeleteHistoricalDataUsage();
#endif
    current_data_usage_load_status_ = NOT_LOADED;
  }
}

void DataReductionProxyCompressionStats::InitializeWeeklyAggregateDataUse(
    const base::Time& now) {
#if defined(OS_ANDROID) && defined(ARCH_CPU_X86)
  // TODO(rajendrant): Enable aggregate metrics recording in x86 Android.
  // http://crbug.com/865373
  return;
#endif

  MaybeInitWeeklyAggregateDataUsePrefs(now, pref_service_);
  // Record the histograms that will show up in the user feedback.
  RecordDictionaryToHistogram(
      "DataReductionProxy.ThisWeekAggregateKB.Services.Downstream.Background",
      pref_service_->GetDictionary(
          prefs::kThisWeekServicesDownstreamBackgroundKB));
  RecordDictionaryToHistogram(
      "DataReductionProxy.ThisWeekAggregateKB.Services.Downstream.Foreground",
      pref_service_->GetDictionary(
          prefs::kThisWeekServicesDownstreamForegroundKB));
  RecordDictionaryToHistogram(
      "DataReductionProxy.ThisWeekAggregateKB.UserTraffic.Downstream."
      "ContentType",
      pref_service_->GetDictionary(
          prefs::kThisWeekUserTrafficContentTypeDownstreamKB));
  RecordDictionaryToHistogram(
      "DataReductionProxy.LastWeekAggregateKB.Services.Downstream.Background",
      pref_service_->GetDictionary(
          prefs::kLastWeekServicesDownstreamBackgroundKB));
  RecordDictionaryToHistogram(
      "DataReductionProxy.LastWeekAggregateKB.Services.Downstream.Foreground",
      pref_service_->GetDictionary(
          prefs::kLastWeekServicesDownstreamForegroundKB));
  RecordDictionaryToHistogram(
      "DataReductionProxy.LastWeekAggregateKB.UserTraffic.Downstream."
      "ContentType",
      pref_service_->GetDictionary(
          prefs::kLastWeekUserTrafficContentTypeDownstreamKB));
}

void DataReductionProxyCompressionStats::RecordWeeklyAggregateDataUse(
    const base::Time& now,
    int32_t data_used_kb,
    bool is_user_request,
    data_use_measurement::DataUseUserData::DataUseContentType content_type,
    int32_t service_hash_code) {
#if defined(OS_ANDROID) && defined(ARCH_CPU_X86)
  // TODO(rajendrant): Enable aggregate metrics recording in x86 Android.
  // http://crbug.com/865373
  return;
#endif
  // Update the prefs if this is a new week. This can happen when chrome is open
  // for weeks without being closed.
  MaybeInitWeeklyAggregateDataUsePrefs(now, pref_service_);
  if (is_user_request) {
    AddToDictionaryPref(pref_service_,
                        prefs::kThisWeekUserTrafficContentTypeDownstreamKB,
                        content_type, data_used_kb);
  } else {
    bool is_app_foreground = true;
    if (is_app_foreground) {
      AddToDictionaryPref(pref_service_,
                          prefs::kThisWeekServicesDownstreamForegroundKB,
                          service_hash_code, data_used_kb);
    } else {
      AddToDictionaryPref(pref_service_,
                          prefs::kThisWeekServicesDownstreamBackgroundKB,
                          service_hash_code, data_used_kb);
    }
  }
}

// static
std::string DataReductionProxyCompressionStats::NormalizeHostname(
    const std::string& host) {
  size_t pos = host.find("://");
  if (pos != std::string::npos)
    return host.substr(pos + 3);

  return host;
}

}  // namespace data_reduction_proxy
