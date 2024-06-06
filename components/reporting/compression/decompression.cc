// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/compression/decompression.h"

#include <string>

#include "components/reporting/proto/synced/record.pb.h"
#include "third_party/snappy/src/snappy.h"

namespace reporting::test {

std::string DecompressRecord(std::string record,
                             CompressionInformation compression_information) {
  // Decompress
  switch (compression_information.compression_algorithm()) {
    case CompressionInformation::COMPRESSION_NONE: {
      // Don't decompress, simply return serialized record
      return record;
    }
    case CompressionInformation::COMPRESSION_SNAPPY: {
      // Compression is enabled and crosses the threshold,
      std::string output;
      snappy::Uncompress(record.data(), record.size(), &output);
      return output;
    }
  }
}
}  // namespace reporting::test
