// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use_tracker_prefs.h"

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/data_use_measurement/core/data_use_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace data_use_measurement {

DataUseTrackerPrefs::DataUseTrackerPrefs(const base::Clock* time_clock,
                                         PrefService* pref_service)
    : time_clock_(time_clock), pref_service_(pref_service) {
  DCHECK(time_clock_);

  RemoveExpiredEntriesForPref(prefs::kDataUsedUserForeground);
  RemoveExpiredEntriesForPref(prefs::kDataUsedUserBackground);
  RemoveExpiredEntriesForPref(prefs::kDataUsedServicesForeground);
  RemoveExpiredEntriesForPref(prefs::kDataUsedServicesBackground);
}

void DataUseTrackerPrefs::ReportNetworkServiceDataUse(
    bool is_metered_connection,
    bool is_app_foreground,
    bool is_user_traffic,
    int64_t sent_or_recv_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_metered_connection)
    return;

  if (sent_or_recv_bytes <= 0)
    return;

  if (is_user_traffic && is_app_foreground) {
    UpdateUsagePref(prefs::kDataUsedUserForeground, sent_or_recv_bytes);
  } else if (is_user_traffic && !is_app_foreground) {
    UpdateUsagePref(prefs::kDataUsedUserBackground, sent_or_recv_bytes);
  } else if (!is_user_traffic && is_app_foreground) {
    UpdateUsagePref(prefs::kDataUsedServicesForeground, sent_or_recv_bytes);
  } else {
    UpdateUsagePref(prefs::kDataUsedServicesBackground, sent_or_recv_bytes);
  }
}

base::Time DataUseTrackerPrefs::GetCurrentMeasurementDate() const {
  return time_clock_->Now().LocalMidnight();
}

void DataUseTrackerPrefs::RemoveExpiredEntriesForPref(
    const std::string& pref_name) {
  if (!pref_service_)
    return;

  const base::DictionaryValue* user_pref_dict =
      pref_service_->GetDictionary(pref_name);
  const base::Time current_date = GetCurrentMeasurementDate();
  const base::Time last_date = current_date - base::Days(60);

  base::DictionaryValue user_pref_new_dict;
  for (auto it : user_pref_dict->DictItems()) {
    base::Time key_date;
    if (base::Time::FromUTCString(it.first.c_str(), &key_date) &&
        key_date > last_date) {
      user_pref_new_dict.Set(it.first,
                             base::Value::ToUniquePtrValue(it.second.Clone()));
    }
  }
  pref_service_->Set(pref_name, user_pref_new_dict);
}

std::string DataUseTrackerPrefs::GetCurrentMeasurementDateAsString() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Time::Exploded today_exploded;
  GetCurrentMeasurementDate().LocalExplode(&today_exploded);
  std::string date =
      base::StringPrintf("%04d-%02d-%02d", today_exploded.year,
                         today_exploded.month, today_exploded.day_of_month);
  return date;
}

void DataUseTrackerPrefs::UpdateUsagePref(const std::string& pref_name,
                                          int64_t message_size_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!pref_service_)
    return;

  DictionaryPrefUpdate pref_updater(pref_service_, pref_name);
  std::string todays_key = GetCurrentMeasurementDateAsString();

  const base::DictionaryValue* user_pref_dict =
      pref_service_->GetDictionary(pref_name);
  double todays_traffic = user_pref_dict->FindDoubleKey(todays_key).value_or(0);
  pref_updater->SetDouble(
      todays_key,
      todays_traffic + (static_cast<double>(message_size_bytes) / 1024.0));
}

// static
void DataUseTrackerPrefs::RegisterDataUseTrackerLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kDataUsedUserForeground,
                                   PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(prefs::kDataUsedUserBackground,
                                   PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(prefs::kDataUsedServicesForeground,
                                   PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(prefs::kDataUsedServicesBackground,
                                   PrefRegistry::LOSSY_PREF);
}

}  // namespace data_use_measurement
