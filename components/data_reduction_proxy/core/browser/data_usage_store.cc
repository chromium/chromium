// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Each |DataUsageBucket| corresponds to data usage for an interval of
// |kDataUsageBucketLengthMins| minutes. We store data usage for the past
// |kNumDataUsageBuckets| buckets. Buckets are maintained as a circular array
// with indexes from 0 to (kNumDataUsageBuckets - 1). To store the circular
// array in a key-value store, we convert each index to a unique key. The latest
// bucket persisted to DB overwrites the oldest.

#include "components/data_reduction_proxy/core/browser/data_usage_store.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"

namespace data_reduction_proxy {

namespace {

const char kCurrentBucketIndexKey[] = "current_bucket_index";
const char kBucketKeyPrefix[] = "data_usage_bucket:";

const int kMinutesInHour = 60;
const int kMinutesInDay = 24 * kMinutesInHour;

static_assert(data_reduction_proxy::kDataUsageBucketLengthInMinutes > 0,
              "Length of time should be positive");
static_assert(kMinutesInHour %
                      data_reduction_proxy::kDataUsageBucketLengthInMinutes ==
                  0,
              "kDataUsageBucketLengthMins must be a factor of kMinsInHour");

// Total number of buckets persisted to DB.
const int kNumDataUsageBuckets =
    kDataUsageHistoryNumDays * kMinutesInDay / kDataUsageBucketLengthInMinutes;

std::string DbKeyForBucketIndex(int index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, kNumDataUsageBuckets);

  return base::StringPrintf("%s%d", kBucketKeyPrefix, index);
}

base::Time BucketLowerBoundary(base::Time time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  exploded.minute -= exploded.minute % kDataUsageBucketLengthInMinutes;
  exploded.second = 0;
  exploded.millisecond = 0;

  base::Time out_time;
  bool conversion_success = base::Time::FromUTCExploded(exploded, &out_time);
  DCHECK(conversion_success);
  return out_time;
}

}  // namespace

DataUsageStore::DataUsageStore(DataStore* db)
    : db_(db), current_bucket_index_(-1) {
  sequence_checker_.DetachFromSequence();
}

DataUsageStore::~DataUsageStore() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
}

void DataUsageStore::LoadDataUsage(std::vector<DataUsageBucket>* data_usage) {
  SCOPED_UMA_HISTOGRAM_TIMER("DataReductionProxy.HistoricalDataUsageLoadTime");

  DCHECK(data_usage);

  DataUsageBucket empty_bucket;
  data_usage->clear();
  data_usage->resize(kNumDataUsageBuckets, empty_bucket);

  for (int i = 0; i < kNumDataUsageBuckets; ++i) {
    int circular_array_index =
        (i + current_bucket_index_ + 1) % kNumDataUsageBuckets;
    LoadBucketAtIndex(circular_array_index, &data_usage->at(i));
  }
}

void DataUsageStore::LoadCurrentDataUsageBucket(DataUsageBucket* current) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(current);

  std::string current_index_string;
  DataStore::Status index_read_status =
      db_->Get(kCurrentBucketIndexKey, &current_index_string);

  if (index_read_status != DataStore::Status::OK ||
      !base::StringToInt(current_index_string, &current_bucket_index_)) {
    current_bucket_index_ = 0;
  }

  DCHECK_GE(current_bucket_index_, 0);
  DCHECK_LT(current_bucket_index_, kNumDataUsageBuckets);

  DataStore::Status status = LoadBucketAtIndex(current_bucket_index_, current);
  bool bucket_read_ok = status == DataStore::Status::OK;
  current->set_had_read_error(!bucket_read_ok);
  if (bucket_read_ok) {
    current_bucket_last_updated_ =
        base::Time::FromInternalValue(current->last_updated_timestamp());
  }
}

void DataUsageStore::StoreCurrentDataUsageBucket(
    const DataUsageBucket& current) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(current_bucket_index_ >= 0 &&
         current_bucket_index_ < kNumDataUsageBuckets);

  // If current bucket does not have any information, we skip writing to DB.
  if (!current.has_last_updated_timestamp() ||
      (current.has_had_read_error() && current.had_read_error()))
    return;

  int prev_current_bucket_index = current_bucket_index_;
  base::Time prev_current_bucket_last_updated_ = current_bucket_last_updated_;
  // We might have skipped saving buckets because Chrome was not being used.
  // Write empty buckets to those slots to overwrite outdated information.
  base::Time last_updated =
      base::Time::FromInternalValue(current.last_updated_timestamp());
  std::map<std::string, std::string> buckets_to_save;
  int num_buckets_since_last_saved = BucketOffsetFromLastSaved(last_updated);
  for (int i = 0; i < num_buckets_since_last_saved - 1; ++i)
    GenerateKeyAndAddToMap(DataUsageBucket(), &buckets_to_save, true);

  GenerateKeyAndAddToMap(current, &buckets_to_save,
                         num_buckets_since_last_saved > 0);

  current_bucket_last_updated_ =
      base::Time::FromInternalValue(current.last_updated_timestamp());

  buckets_to_save.insert(std::pair<std::string, std::string>(
      kCurrentBucketIndexKey, base::NumberToString(current_bucket_index_)));

  DataStore::Status status = db_->Put(buckets_to_save);
  if (status != DataStore::Status::OK) {
    current_bucket_index_ = prev_current_bucket_index;
    current_bucket_last_updated_ = prev_current_bucket_last_updated_;
    LOG(WARNING) << "Failed to write data usage buckets to LevelDB" << status;
  }
}

