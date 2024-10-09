// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/data_use_tracker.h"

#include <memory>
#include <string>

#include "base/i18n/time_formatting.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace metrics {

namespace {

// Default weekly quota and allowed UMA ratio for UMA log uploads for Android.
// These defaults will not be used for non-Android as |DataUseTracker| will not
// be initialized.
const int kDefaultUMAWeeklyQuotaBytes = 200 * 1024;  // 200KB.
const double kDefaultUMARatio = 0.05;

}  // namespace

DataUseTracker::DataUseTracker(PrefService* local_state)
    : local_state_(local_state) {}

DataUseTracker::~DataUseTracker() = default;

// static
std::unique_ptr<DataUseTracker> DataUseTracker::Create(
    PrefService* local_state) {
  std::unique_ptr<DataUseTracker> data_use_tracker;
// Instantiate DataUseTracker only on Android. UpdateMetricsUsagePrefs() honors
// this rule too.
#if BUILDFLAG(IS_ANDROID)
  data_use_tracker = std::make_unique<DataUseTracker>(local_state);
#endif
  return data_use_tracker;
}

// static
void DataUseTracker::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(metrics::prefs::kUserCellDataUse);
  registry->RegisterDictionaryPref(metrics::prefs::kUmaCellDataUse);
}

// static
void DataUseTracker::UpdateMetricsUsagePrefs(int message_size,
                                             bool is_cellular,
                                             bool is_metrics_service_usage,
                                             PrefService* local_state) {
// Instantiate DataUseTracker only on Android. Create() honors this rule too.
#if BUILDFLAG(IS_ANDROID)
  metrics::DataUseTracker tracker(local_state);
  tracker.UpdateMetricsUsagePrefsInternal(message_size, is_cellular,
                                          is_metrics_service_usage);
#endif  // BUILDFLAG(IS_ANDROID)
}

void DataUseTracker::UpdateMetricsUsagePrefsInternal(
    int message_size,
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

  int uma_total_data_use = ComputeTotalDataUse(prefs::kUmaCellDataUse);
  int new_uma_total_data_use = log_bytes + uma_total_data_use;
  // If the new log doesn't increase the total UMA traffic to be above the
  // allowed quota then the log should be uploaded.
  if (new_uma_total_data_use <= kDefaultUMAWeeklyQuotaBytes) {
    return true;
  }

  int user_total_data_use = ComputeTotalDataUse(prefs::kUserCellDataUse);
  // If after adding the new log the uma ratio is still under the allowed ratio
  // then the log should be uploaded and vice versa.
  return new_uma_total_data_use /
             static_cast<double>(log_bytes + user_total_data_use) <=
         kDefaultUMARatio;
}

void DataUseTracker::UpdateUsagePref(const std::string& pref_name,
                                     int message_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ScopedDictPrefUpdate pref_updater(local_state_, pref_name);
  std::string todays_key = GetCurrentMeasurementDateAsString();

  const base::Value::Dict& user_pref_dict = local_state_->GetDict(pref_name);
  int todays_traffic = user_pref_dict.FindInt(todays_key).value_or(0);
  pref_updater->Set(todays_key, todays_traffic + message_size);
}

void DataUseTracker::RemoveExpiredEntries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveExpiredEntriesForPref(prefs::kUmaCellDataUse);
  RemoveExpiredEntriesForPref(prefs::kUserCellDataUse);
}

void DataUseTracker::RemoveExpiredEntriesForPref(const std::string& pref_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value::Dict& user_pref_dict = local_state_->GetDict(pref_name);
  const base::Time current_date = GetCurrentMeasurementDate();
  const base::Time week_ago = current_date - base::Days(7);

  base::Value::Dict user_pref_new_dict;
  for (const auto it : user_pref_dict) {
    base::Time key_date;
    if (base::Time::FromUTCString(it.first.c_str(), &key_date) &&
        key_date > week_ago) {
      user_pref_new_dict.Set(it.first, it.second.Clone());
    }
  }
  local_state_->SetDict(pref_name, std::move(user_pref_new_dict));
}

// Note: We compute total data use regardless of what is the current date. In
// scenario when user travels back in time zone and current date becomes earlier
// than latest registered date in perf, we still count that in total use as user
// actually used that data.
int DataUseTracker::ComputeTotalDataUse(const std::string& pref_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int total_data_use = 0;
  const base::Value::Dict& pref_dict = local_state_->GetDict(pref_name);
  for (const auto it : pref_dict) {
    total_data_use += it.second.GetIfInt().value_or(0);
  }
  return total_data_use;
}

base::Time DataUseTracker::GetCurrentMeasurementDate() const {
  return base::Time::Now().LocalMidnight();
}

std::string DataUseTracker::GetCurrentMeasurementDateAsString() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::UnlocalizedTimeFormatWithPattern(GetCurrentMeasurementDate(),
                                                "yyyy-MM-dd");
}

}  // namespace metrics
