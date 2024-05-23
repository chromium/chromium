// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/reporting/compression/decompression.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "third_party/snappy/src/snappy.h"

namespace reporting {

namespace {

std::string DecompressRecordSnappy(std::string record) {
  // Compression is enabled and crosses the threshold,
  std::string output;
  snappy::Uncompress(record.data(), record.size(), &output);
  return output;
}
}  // namespace

// static
scoped_refptr<Decompression> Decompression::Create() {
  return base::WrapRefCounted(new Decompression());
}

std::string Decompression::DecompressRecord(
    std::string record,
    CompressionInformation compression_information) {
  // Decompress
  switch (compression_information.compression_algorithm()) {
    case CompressionInformation::COMPRESSION_NONE: {
      // Don't decompress, simply return serialized record
      return record;
    }
    case CompressionInformation::COMPRESSION_SNAPPY: {
      return DecompressRecordSnappy(std::move(record));
    }
  }
}

Decompression::Decompression() = default;
Decompression::~Decompression() = default;

}  // namespace reporting
