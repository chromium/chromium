// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/data_type_histogram.h"

#include "base/metrics/histogram_functions.h"

namespace {
const char kModelTypeMemoryHistogramPrefix[] = "Sync.ModelTypeMemoryKB.";
const char kModelTypeCountHistogramPrefix[] = "Sync.ModelTypeCount4.";
}  // namespace

void SyncRecordModelTypeMemoryHistogram(syncer::ModelType model_type,
                                        size_t bytes) {
  std::string type_string = ModelTypeToHistogramSuffix(model_type);
  std::string full_histogram_name =
      kModelTypeMemoryHistogramPrefix + type_string;
  base::UmaHistogramCounts1M(full_histogram_name, bytes / 1024);
}

void SyncRecordModelTypeCountHistogram(syncer::ModelType model_type,
                                       size_t count) {
  std::string type_string = ModelTypeToHistogramSuffix(model_type);
  std::string full_histogram_name =
      kModelTypeCountHistogramPrefix + type_string;
  base::UmaHistogramCounts1M(full_histogram_name, count);
}
