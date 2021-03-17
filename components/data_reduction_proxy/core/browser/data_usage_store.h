// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_USAGE_STORE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_USAGE_STORE_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"

namespace base {
class Time;
}

namespace data_reduction_proxy {
class DataStore;
class DataUsageBucket;

// Time interval for each DataUsageBucket.
constexpr int kDataUsageBucketLengthInMinutes = 15;

// Number of days for which to maintain data usage history.
constexpr int kDataUsageHistoryNumDays = 60;

// Store for detailed data usage stats. Data usage from every
// |kDataUsageBucketLengthMins| interval is stored in a DataUsageBucket.
class DataUsageStore {
 public:
  explicit DataUsageStore(DataStore* db);

  ~DataUsageStore();

  // Loads the historic data usage into |data_usage|.
  void LoadDataUsage(std::vector<DataUsageBucket>* data_usage);

  // Loads the data usage bucket for the current interval into |current_bucket|.
  // This method must be called at least once before any calls to
  // |StoreCurrentDataUsageBucket|.
  void LoadCurrentDataUsageBucket(DataUsageBucket* bucket);

  // Stores the data usage bucket for the current interval. This will overwrite
  // the current data usage bucket in the |db_| if they are for the same
  // interval. It will also backfill any missed intervals with empty data.
  // Intervals might be missed because Chrome was not running, or there was no
  // network activity during an interval.
  void StoreCurrentDataUsageBucket(const DataUsageBucket& current_bucket);

  // Deletes all historical data usage from storage.
  void DeleteHistoricalDataUsage();

  // Deletes historical data usage from storage.
  void DeleteBrowsingHistory(const base::Time& start, const base::Time& end);

  // Returns whether |time1| and |time2| are in the same interval. Each hour is
  // divided into |kDataUsageBucketLengthMins| minute long intervals. Returns
  // true if either |time1| or |time2| has NULL time since an uninitialized
  // bucket can be assigned to any interval.
  static bool AreInSameInterval(const base::Time& time1,
                                const base::Time& time2);

  // Returns whether the bucket that was last updated at |bucket_last_updated|
  // overlaps in time with the interval [|start_interval|, |end_interval|].
  static bool BucketOverlapsInterval(const base::Time& bucket_last_updated,
                                     const base::Time& start_interval,
                                     const base::Time& end_interval);

 private:
  friend class DataUsageStoreTest;

  // Converts the given |bucket| into a string format for persistance to
  // |DataReductionProxyStore| and adds it to the map. The key is generated
  // based on |current_bucket_index_|.
  // |current_bucket_index_| will be incremented before generating the key if
  // |increment_current_index| is true.
  void GenerateKeyAndAddToMap(const DataUsageBucket& bucket,
                              std::map<std::string, std::string>* map,
                              bool increment_current_index);

  // Returns the offset between the bucket for |current| time and the last
  // bucket that was persisted to the store. Eg: Returns 0 if |current| is in
  // the last persisted bucket. Returns 1 if |current| belongs to the bucket
  // immediately after the last persisted bucket.
  int BucketOffsetFromLastSaved(const base::Time& current) const;

  // Computes the index of the bucket for the given |time| relative to
  // |current_bucket_index_| and |current_bucket_last_updated_|.
  // |current_bucket_last_updated_| belongs at |current_bucket_index_| and
  // bucket index is computed by going backwards or forwards from current index
  // by one position for every |kDataUsageBucketLengthInMinutes| minutes.
  int ComputeBucketIndex(const base::Time& time) const;

  // Loads the data usage bucket at the given index.
  DataStore::Status LoadBucketAtIndex(int index, DataUsageBucket* current);

  // The store to persist data usage information.
  DataStore* db_;

  // The index of the last bucket persisted in the |db_|. |DataUsageBucket| is
  // stored in the |db_| as a circular array. This index points to the array
  // position corresponding to the current bucket.
  int current_bucket_index_;

  // The time when the current bucket was last written to |db_|. This field is
  // used to determine if a DataUsageBucket to be saved belongs to the same
  // interval, or a more recent interval.
  base::Time current_bucket_last_updated_;

  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataUsageStore);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_USAGE_STORE_H_
