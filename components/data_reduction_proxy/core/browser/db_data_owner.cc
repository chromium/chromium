// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/db_data_owner.h"

#include <utility>

#include "base/check.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/core/browser/data_usage_store.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"

namespace data_reduction_proxy {

DBDataOwner::DBDataOwner(std::unique_ptr<DataStore> store)
    : store_(std::move(store)), data_usage_(new DataUsageStore(store_.get())) {
  sequence_checker_.DetachFromSequence();
}

DBDataOwner::~DBDataOwner() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
}

void DBDataOwner::InitializeOnDBThread() {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  store_->InitializeOnDBThread();
}

void DBDataOwner::LoadHistoricalDataUsage(
    std::vector<DataUsageBucket>* data_usage) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  data_usage_->LoadDataUsage(data_usage);
}

void DBDataOwner::LoadCurrentDataUsageBucket(DataUsageBucket* bucket) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  data_usage_->LoadCurrentDataUsageBucket(bucket);
}

void DBDataOwner::StoreCurrentDataUsageBucket(
    std::unique_ptr<DataUsageBucket> current) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  data_usage_->StoreCurrentDataUsageBucket(*current);
}

void DBDataOwner::DeleteHistoricalDataUsage() {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  data_usage_->DeleteHistoricalDataUsage();
}

void DBDataOwner::DeleteBrowsingHistory(const base::Time& start,
                                        const base::Time& end) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  data_usage_->DeleteBrowsingHistory(start, end);
}

base::WeakPtr<DBDataOwner> DBDataOwner::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace data_reduction_proxy
