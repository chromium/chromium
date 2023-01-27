// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DATA_USE_TRACKER_H_
#define COMPONENTS_METRICS_DATA_USE_TRACKER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace metrics {

// Records the data use of user traffic and UMA traffic in user prefs. Taking
// into account those prefs it can verify whether certain UMA log upload is
// allowed.
class DataUseTracker {
 public:
  explicit DataUseTracker(PrefService* local_state);

  DataUseTracker(const DataUseTracker&) = delete;
  DataUseTracker& operator=(const DataUseTracker&) = delete;

  virtual ~DataUseTracker();

  // Returns an instance of |DataUseTracker| with provided |local_state| if
  // users data use should be tracked and null pointer otherwise.
  static std::unique_ptr<DataUseTracker> Create(PrefService* local_state);

  // Registers data use prefs using provided |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Updates data usage tracking prefs with the specified values.
  static void UpdateMetricsUsagePrefs(int message_size,
                                      bool is_cellular,
                                      bool is_metrics_service_usage,
                                      PrefService* local_state);

  // Returns whether a log with provided |log_bytes| can be uploaded according
  // to data use ratio and UMA quota provided by variations.
  bool ShouldUploadLogOnCellular(int log_bytes);

 private:
  FRIEND_TEST_ALL_PREFIXES(DataUseTrackerTest, CheckUpdateUsagePref);
  FRIEND_TEST_ALL_PREFIXES(DataUseTrackerTest, CheckRemoveExpiredEntries);
  FRIEND_TEST_ALL_PREFIXES(DataUseTrackerTest, CheckComputeTotalDataUse);
  FRIEND_TEST_ALL_PREFIXES(DataUseTrackerTest, CheckCanUploadUMALog);

  // Updates data usage tracking prefs with the specified values.
  void UpdateMetricsUsagePrefsInternal(int message_size,
                                       bool is_cellular,
                                       bool is_metrics_service_usage);

  // Updates provided |pref_name| for a current date with the given message
  // size.
  void UpdateUsagePref(const std::string& pref_name, int message_size);

  // Removes entries from the all data use  prefs.
  void RemoveExpiredEntries();

  // Removes entries from the given |pref_name| if they are more than 7 days
  // old.
  void RemoveExpiredEntriesForPref(const std::string& pref_name);

  // Computes data usage according to all the entries in the given dictionary
  // pref.
  int ComputeTotalDataUse(const std::string& pref_name);

  // Returns the current date for measurement.
  virtual base::Time GetCurrentMeasurementDate() const;

  // Returns the current date as a string with a proper formatting.
  virtual std::string GetCurrentMeasurementDateAsString() const;

  raw_ptr<PrefService> local_state_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace metrics
#endif  // COMPONENTS_METRICS_DATA_USE_TRACKER_H_
