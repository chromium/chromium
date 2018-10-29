// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/data_use_tracker.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/variations/variations_associated_data.h"

namespace metrics {

namespace {

// Default weekly quota and allowed UMA ratio for UMA log uploads for Android.
// These defaults will not be used for non-Android as |DataUseTracker| will not
// be initialized. Default values can be overridden by variation params.
const int kDefaultUMAWeeklyQuotaBytes = 204800;
const double kDefaultUMARatio = 0.05;

}  // namespace

DataUseTracker::DataUseTracker(PrefService* local_state)
    : local_state_(local_state) {}

DataUseTracker::~DataUseTracker() {}

// static
std::unique_ptr<DataUseTracker> DataUseTracker::Create(
    PrefService* local_state) {
  std::unique_ptr<DataUseTracker> data_use_tracker;
#if defined(OS_ANDROID)
  data_use_tracker.reset(new DataUseTracker(local_state));
#endif
  return data_use_tracker;
}

// static
void DataUseTracker::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(metrics::prefs::kUserCellDataUse);
  registry->RegisterDictionaryPref(metrics::prefs::kUmaCellDataUse);
}

void DataUseTracker::UpdateMetricsUsagePrefs(int message_size,
                                             bool is_cellular,
                                             bool is_metrics_service_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_cellular)
    return;

  UpdateUsagePref(prefs::kUserCellDataUse, message_size);
  // TODO(holte): Consider adding seperate tracking for UKM.
  if (is_metrics_service_usage)
    UpdateUsagePref(prefs::kUmaCellDataUse, message_size);
}

bool DataUseTracker::ShouldUploadLogOnCellular(int log_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RemoveExpiredEntries();

  int uma_weekly_quota_bytes;
  if (!GetUmaWeeklyQuota(&uma_weekly_quota_bytes))
    return true;

  int uma_total_data_use = ComputeTotalDataUse(prefs::kUmaCellDataUse);
  int new_uma_total_data_use = log_bytes + uma_total_data_use;
  // If the new log doesn't increase the total UMA traffic to be above the
  // allowed quota then the log should be uploaded.
  if (new_uma_total_data_use <= uma_weekly_quota_bytes)
    return true;

  double uma_ratio;
  if (!GetUmaRatio(&uma_ratio))
    return true;

  int user_total_data_use = ComputeTotalDataUse(prefs::kUserCellDataUse);
  // If after adding the new log the uma ratio is still under the allowed ratio
  // then the log should be uploaded and vice versa.
  return new_uma_total_data_use /
             static_cast<double>(log_bytes + user_total_data_use) <=
         uma_ratio;
}

void DataUseTracker::UpdateUsagePref(const std::string& pref_name,
                                     int message_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DictionaryPrefUpdate pref_updater(local_state_, pref_name);
  int todays_traffic = 0;
  std::string todays_key = GetCurrentMeasurementDateAsString();

  const base::DictionaryValue* user_pref_dict =
      local_state_->GetDictionary(pref_name);
  user_pref_dict->GetInteger(todays_key, &todays_traffic);
  pref_updater->SetInteger(todays_key, todays_traffic + message_size);
}

void DataUseTracker::RemoveExpiredEntries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveExpiredEntriesForPref(prefs::kUmaCellDataUse);
  RemoveExpiredEntriesForPref(prefs::kUserCellDataUse);
}

void DataUseTracker::RemoveExpiredEntriesForPref(const std::string& pref_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::DictionaryValue* user_pref_dict =
      local_state_->GetDictionary(pref_name);
  const base::Time current_date = GetCurrentMeasurementDate();
  const base::Time week_ago = current_date - base::TimeDelta::FromDays(7);

  base::DictionaryValue user_pref_new_dict;
  for (base::DictionaryValue::Iterator it(*user_pref_dict); !it.IsAtEnd();
       it.Advance()) {
    base::Time key_date;
    if (base::Time::FromUTCString(it.key().c_str(), &key_date) &&
        key_date > week_ago)
      user_pref_new_dict.Set(it.key(), it.value().CreateDeepCopy());
  }
  local_state_->Set(pref_name, user_pref_new_dict);
}

// Note: We compute total data use regardless of what is the current date. In
// scenario when user travels back in time zone and current date becomes earlier
// than latest registered date in perf, we still count that in total use as user
// actually used that data.
int DataUseTracker::ComputeTotalDataUse(const std::string& pref_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int total_data_use = 0;
  const base::DictionaryValue* pref_dict =
      local_state_->GetDictionary(pref_name);
  for (base::DictionaryValue::Iterator it(*pref_dict); !it.IsAtEnd();
       it.Advance()) {
    int value = 0;
    it.value().GetAsInteger(&value);
    total_data_use += value;
  }
  return total_data_use;
}

bool DataUseTracker::GetUmaWeeklyQuota(int* uma_weekly_quota_bytes) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string param_value_str = variations::GetVariationParamValue(
      "UMA_EnableCellularLogUpload", "Uma_Quota");
  if (param_value_str.empty())
    *uma_weekly_quota_bytes = kDefaultUMAWeeklyQuotaBytes;
  else
    base::StringToInt(param_value_str, uma_weekly_quota_bytes);
  return true;
}

bool DataUseTracker::GetUmaRatio(double* ratio) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string param_value_str = variations::GetVariationParamValue(
      "UMA_EnableCellularLogUpload", "Uma_Ratio");
  if (param_value_str.empty())
    *ratio = kDefaultUMARatio;
  else
    base::StringToDouble(param_value_str, ratio);
  return true;
}

base::Time DataUseTracker::GetCurrentMeasurementDate() const {
  return base::Time::Now().LocalMidnight();
}

std::string DataUseTracker::GetCurrentMeasurementDateAsString() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Time::Exploded today_exploded;
  GetCurrentMeasurementDate().LocalExplode(&today_exploded);
  return base::StringPrintf("%04d-%02d-%02d", today_exploded.year,
                            today_exploded.month, today_exploded.day_of_month);
}

}  // namespace metrics
