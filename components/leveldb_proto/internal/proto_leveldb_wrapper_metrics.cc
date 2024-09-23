// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/proto_leveldb_wrapper_metrics.h"

#include "base/metrics/histogram.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace leveldb_proto {

// static
void ProtoLevelDBWrapperMetrics::RecordUpdate(const std::string& client,
                                              bool success,
                                              const leveldb::Status& status) {
  base::HistogramBase* update_success_histogram_ =
      base::BooleanHistogram::FactoryGet(
          std::string("ProtoDB.UpdateSuccess.") + client,
          base::Histogram::kUmaTargetedHistogramFlag);

  if (update_success_histogram_)
    update_success_histogram_->Add(success);
}

// static
void ProtoLevelDBWrapperMetrics::RecordLoadEntries(const std::string& client,
                                                   bool success) {
  base::HistogramBase* load_entries_success_histogram =
      base::BooleanHistogram::FactoryGet(
          std::string("ProtoDB.LoadEntriesSuccess.") + client,
          base::Histogram::kUmaTargetedHistogramFlag);

  if (load_entries_success_histogram)
    load_entries_success_histogram->Add(success);
}

}  // namespace leveldb_proto