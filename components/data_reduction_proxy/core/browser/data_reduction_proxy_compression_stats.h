// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_COMPRESSION_STATS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_COMPRESSION_STATS_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/db_data_owner.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/prefs/pref_member.h"

class PrefService;

namespace base {
class ListValue;
class Value;
}

namespace data_reduction_proxy {
class DataReductionProxyService;
class DataUsageBucket;
class PerSiteDataUsage;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
//     org.chromium.chrome.browser.settings.datareduction)
enum class DataReductionProxySavingsClearedReason {
  SYSTEM_CLOCK_MOVED_BACK,
  PREFS_PARSE_ERROR,
  USER_ACTION_EXTENSION,
  USER_ACTION_SETTINGS_MENU,
  USER_ACTION_DELETE_BROWSING_HISTORY,
  // NOTE: always keep this entry at the end. Add new result types only
  // immediately above this line. Make sure to update the corresponding
  // histogram enum accordingly.
  REASON_COUNT,
};

// Data reduction proxy delayed pref service reduces the number calls to pref
// service by storing prefs in memory and writing to the given PrefService after
// |delay| amount of time. If |delay| is zero, the delayed pref service writes
// directly to the PrefService and does not store the prefs in memory. All
// prefs must be stored and read on the UI thread.
class DataReductionProxyCompressionStats {
 public:
  typedef std::unordered_map<std::string, std::unique_ptr<PerSiteDataUsage>>
      SiteUsageMap;

  // Collects and store data usage and compression statistics. Basic data usage
  // stats are stored in browser preferences. More detailed stats broken down
  // by site and internet type are stored in |DataReductionProxyStore|.
  //
  // To store basic stats, it constructs a data reduction proxy delayed pref
  // service object using |pref_service|. Writes prefs to |pref_service| after
  // |delay| and stores them in |pref_map_| and |list_pref_map| between writes.
  // If |delay| is zero, writes directly to the PrefService and does not store
  // in the maps.
  DataReductionProxyCompressionStats(DataReductionProxyService* service,
                                     PrefService* pref_service,
                                     const base::TimeDelta& delay);
  ~DataReductionProxyCompressionStats();

  // Records detailed data usage broken down by |mime_type|. Also records daily
  // data savings statistics to prefs and reports data savings UMA. |data_used|
  // and |original_size| are measured in bytes.
  void RecordDataUseWithMimeType(
      int64_t compressed_size,
      int64_t original_size,
      bool data_reduction_proxy_enabled,
      DataReductionProxyRequestType request_type,
      const std::string& mime_type,
      bool is_user_traffic,
      data_use_measurement::DataUseUserData::DataUseContentType content_type,
      int32_t service_hash_code);

  // Record data usage and original size of request broken down by host.
  // |original_request_size| and |data_used| are in bytes. |time| is the time at
  // which the data usage occurred. This method should be called in real time,
  // so |time| is expected to be |Time::Now()|.
  void RecordDataUseByHost(const std::string& data_usage_host,
                           int64_t original_request_size,
                           int64_t data_used,
                           const base::Time time);

  // Returns the time in milliseconds since epoch that the last update was made
  // to the daily original and received content lengths.
  int64_t GetLastUpdateTime();

  // Resets daily content length statistics.
  void ResetStatistics();

  // Clears all data saving statistics for the given |reason|.
  void ClearDataSavingStatistics(DataReductionProxySavingsClearedReason reason);

  // Returns the total size of all HTTP content received from the network.
  int64_t GetHttpReceivedContentLength();

  // Returns the value the total original size of all HTTP content received from
  // the network.
  int64_t GetHttpOriginalContentLength();

  // Returns a list of all the daily content lengths.
  ContentLengthList GetDailyContentLengths(const char* pref_name);

  // Returns aggregate received and original content lengths over the specified
  // number of days, as well as the time these stats were last updated.
  void GetContentLengths(unsigned int days,
                         int64_t* original_content_length,
                         int64_t* received_content_length,
                         int64_t* last_update_time);

  // Calls |get_data_usage_callback| with full data usage history. In-memory
  // data usage stats are flushed to storage before querying for full history.
  // An empty vector will be returned if "data_usage_reporting.enabled" pref is
  // not enabled or if called immediately after enabling the pref before
  // in-memory stats could be initialized from storage. Data usage is sorted
  // chronologically with the last entry corresponding to |base::Time::Now()|.
  void GetHistoricalDataUsage(
      HistoricalDataUsageCallback get_data_usage_callback);

  // Deletes browsing history from storage and memory for the given time
  // range. Currently, this method deletes all data usage for the given range.
  void DeleteBrowsingHistory(const base::Time& start, const base::Time& end);

  // Callback from loading detailed data usage. Initializes in memory data
  // structures used to collect data usage. |data_usage| contains the data usage
  // for the last stored interval.
  void OnCurrentDataUsageLoaded(std::unique_ptr<DataUsageBucket> data_usage);

  // Sets the value of |prefs::kDataUsageReportingEnabled| to |enabled|.
  // Initializes data usage statistics in memory when pref is enabled and
  // persists data usage to memory when pref is disabled.
  void SetDataUsageReportingEnabled(bool enabled);

  // Returns |data_usage_map_|.
  const DataReductionProxyCompressionStats::SiteUsageMap&
  DataUsageMapForTesting() const {
    return data_usage_map_;
  }

 private:
  // Enum to track the state of loading data usage from storage.
  enum CurrentDataUsageLoadStatus { NOT_LOADED = 0, LOADING = 1, LOADED = 2 };

