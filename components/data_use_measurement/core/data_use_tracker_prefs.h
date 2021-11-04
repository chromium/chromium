// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_TRACKER_PREFS_H_
#define COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_TRACKER_PREFS_H_

#include <memory>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"

class PrefRegistrySimple;
class PrefService;

namespace data_use_measurement {

// DataUseTrackerPrefs keeps track of the data used over last 60 days. The data
// used is recorded separately based on whether the data use was initiated by a
// Chrome service or a user-initiated request.
class DataUseTrackerPrefs {
 public:
  // |pref_service| may be null in tests.
  DataUseTrackerPrefs(const base::Clock* time_clock, PrefService* pref_service);

  // Move-only class.
  DataUseTrackerPrefs(const DataUseTrackerPrefs&) = delete;
  DataUseTrackerPrefs& operator=(const DataUseTrackerPrefs&) = delete;

  // Report data used by a service or a user-initiated request.
  // |is_metered_connection| should be true if data consumption happened on a
  // metered connection. |is_app_foreground| should be true if data was used
  // when Chrome app was in foregorund. |is_user_traffic| should be true if data
  // was used by a user-initiated request. |sent_or_recv_bytes| should be set to
  // the data consumed (in bytes).
  void ReportNetworkServiceDataUse(bool is_metered_connection,
                                   bool is_app_foreground,
                                   bool is_user_traffic,
                                   int64_t sent_or_recv_bytes);

  // Register local state prefs.
  static void RegisterDataUseTrackerLocalStatePrefs(
      PrefRegistrySimple* registry);

 private:
  // Returns the current date for measurement.
  base::Time GetCurrentMeasurementDate() const;

  // Removes entries from the given |pref_name| if they are too old.
  void RemoveExpiredEntriesForPref(const std::string& pref_name);

  // Returns the current date as a string with a proper formatting.
  std::string GetCurrentMeasurementDateAsString() const;

  // Updates provided |pref_name| for a current date with the given message
  // size.
  void UpdateUsagePref(const std::string& pref_name,
                       int64_t message_size_bytes);

  const base::Clock* time_clock_;
  PrefService* pref_service_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace data_use_measurement

#endif  // COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_TRACKER_PREFS_H_
