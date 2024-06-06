// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_COMPRESSION_DECOMPRESSION_H_
#define COMPONENTS_REPORTING_COMPRESSION_DECOMPRESSION_H_

#include <string>

#include "components/reporting/proto/synced/record.pb.h"

namespace reporting::test {

// DecompressRecord will decompress the provided |record| and respond
// with the callback. The compression_information provided will determine
// which compression algorithm is used. On success the returned std::string
// sink will contain a decompressed EncryptedWrappedRecord string. The sink
// string then can be further updated by the caller. std::string is used
// instead of std::string_view because ownership is taken of |record| through
// std::move(record).
[[nodiscard]] std::string DecompressRecord(
    std::string record,
    CompressionInformation compression_information);

}  // namespace reporting::test

#endif  // COMPONENTS_REPORTING_COMPRESSION_DECOMPRESSION_H_