  friend class DataReductionProxyCompressionStatsTest;

  typedef std::map<const char*, int64_t> DataReductionProxyPrefMap;
  typedef std::unordered_map<const char*, std::unique_ptr<base::ListValue>>
      DataReductionProxyListPrefMap;

  class DailyContentLengthUpdate;
  class DailyDataSavingUpdate;

  // Loads all data_reduction_proxy::prefs into the |pref_map_| and
  // |list_pref_map_|.
  void Init();

  // Gets the value of |pref| from the pref service and adds it to the
  // |pref_map|.
  void InitInt64Pref(const char* pref);

  // Gets the value of |pref| from the pref service and adds it to the
  // |list_pref_map|.
  void InitListPref(const char* pref);

  void OnUpdateContentLengths();

  // Gets the int64_t pref at |pref_path| from the |DataReductionProxyPrefMap|.
  int64_t GetInt64(const char* pref_path);

  // Updates the pref value in the |DataReductionProxyPrefMap| map.
  // The pref is later written to |pref service_|.
  void SetInt64(const char* pref_path, int64_t pref_value);

  // Increases the pref value in the |DataReductionProxyPrefMap| map.
  // The pref is later written to |pref service_|.
  void IncreaseInt64Pref(const char* pref_path, int64_t delta);

  // Gets the pref list at |pref_path| from the |DataReductionProxyPrefMap|.
  base::ListValue* GetList(const char* pref_path);

  // Writes the prefs stored in |DataReductionProxyPrefMap| and
  // |DataReductionProxyListPrefMap| to |pref_service|.
  void WritePrefs();

  // Starts a timer (if necessary) to write prefs in |kMinutesBetweenWrites| to
  // the |pref_service|.
  void DelayedWritePrefs();

  // Copies the values at each index of |from_list| to the same index in
  // |to_list|.
  void TransferList(const base::ListValue& from_list,
                    base::ListValue* to_list);

  // Records content length updates to prefs.
  void RecordRequestSizePrefs(int64_t compressed_size,
                              int64_t original_size,
                              bool with_data_reduction_proxy_enabled,
                              DataReductionProxyRequestType request_type,
                              const std::string& mime_type,
                              const base::Time& now);

  void IncrementDailyUmaPrefs(int64_t original_size,
                              int64_t received_size,
                              const char* original_size_pref,
                              const char* received_size_pref,
                              bool data_reduction_proxy_enabled,
                              const char* original_size_with_proxy_enabled_pref,
                              const char* recevied_size_with_proxy_enabled_pref,
                              bool via_data_reduction_proxy,
                              const char* original_size_via_proxy_pref,
                              const char* received_size_via_proxy_pref);

  // Persists the in memory data usage information to storage and clears all
  // in-memory data usage. Do not call this method unless |data_usage_loaded_|
  // is |LOADED|.
  void PersistDataUsage();

  // Deletes all historical data usage from storage and memory. This method
  // should not be called when |current_data_usage_load_status_| is |LOADING|.
  void DeleteHistoricalDataUsage();

  // Actual implementation of |GetHistoricalDataUsage|. This helper method
  // explicitly passes |base::Time::Now()| to make testing easier.
  void GetHistoricalDataUsageImpl(
      HistoricalDataUsageCallback get_data_usage_callback,
      const base::Time& now);

  // Called when |prefs::kDataUsageReportingEnabled| pref values changes.
  // Initializes data usage statistics in memory when pref is enabled and
  // persists data usage to memory when pref is disabled.
  void OnDataUsageReportingPrefChanged();

  // Initialize the weekly data use prefs for the current week, and records the
  // weekly aggregate data use histograms.
  void InitializeWeeklyAggregateDataUse(const base::Time& now);

  // Records |data_used_kb| to the current week data use pref. |is_user_request|
  // indicates if this is user-initiated traffic or chrome services traffic, and
  // |service_hash_code| uniquely identifies the corresponding chrome service.
  void RecordWeeklyAggregateDataUse(
      const base::Time& now,
      int32_t data_used_kb,
      bool is_user_request,
      data_use_measurement::DataUseUserData::DataUseContentType content_type,
      int32_t service_hash_code);

  // Normalizes the hostname for data usage attribution. Returns a substring
  // without the protocol.
  // Example: "http://www.finance.google.com" -> "www.finance.google.com"
  static std::string NormalizeHostname(const std::string& host);

  DataReductionProxyService* service_;
  PrefService* pref_service_;
  const base::TimeDelta delay_;
  DataReductionProxyPrefMap pref_map_;
  DataReductionProxyListPrefMap list_pref_map_;
  BooleanPrefMember data_usage_reporting_enabled_;

  // Maintains detailed data usage for current interval.
  SiteUsageMap data_usage_map_;

  // Time when |data_usage_map_| was last updated. Contains NULL time if
  // |data_usage_map_| does not have any data. This could happen either because
  // current data usage has not yet been loaded from storage, or because
  // no data usage has ever been recorded.
  base::Time data_usage_map_last_updated_;

  // Tracks whether |data_usage_map_| has changes that have not yet been
  // persisted to storage.
  bool data_usage_map_is_dirty_;

  // Tracks state of loading data usage from storage.
  CurrentDataUsageLoadStatus current_data_usage_load_status_;

  base::OneShotTimer pref_writer_timer_;
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<DataReductionProxyCompressionStats> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyCompressionStats);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_COMPRESSION_STATS_H_