void DataUsageStore::DeleteHistoricalDataUsage() {
  std::string current_index_string;
  DataStore::Status index_read_status =
      db_->Get(kCurrentBucketIndexKey, &current_index_string);

  // If the index doesn't exist, then no buckets have been written and the
  // data usage doesn't need to be deleted.
  if (index_read_status != DataStore::Status::OK)
    return;

  db_->RecreateDB();
}

void DataUsageStore::DeleteBrowsingHistory(const base::Time& start,
                                           const base::Time& end) {
  DCHECK_LE(start, end);
  if (current_bucket_last_updated_.is_null())
    return;

  base::Time begin_current_interval =
      BucketLowerBoundary(current_bucket_last_updated_);
  // Data usage is stored for the past |kDataUsageHistoryNumDays| days. Compute
  // the begin time for data usage.
  base::Time begin_history = begin_current_interval -
                             base::Days(kDataUsageHistoryNumDays) +
                             base::Minutes(kDataUsageBucketLengthInMinutes);

  // Nothing to do if there is no overlap between given interval and the
  // interval for which data usage history is maintained.
  if (begin_history > end || start > current_bucket_last_updated_)
    return;

  base::Time start_delete = start > begin_history ? start : begin_history;
  base::Time end_delete =
      end < current_bucket_last_updated_ ? end : current_bucket_last_updated_;

  int first_index_to_delete = ComputeBucketIndex(start_delete);
  int num_buckets_to_delete =
      1 +
      (BucketLowerBoundary(end_delete) - BucketLowerBoundary(start_delete))
              .InMinutes() /
          kDataUsageBucketLengthInMinutes;
  for (int i = 0; i < num_buckets_to_delete; ++i) {
    int index_to_delete = (first_index_to_delete + i) % kNumDataUsageBuckets;
    db_->Delete(DbKeyForBucketIndex(index_to_delete));
  }
  UMA_HISTOGRAM_COUNTS_10000(
      "DataReductionProxy.DeleteBrowsingHistory.NumBuckets",
      num_buckets_to_delete);
}

int DataUsageStore::ComputeBucketIndex(const base::Time& time) const {
  int offset = BucketOffsetFromLastSaved(time);

  int index = current_bucket_index_ + offset;
  if (index < 0) {
    index += kNumDataUsageBuckets;
  } else if (index >= kNumDataUsageBuckets) {
    index -= kNumDataUsageBuckets;
  }
  DCHECK_GE(index, 0);
  DCHECK_LT(index, kNumDataUsageBuckets);
  return index;
}

// static
bool DataUsageStore::AreInSameInterval(const base::Time& time1,
                                       const base::Time& time2) {
  if (time1.is_null() || time2.is_null())
    return true;

  return BucketLowerBoundary(time1) == BucketLowerBoundary(time2);
}

// static
bool DataUsageStore::BucketOverlapsInterval(
    const base::Time& bucket_last_updated,
    const base::Time& start_interval,
    const base::Time& end_interval) {
  DCHECK(!bucket_last_updated.is_null());
  DCHECK(!end_interval.is_null());
  DCHECK_LE(start_interval, end_interval);

  base::Time bucket_start = BucketLowerBoundary(bucket_last_updated);
  base::Time bucket_end =
      bucket_start + base::Minutes(kDataUsageBucketLengthInMinutes);
  DCHECK_LE(bucket_start, bucket_end);
  return bucket_end >= start_interval && end_interval >= bucket_start;
}

void DataUsageStore::GenerateKeyAndAddToMap(
    const DataUsageBucket& bucket,
    std::map<std::string, std::string>* map,
    bool increment_current_index) {
  if (increment_current_index) {
    current_bucket_index_++;
    DCHECK(current_bucket_index_ <= kNumDataUsageBuckets);
    if (current_bucket_index_ == kNumDataUsageBuckets)
      current_bucket_index_ = 0;
  }

  std::string bucket_key = DbKeyForBucketIndex(current_bucket_index_);

  std::string bucket_value;
  bool success = bucket.SerializeToString(&bucket_value);
  DCHECK(success);

  map->insert(std::pair<std::string, std::string>(std::move(bucket_key),
                                                  std::move(bucket_value)));
}

int DataUsageStore::BucketOffsetFromLastSaved(
    const base::Time& new_last_updated_timestamp) const {
  if (current_bucket_last_updated_.is_null())
    return 0;

  base::TimeDelta time_delta =
      BucketLowerBoundary(new_last_updated_timestamp) -
      BucketLowerBoundary(current_bucket_last_updated_);
  int offset_from_last_saved =
      (time_delta.InMinutes() / kDataUsageBucketLengthInMinutes);
  return offset_from_last_saved > 0
             ? std::min(offset_from_last_saved, kNumDataUsageBuckets)
             : std::max(offset_from_last_saved, -kNumDataUsageBuckets);
}

DataStore::Status DataUsageStore::LoadBucketAtIndex(int index,
                                                    DataUsageBucket* bucket) {
  DCHECK(index >= 0 && index < kNumDataUsageBuckets);

  std::string bucket_as_string;
  DataStore::Status bucket_read_status =
      db_->Get(DbKeyForBucketIndex(index), &bucket_as_string);
  if ((bucket_read_status != DataStore::Status::OK &&
       bucket_read_status != DataStore::NOT_FOUND)) {
    LOG(WARNING) << "Failed to read data usage bucket from LevelDB: "
                 << bucket_read_status;
  }
  if (bucket_read_status == DataStore::Status::OK) {
    bool parse_successful = bucket->ParseFromString(bucket_as_string);
    DCHECK(parse_successful);
  }
  return bucket_read_status;
}

}  // namespace data_reduction_proxy
