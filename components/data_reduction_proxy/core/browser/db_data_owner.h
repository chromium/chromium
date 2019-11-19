// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DB_SERVICE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DB_SERVICE_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

namespace data_reduction_proxy {
class DataStore;
class DataUsageBucket;
class DataUsageStore;

// Callback for loading the historical data usage.
typedef base::Callback<void(std::unique_ptr<std::vector<DataUsageBucket>>)>
    HistoricalDataUsageCallback;

// Callback for loading data usage for the current bucket.
typedef base::Callback<void(std::unique_ptr<DataUsageBucket>)>
    LoadCurrentDataUsageCallback;

// Contains and initializes all Data Reduction Proxy objects that have a
// lifetime based on the DB task runner.
class DBDataOwner {
 public:
  explicit DBDataOwner(std::unique_ptr<DataStore> store);
  virtual ~DBDataOwner();

  // Initializes all the DB objects. Must be called on the DB task runner.
  void InitializeOnDBThread();

  // Loads data usage history stored in |DataStore|.
  void LoadHistoricalDataUsage(std::vector<DataUsageBucket>* data_usage);

  // Loads the last stored data usage bucket from |DataStore| into |bucket|.
  void LoadCurrentDataUsageBucket(DataUsageBucket* bucket);

  // Stores |current| to |DataStore|.
  void StoreCurrentDataUsageBucket(std::unique_ptr<DataUsageBucket> current);

  // Deletes all historical data usage from storage.
  void DeleteHistoricalDataUsage();

  // Deletes browsing history for the given data range from storage.
  void DeleteBrowsingHistory(const base::Time& start, const base::Time& end);

  // Returns a weak pointer to self for use on UI thread.
  base::WeakPtr<DBDataOwner> GetWeakPtr();

 private:
  std::unique_ptr<DataStore> store_;
  std::unique_ptr<DataUsageStore> data_usage_;
  base::SequenceChecker sequence_checker_;
  base::WeakPtrFactory<DBDataOwner> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DBDataOwner);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DB_SERVICE_H_
