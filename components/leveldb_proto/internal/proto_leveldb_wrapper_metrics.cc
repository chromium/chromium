// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/proto_leveldb_wrapper_metrics.h"

#include "base/metrics/histogram.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace leveldb_proto {

// static
void ProtoLevelDBWrapperMetrics::RecordInit(const std::string& client,
                                            const leveldb::Status& status) {
  base::HistogramBase* init_status_histogram =
      base::LinearHistogram::FactoryGet(
          std::string("ProtoDB.InitStatus.") + client, 1,
          leveldb_env::LEVELDB_STATUS_MAX, leveldb_env::LEVELDB_STATUS_MAX + 1,
          base::Histogram::kUmaTargetedHistogramFlag);

  if (init_status_histogram)
    init_status_histogram->Add(leveldb_env::GetLevelDBStatusUMAValue(status));
}

// static
void ProtoLevelDBWrapperMetrics::RecordUpdate(const std::string& client,
                                              bool success,
                                              const leveldb::Status& status) {
  base::HistogramBase* update_success_histogram_ =
      base::BooleanHistogram::FactoryGet(
          std::string("ProtoDB.UpdateSuccess.") + client,
          base::Histogram::kUmaTargetedHistogramFlag);
  base::HistogramBase* update_error_histogram_ =
      base::LinearHistogram::FactoryGet(
          std::string("ProtoDB.UpdateErrorStatus.") + client, 1,
          leveldb_env::LEVELDB_STATUS_MAX, leveldb_env::LEVELDB_STATUS_MAX + 1,
          base::Histogram::kUmaTargetedHistogramFlag);

  if (update_success_histogram_)
    update_success_histogram_->Add(success);
  if (!success && update_error_histogram_)
    update_error_histogram_->Add(leveldb_env::GetLevelDBStatusUMAValue(status));
}

// static
void ProtoLevelDBWrapperMetrics::RecordGet(const std::string& client,
                                           bool success,
                                           bool found,
                                           const leveldb::Status& status) {
  base::HistogramBase* get_success_histogram =
      base::BooleanHistogram::FactoryGet(
          std::string("ProtoDB.GetSuccess.") + client,
          base::Histogram::kUmaTargetedHistogramFlag);
  base::HistogramBase* get_found_histogram = base::BooleanHistogram::FactoryGet(
      std::string("ProtoDB.GetFound.") + client,
      base::Histogram::kUmaTargetedHistogramFlag);
  base::HistogramBase* get_error_histogram = base::LinearHistogram::FactoryGet(
      std::string("ProtoDB.GetErrorStatus.") + client, 1,
      leveldb_env::LEVELDB_STATUS_MAX, leveldb_env::LEVELDB_STATUS_MAX + 1,
      base::Histogram::kUmaTargetedHistogramFlag);

  if (get_success_histogram)
    get_success_histogram->Add(success);
  if (get_found_histogram)
    get_found_histogram->Add(found);
  if (!success && get_error_histogram)
    get_error_histogram->Add(leveldb_env::GetLevelDBStatusUMAValue(status));
}

// static
void ProtoLevelDBWrapperMetrics::RecordLoadKeys(const std::string& client,
                                                bool success) {
  base::HistogramBase* load_keys_success_histogram =
      base::BooleanHistogram::FactoryGet(
          std::string("ProtoDB.LoadKeysSuccess.") + client,
          base::Histogram::kUmaTargetedHistogramFlag);

  if (load_keys_success_histogram)
    load_keys_success_histogram->Add(success);
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

// static
void ProtoLevelDBWrapperMetrics::RecordLoadKeysAndEntries(
    const std::string& client,
    bool success) {
  base::HistogramBase* load_keys_and_entries_success_histogram =
      base::BooleanHistogram::FactoryGet(
          std::string("ProtoDB.LoadKeysAndEntriesSuccess.") + client,
          base::Histogram::kUmaTargetedHistogramFlag);

  if (load_keys_and_entries_success_histogram)
    load_keys_and_entries_success_histogram->Add(success);
}

// static
void ProtoLevelDBWrapperMetrics::RecordDestroy(const std::string& client,
                                               bool success) {
  base::HistogramBase* destroy_success_histogram =
      base::BooleanHistogram::FactoryGet(
          std::string("ProtoDB.DestroySuccess.") + client,
          base::Histogram::kUmaTargetedHistogramFlag);

  if (destroy_success_histogram)
    destroy_success_histogram->Add(success);
}

}  // namespace leveldb_proto